// UDP.send.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#pragma comment(lib, "Ws2_32.lib")
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable:4996)

#include <iostream>
#include <winsock2.h>
#include <stdio.h>
#include <fstream>
#include<time.h>

using namespace std;

const unsigned char FLAG_ACK = 0x01;
const unsigned char FLAG_SEQ = 0x02;
const unsigned char FLAG_FIN = 0x04;
const unsigned char FLAG_SYN = 0x08;
char pre_seq = 50;//序列号随机值
const int Max_pack_len = 1024;// 256 * 128 - 6;//每个数据报文大小为char[251],251个字节用来存储数据
const int head_len = 6;//每个数据报头大小为char[4],4个字节, 分别为校验和、flags、seq、ack、有效数据段总长度
const int TIMEOUT = 1000;//毫秒
char buffer[1024 * 1024 * 256];//缓冲区256M
int buffer_len;//缓冲区已写长度
const int MAX_CHAR = 128;
const int N = 10;//GBN窗口大小
SOCKADDR_IN server_addr, client_addr;
SOCKET s_socket; //选择udp协议

unsigned char cksum(char* flag, int len) {//计算校验和
    if (len == 0) {
        return ~(0);
    }
    unsigned int ret = 0;
    int i = 0;
    while (len--) {
        ret += (unsigned char)flag[i++];
        if (ret & 0xFF00) {
            ret &= 0x00FF;
            ret++;
        }
    }
    return ~(ret & 0x00FF);
}
void wait_shake_hand() {//等待建立连接
    char send_buf[head_len], recv_buf[head_len];
    unsigned char seq = pre_seq++;
    pre_seq %= MAX_CHAR;

    while (1) {
        //第一次握手，接收syn,seq,被动建连
        bool connect_success = 0;
        memset(recv_buf, 0, sizeof(recv_buf));
        int lentmp = sizeof(client_addr);
        while (recvfrom(s_socket, recv_buf, head_len, 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        if (cksum(recv_buf, head_len) != 0 || (recv_buf[1] & FLAG_SYN) == 0)//接受了之后算出校验和不等于0或者接收到的不是syn
            continue;
        std::printf("接收SHAKE_1\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//日志

        while (1) {
            //第二次握手，发送syn，ack，seq
            memset(send_buf, 0, sizeof(send_buf));
            send_buf[1] = FLAG_ACK | FLAG_SEQ | FLAG_SYN;
            send_buf[2] = seq;
            send_buf[3] = recv_buf[2] + 1;
            send_buf[0] = cksum(send_buf, head_len);
            sendto(s_socket, send_buf, head_len, 0, (sockaddr*)&client_addr, sizeof(client_addr));//发送第二次握手
            std::printf("发送SHAKE_2\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//日志

           //第三次握手，接收ack
            memset(recv_buf, 0, sizeof(recv_buf));
            while (recvfrom(s_socket, recv_buf, head_len, 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
            if (cksum(recv_buf, head_len) == 0 && recv_buf[1] & (FLAG_ACK | FLAG_SEQ) && recv_buf[2] == send_buf[3] && recv_buf[3] == send_buf[2] + 1) {
                connect_success = 1;
                std::printf("接收SHAKE_3\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//日志
                break;
            }
        }
        if (connect_success)
            break;
    }
}

void send_ack(unsigned char seq, bool fin = false) {
	//发送ack
    char send_buf[head_len];
    memset(send_buf, 0, sizeof(send_buf));
    if (fin) {//发送挥手ack
        send_buf[1] = FLAG_ACK | FLAG_FIN;
        send_buf[3] = seq;
    }
    else {//一般ack
        send_buf[1] = FLAG_ACK;
        send_buf[3] = seq;
    }
    send_buf[0] = cksum(send_buf, head_len);
    sendto(s_socket, send_buf, head_len, 0, (sockaddr*)&client_addr, sizeof(client_addr));
    std::printf("发送ACK\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//日志
}
void recv_message(char* message, int& len_recv) {//未加缓冲区的接收
    //接收数据，写入message，其长度写入len_recv
    char recv_buf[Max_pack_len + head_len];
    int lentmp = sizeof(client_addr);
    int last_ack = -1;//记录序列号
    len_recv = 0;//记录文件总长度 
    int pack_i = 0;
    while (1) {
        //接收数据包
        memset(recv_buf, 0, sizeof(recv_buf));
        while (recvfrom(s_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        int lenth = (unsigned char)recv_buf[4] * 256 + (unsigned char)recv_buf[5];
        std::printf("接收数据包%d\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n",pack_i++, recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], lenth);//日志
        if (cksum(recv_buf, head_len + lenth) == 0 && recv_buf[1] & FLAG_FIN) {
            //收到挥手fin请求，结束连接
            send_ack(recv_buf[2], true);//发送fin与ack
            closesocket(s_socket);
            WSACleanup();
            break;
        }
        if (cksum(recv_buf, head_len + lenth) == 0 && (recv_buf[1]&FLAG_SEQ) && ((last_ack +1)%MAX_CHAR == recv_buf[2]|| last_ack==-1)) {
            //若正确接收数据包
            len_recv += lenth;
            for (int i = 0; i < lenth; i++) {
                message[len_recv - lenth + i] = recv_buf[head_len + i];
            }
            last_ack = recv_buf[2];

            //发送ack
            send_ack(recv_buf[2]);
        }
        else {
            //若错误接收数据包
            send_ack(last_ack);//发送上个ack
        }

    }
}
void recv_message_buf(char* message, int& len_recv) {//缓冲区窗口大小为N，接收数据，写入message，其长度写入len_recv
    char recv_window[N * Max_pack_len];//缓冲区
    bool recv_flag[N];//缓冲区标志位
    memset(recv_window, 0, sizeof(recv_window));
    memset(recv_flag, 0, sizeof(recv_flag));

    int first_seq = -1;//缓冲区中首个数据包的seq
    char recv_buf[Max_pack_len + head_len];
    int lentmp = sizeof(client_addr);
    len_recv = 0;//记录文件总长度 
    while (1) {
        //接收数据包
        memset(recv_buf, 0, sizeof(recv_buf));
        while (recvfrom(s_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        int pack_lenth = (unsigned char)recv_buf[4] * 256 + (unsigned char)recv_buf[5];
        std::printf("接收数据包\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], pack_lenth);//日志
        
        if (recv_buf[1] & FLAG_FIN) {
            //收到挥手fin请求，结束连接
            send_ack(recv_buf[2], true);//发送fin与ack
            closesocket(s_socket);
            WSACleanup();
            break;
        }
        
        //move_num是新到的包相对于buf初始包的偏移个数
        int move_num = (first_seq > MAX_CHAR - N && recv_buf[2] < N) ?
            recv_buf[2] + MAX_CHAR - first_seq:
            recv_buf[2] - first_seq;
        if (cksum(recv_buf, head_len + pack_lenth) == 0 && (recv_buf[1] & FLAG_SEQ) &&
            ((first_seq == -1) || (move_num >= 0 && move_num < N && !recv_flag[move_num]))) {
            //若正确接收数据包
            if (first_seq == -1) {
                //初始化
                first_seq = recv_buf[2];
                move_num = 0;
            }
            //更新缓冲区窗口位置
            recv_flag[move_num] = true;
            for (int i = 0; i < pack_lenth; i++) {
                recv_window[move_num * Max_pack_len + i] = recv_buf[head_len + i];
            }
            int write_num = 0;
            while (write_num < N && recv_flag[write_num]) {
                recv_flag[write_num] = false;
                write_num++;
            }
            first_seq += write_num;
            first_seq %= MAX_CHAR;
            //写入文件
            for (int i = 0; i < write_num * Max_pack_len; i++) {
                message[len_recv + i] = recv_window[i];
            }
            len_recv += write_num * Max_pack_len;
            //缓冲区前移
            for (int i = 0; i < (N - write_num) * Max_pack_len; i++) {
                recv_window[i] = recv_window[i + write_num * Max_pack_len];
            }
            std::printf("窗口起始位置:%d\n", first_seq);
        }
        //发送累计确认的ack
        send_ack((first_seq == 0) ? MAX_CHAR - 1 : first_seq - 1);      
    }  
}

int main() {

    WSADATA wsadata;
    int nError = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (nError) {
        std::printf("start error\n");
        return 0;
    }

    int Port = 11452;

    server_addr.sin_family = AF_INET; //使用ipv4
    server_addr.sin_port = htons(Port); //服务端端口
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    string sendip = "127.0.0.2";
    if (inet_addr(sendip.c_str()) == INADDR_NONE) {
        std::printf("ip地址不合法!\n");
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1111);
    server_addr.sin_addr.s_addr = inet_addr(sendip.c_str());


    s_socket = socket(AF_INET, SOCK_DGRAM, 0);
    //设置非阻塞
    int time_out = 1;//1ms超时
    //即让没发完的数据发送出去后在关闭socket，允许逗留1ms
    setsockopt(s_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));

    if (s_socket == INVALID_SOCKET) {
        std::printf("create fail");
        closesocket(s_socket);
        return 0;
    }
    int retVAL = bind(s_socket, (sockaddr*)(&server_addr), sizeof(server_addr));
    if (retVAL == SOCKET_ERROR) {
        std::printf("bind fail");
        closesocket(s_socket);
        WSACleanup();
        return 0;
    }

    //1. 等待握手接入
    std::printf("等待接入...\n");
    wait_shake_hand();
    std::printf("用户已接入。\n正在接收数据...\n");

    string file_name("file.jpg");//设置传输文件路径

    //2. 接收数据直至收到挥手请求
    long long head, tail, freq, time;
    std::printf("开始接收文件数据...\n");
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    recv_message_buf(buffer, buffer_len);
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);
    time = (tail - head) * 1000.0 / freq;
    std::printf("文件数据接收成功。\n");

    ofstream fout(file_name.c_str(), ofstream::binary);
    for (int i = 0; i < buffer_len; i++)
        fout << buffer[i];
    fout.close();
    std::printf("接收到的文件bytes:%d\n", buffer_len);

    std::cout << "传输时间为：" << time << "ms" << endl;
    std::cout << "平均吞吐率为：" << buffer_len * 8 / 1000 / ((double)time / 1000) << "kbps" << endl;
    std::cout << "--------------------------------------\n\n\n";//日志分隔符
    //getchar();
    return 0;
}

