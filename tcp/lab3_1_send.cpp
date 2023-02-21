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
unsigned char pre_seq = 0;//序列号随机值
const int Max_pack_len = 1024;// 256 * 128 - 6;//每个数据报文大小为char[251],251个字节用来存储数据
const int head_len = 6;//每个数据报头大小为char[6],6个字节, 分别为校验和、flags、seq、ack、有效数据段总长度(2字节)
const int TIMEOUT = 1000;//毫秒
char buffer[1024 * 1024 * 256];//缓冲区1M
int buffer_len;//缓冲区已写长度

SOCKET c_socket;
SOCKADDR_IN server_addr, client_addr;


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
    //cout << ~(ret & 0x00FF);
    return ~(ret & 0x00FF);
}

void wave_hand() {//进行挥手，断开连接
    char send_buf[head_len], recv_buf[head_len];
    unsigned char seq = pre_seq++;
    pre_seq %= 128;
    int tot_fail = 0;//记录失败次数
    while (1) {
        //第一次挥手，发送fin
        memset(send_buf, 0, sizeof(send_buf));
        send_buf[1] = FLAG_FIN | FLAG_SEQ;
        send_buf[2] = seq;
        send_buf[0] = cksum(send_buf, head_len);//第一位记录校验和
        sendto(c_socket, send_buf, head_len, 0, (sockaddr*)&server_addr, sizeof(server_addr));//发送到服务器端
        std::printf("第%d次发送WAVE_1\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n",tot_fail, send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//日志
        int begintime = clock();
        int lentmp = sizeof(server_addr);
        int fail_send = 0;//标识是否发送失败
        memset(recv_buf, 0, sizeof(recv_buf));
        while (recvfrom(c_socket, recv_buf, head_len, 0, (sockaddr*)&server_addr, &lentmp) == SOCKET_ERROR)
            if (clock() - begintime > TIMEOUT) {//接收已经超时
                std::printf("超时重新发送WAVE_1\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//日志
                fail_send = 1;//发送失败
                //tot_fail++;//失败次数加一
                break;
            }
        //第二次挥手，接收ack
        if (fail_send == 0 && cksum(recv_buf, head_len) == 0 && recv_buf[1] == (FLAG_ACK | FLAG_FIN) && recv_buf[3] == send_buf[2] + 1) {
            std::printf("接收WAVE_2\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//日志
            break;
        }
        tot_fail++;
    }
}


bool send_package(char* message, int lent, unsigned char seq) {//分片发包
    if (lent > Max_pack_len) {//单片长度过大
        return false;
    }
    char send_buf[Max_pack_len + head_len];

    memset(send_buf, 0, Max_pack_len + head_len);
    send_buf[1] = FLAG_SEQ;
    send_buf[2] = seq;//序列号
    send_buf[4] = lent/256;//数据段大小
    send_buf[5] = lent % 256;//数据段大小
    for (int i = head_len; i < lent + head_len; i++)
        send_buf[i] = message[i - head_len];//存入内容
    send_buf[0] = cksum(send_buf, Max_pack_len + head_len);//校验和
    sendto(c_socket, send_buf, Max_pack_len + head_len, 0, (sockaddr*)&server_addr, sizeof(server_addr));//信息发送
    std::printf("发送数据报\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], (unsigned char)send_buf[4]*256 + (unsigned char)send_buf[5]);//日志
    return true;
}

void shake_hand() {//握手，建立连接
    char send_buf[head_len], recv_buf[head_len];
    unsigned char seq = pre_seq++;
    pre_seq %= 128;

    while (1) {
        //第一次握手，发送syn,主动建连
        memset(send_buf, 0, sizeof(send_buf));
        send_buf[1] = FLAG_SYN | FLAG_SEQ;
        send_buf[2] = seq;
        send_buf[0] = cksum(send_buf, head_len);
        sendto(c_socket, send_buf, sizeof(send_buf), 0, (sockaddr*)&server_addr, sizeof(server_addr));
        std::printf("发送SHAKE_1\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0],sizeof(send_buf));//日志

        //第二次握手，接收syn ack seq并校验
        int begintime = clock(); 
        memset(recv_buf, 0, sizeof(recv_buf));
        int lentmp = sizeof(server_addr);
        int fail_send = 0;
        while (recvfrom(c_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&server_addr, &lentmp) == SOCKET_ERROR){
            if (clock() - begintime > TIMEOUT) {
                //已超时,重新发送syn
                fail_send = 1;
                break;
            }
        }
        if (fail_send == 0 && cksum(recv_buf, head_len) == 0 && recv_buf[1]&(FLAG_ACK|FLAG_SEQ|FLAG_SYN) && recv_buf[3] == seq +1) {
            {
                std::printf("接收SHAKE_2\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//日志
                
                //第三次握手，发送ack
                memset(send_buf, 0, sizeof(send_buf));
                send_buf[1] = FLAG_ACK | FLAG_SEQ;
                send_buf[2] = seq+1;
                send_buf[3] = recv_buf[2] + 1;
                send_buf[0] = cksum(send_buf, head_len);
                int lentmp = sizeof(server_addr);
                sendto(c_socket, send_buf, sizeof(send_buf), 0, (sockaddr*)&server_addr, lentmp);
                std::printf("发送SHAKE_3\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//日志
                break;
            }
        }
    }
}


void send_message(char* message, int message_length) {
    int package_num = message_length / Max_pack_len + (message_length % Max_pack_len != 0);//确定发包数，是否加最后一个包
    
    std::printf("共需要发送%d个数据包\n", package_num);
    for (int i = 0; i < package_num; i++) {
        unsigned char seq = pre_seq++;
        pre_seq %= 128;
        std::printf("正在发送第%d个数据包\n", i);
        //发送第i个包
        while (1) {
            //发送
            int lent = (i == (package_num - 1)) ? (message_length % Max_pack_len) : Max_pack_len;//若是最后一个包长度需额外计算
            send_package(message + i * Max_pack_len, lent, seq);
            int timer_begin = clock();//启动定时器

            //接收ack == seq
            char recv_buf[head_len];
            memset(recv_buf, 0, sizeof(recv_buf));
            int lentmp = sizeof(server_addr);
            bool timeout = false;
            while (recvfrom(c_socket, recv_buf, head_len, 0, (sockaddr*)&server_addr, &lentmp) == SOCKET_ERROR)
                if (clock() - timer_begin > TIMEOUT) {
                    //超时，返回重传
                    timeout = true;
                    break;
                }
            if (!timeout && cksum(recv_buf, head_len) == 0 && (recv_buf[1]&FLAG_ACK) && recv_buf[3] == seq) {
                //成功接收，可以发送下一个包
                std::printf("接收ACK\tflag:%x\tseq:%d\tack:%d\t校验和:%d\t报文长度:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//日志
                break;
            }
            //未成功接收，返回重传
        }
    }
}


int main() {
    WSADATA wsadata;
    int error = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (error) {
        std::printf("init error");
        return 0;
    }
    string sendip = "127.0.0.2";
    if (inet_addr(sendip.c_str()) == INADDR_NONE) {
        std::printf("ip地址不合法!\n");
    }

    int port = 1111;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(sendip.c_str());

    c_socket = socket(AF_INET, SOCK_DGRAM, 0);

    //设置非阻塞
    int time_out = 1;//1ms超时
    setsockopt(c_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));

    if (c_socket == INVALID_SOCKET) {
        std::printf("creat udp c_socket error");
        return 0;
    }



    string filename;
    while (1) {
        //std::printf("请输入要发送的文件名：");
        //cin >> filename;
        filename = "1.jpg";
        ifstream fin(filename.c_str(), ifstream::binary);
        if (!fin) {
            continue;
            std::printf("文件未找到!\n");
        }
        buffer_len = 0;
        unsigned char t = fin.get();
        while (fin) {
            buffer[buffer_len++] = t;
            t = fin.get();
        }
        fin.close();
        break;
    }
    std::printf("连接建立中...\n");
    //1.三次握手建连
    shake_hand();

    std::printf("连接建立完成。 \n正在发送信息...\n");
    long long head, tail, freq, time;
    //send_message((char*)(filename.c_str()), filename.length());
    //std::printf("%s文件名发送完毕，正在发送文件内容...\n", filename.c_str());

    //2. 停等机制发送数据
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    send_message(buffer, buffer_len);
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);
    time = (tail - head) * 1000.0 / freq;

    std::printf("发送的文件bytes:%d\n", buffer_len);
    std::cout << "传输时间为：" << time << "ms" << endl;
    std::cout << "平均吞吐率为：" << buffer_len * 8 / 1000 / ((double)time / 1000) << "     kbps" << endl;
    std::printf("%s文件内容发送完毕。\n", filename.c_str());

    //3. 2次挥手断开连接
    std::printf("开始断开连接...\n");
    wave_hand();
    std::printf("连接已断开。");

    closesocket(c_socket);
    WSACleanup();
    cout << "--------------------------------------\n\n\n";//日志分隔符
    getchar();
    return 0;
}