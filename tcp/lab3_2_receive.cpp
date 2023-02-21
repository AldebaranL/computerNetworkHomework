// UDP.send.cpp : ���ļ����� "main" ����������ִ�н��ڴ˴���ʼ��������
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
char pre_seq = 50;//���к����ֵ
const int Max_pack_len = 1024;// 256 * 128 - 6;//ÿ�����ݱ��Ĵ�СΪchar[251],251���ֽ������洢����
const int head_len = 6;//ÿ�����ݱ�ͷ��СΪchar[4],4���ֽ�, �ֱ�ΪУ��͡�flags��seq��ack����Ч���ݶ��ܳ���
const int TIMEOUT = 1000;//����
char buffer[1024 * 1024 * 256];//������256M
int buffer_len;//��������д����
const int MAX_CHAR = 128;
const int N = 10;//GBN���ڴ�С
SOCKADDR_IN server_addr, client_addr;
SOCKET s_socket; //ѡ��udpЭ��

unsigned char cksum(char* flag, int len) {//����У���
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
void wait_shake_hand() {//�ȴ���������
    char send_buf[head_len], recv_buf[head_len];
    unsigned char seq = pre_seq++;
    pre_seq %= MAX_CHAR;

    while (1) {
        //��һ�����֣�����syn,seq,��������
        bool connect_success = 0;
        memset(recv_buf, 0, sizeof(recv_buf));
        int lentmp = sizeof(client_addr);
        while (recvfrom(s_socket, recv_buf, head_len, 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        if (cksum(recv_buf, head_len) != 0 || (recv_buf[1] & FLAG_SYN) == 0)//������֮�����У��Ͳ�����0���߽��յ��Ĳ���syn
            continue;
        std::printf("����SHAKE_1\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//��־

        while (1) {
            //�ڶ������֣�����syn��ack��seq
            memset(send_buf, 0, sizeof(send_buf));
            send_buf[1] = FLAG_ACK | FLAG_SEQ | FLAG_SYN;
            send_buf[2] = seq;
            send_buf[3] = recv_buf[2] + 1;
            send_buf[0] = cksum(send_buf, head_len);
            sendto(s_socket, send_buf, head_len, 0, (sockaddr*)&client_addr, sizeof(client_addr));//���͵ڶ�������
            std::printf("����SHAKE_2\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־

           //���������֣�����ack
            memset(recv_buf, 0, sizeof(recv_buf));
            while (recvfrom(s_socket, recv_buf, head_len, 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
            if (cksum(recv_buf, head_len) == 0 && recv_buf[1] & (FLAG_ACK | FLAG_SEQ) && recv_buf[2] == send_buf[3] && recv_buf[3] == send_buf[2] + 1) {
                connect_success = 1;
                std::printf("����SHAKE_3\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//��־
                break;
            }
        }
        if (connect_success)
            break;
    }
}

void send_ack(unsigned char seq, bool fin = false) {
	//����ack
    char send_buf[head_len];
    memset(send_buf, 0, sizeof(send_buf));
    if (fin) {//���ͻ���ack
        send_buf[1] = FLAG_ACK | FLAG_FIN;
        send_buf[3] = seq;
    }
    else {//һ��ack
        send_buf[1] = FLAG_ACK;
        send_buf[3] = seq;
    }
    send_buf[0] = cksum(send_buf, head_len);
    sendto(s_socket, send_buf, head_len, 0, (sockaddr*)&client_addr, sizeof(client_addr));
    std::printf("����ACK\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־
}
void recv_message(char* message, int& len_recv) {//δ�ӻ������Ľ���
    //�������ݣ�д��message���䳤��д��len_recv
    char recv_buf[Max_pack_len + head_len];
    int lentmp = sizeof(client_addr);
    int last_ack = -1;//��¼���к�
    len_recv = 0;//��¼�ļ��ܳ��� 
    int pack_i = 0;
    while (1) {
        //�������ݰ�
        memset(recv_buf, 0, sizeof(recv_buf));
        while (recvfrom(s_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        int lenth = (unsigned char)recv_buf[4] * 256 + (unsigned char)recv_buf[5];
        std::printf("�������ݰ�%d\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n",pack_i++, recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], lenth);//��־
        if (cksum(recv_buf, head_len + lenth) == 0 && recv_buf[1] & FLAG_FIN) {
            //�յ�����fin���󣬽�������
            send_ack(recv_buf[2], true);//����fin��ack
            closesocket(s_socket);
            WSACleanup();
            break;
        }
        if (cksum(recv_buf, head_len + lenth) == 0 && (recv_buf[1]&FLAG_SEQ) && ((last_ack +1)%MAX_CHAR == recv_buf[2]|| last_ack==-1)) {
            //����ȷ�������ݰ�
            len_recv += lenth;
            for (int i = 0; i < lenth; i++) {
                message[len_recv - lenth + i] = recv_buf[head_len + i];
            }
            last_ack = recv_buf[2];

            //����ack
            send_ack(recv_buf[2]);
        }
        else {
            //������������ݰ�
            send_ack(last_ack);//�����ϸ�ack
        }

    }
}
void recv_message_buf(char* message, int& len_recv) {//���������ڴ�СΪN���������ݣ�д��message���䳤��д��len_recv
    char recv_window[N * Max_pack_len];//������
    bool recv_flag[N];//��������־λ
    memset(recv_window, 0, sizeof(recv_window));
    memset(recv_flag, 0, sizeof(recv_flag));

    int first_seq = -1;//���������׸����ݰ���seq
    char recv_buf[Max_pack_len + head_len];
    int lentmp = sizeof(client_addr);
    len_recv = 0;//��¼�ļ��ܳ��� 
    while (1) {
        //�������ݰ�
        memset(recv_buf, 0, sizeof(recv_buf));
        while (recvfrom(s_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        int pack_lenth = (unsigned char)recv_buf[4] * 256 + (unsigned char)recv_buf[5];
        std::printf("�������ݰ�\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], pack_lenth);//��־
        
        if (recv_buf[1] & FLAG_FIN) {
            //�յ�����fin���󣬽�������
            send_ack(recv_buf[2], true);//����fin��ack
            closesocket(s_socket);
            WSACleanup();
            break;
        }
        
        //move_num���µ��İ������buf��ʼ����ƫ�Ƹ���
        int move_num = (first_seq > MAX_CHAR - N && recv_buf[2] < N) ?
            recv_buf[2] + MAX_CHAR - first_seq:
            recv_buf[2] - first_seq;
        if (cksum(recv_buf, head_len + pack_lenth) == 0 && (recv_buf[1] & FLAG_SEQ) &&
            ((first_seq == -1) || (move_num >= 0 && move_num < N && !recv_flag[move_num]))) {
            //����ȷ�������ݰ�
            if (first_seq == -1) {
                //��ʼ��
                first_seq = recv_buf[2];
                move_num = 0;
            }
            //���»���������λ��
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
            //д���ļ�
            for (int i = 0; i < write_num * Max_pack_len; i++) {
                message[len_recv + i] = recv_window[i];
            }
            len_recv += write_num * Max_pack_len;
            //������ǰ��
            for (int i = 0; i < (N - write_num) * Max_pack_len; i++) {
                recv_window[i] = recv_window[i + write_num * Max_pack_len];
            }
            std::printf("������ʼλ��:%d\n", first_seq);
        }
        //�����ۼ�ȷ�ϵ�ack
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

    server_addr.sin_family = AF_INET; //ʹ��ipv4
    server_addr.sin_port = htons(Port); //����˶˿�
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    string sendip = "127.0.0.2";
    if (inet_addr(sendip.c_str()) == INADDR_NONE) {
        std::printf("ip��ַ���Ϸ�!\n");
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1111);
    server_addr.sin_addr.s_addr = inet_addr(sendip.c_str());


    s_socket = socket(AF_INET, SOCK_DGRAM, 0);
    //���÷�����
    int time_out = 1;//1ms��ʱ
    //����û��������ݷ��ͳ�ȥ���ڹر�socket��������1ms
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

    //1. �ȴ����ֽ���
    std::printf("�ȴ�����...\n");
    wait_shake_hand();
    std::printf("�û��ѽ��롣\n���ڽ�������...\n");

    string file_name("file.jpg");//���ô����ļ�·��

    //2. ��������ֱ���յ���������
    long long head, tail, freq, time;
    std::printf("��ʼ�����ļ�����...\n");
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    recv_message_buf(buffer, buffer_len);
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);
    time = (tail - head) * 1000.0 / freq;
    std::printf("�ļ����ݽ��ճɹ���\n");

    ofstream fout(file_name.c_str(), ofstream::binary);
    for (int i = 0; i < buffer_len; i++)
        fout << buffer[i];
    fout.close();
    std::printf("���յ����ļ�bytes:%d\n", buffer_len);

    std::cout << "����ʱ��Ϊ��" << time << "ms" << endl;
    std::cout << "ƽ��������Ϊ��" << buffer_len * 8 / 1000 / ((double)time / 1000) << "kbps" << endl;
    std::cout << "--------------------------------------\n\n\n";//��־�ָ���
    //getchar();
    return 0;
}

