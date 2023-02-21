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
    pre_seq %= 128;

    while (1) {
        //��һ�����֣�����syn,seq,��������
        bool connect_success = 0;
        memset(recv_buf, 0, sizeof(recv_buf));
        int lentmp = sizeof(client_addr);
        while (recvfrom(s_socket, recv_buf, head_len, 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        if (cksum(recv_buf, head_len) != 0 || (recv_buf[1]&FLAG_SYN) == 0)//������֮�����У��Ͳ�����0���߽��յ��Ĳ���syn
            continue;
        printf("����SHAKE_1\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//��־
        
        while (1) {
            //�ڶ������֣�����syn��ack��seq
            memset(send_buf, 0, sizeof(send_buf));
            send_buf[1] = FLAG_ACK | FLAG_SEQ | FLAG_SYN;
            send_buf[2] = seq;
            send_buf[3] = recv_buf[2] + 1;
            send_buf[0] = cksum(send_buf, head_len);
            sendto(s_socket, send_buf, head_len, 0, (sockaddr*)&client_addr, sizeof(client_addr));//���͵ڶ�������
            printf("����SHAKE_2\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־
            
           //���������֣�����ack
            memset(recv_buf, 0, sizeof(recv_buf));
            while (recvfrom(s_socket, recv_buf, head_len, 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
            if (cksum(recv_buf, head_len) == 0 && recv_buf[1] & (FLAG_ACK | FLAG_SEQ) && recv_buf[2] == send_buf[3] && recv_buf[3] == send_buf[2] + 1) {
                connect_success = 1;
                printf("����SHAKE_3\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//��־
                break;
            }
        }
        if (connect_success)
            break;
    }
}

void send_ack(unsigned char seq,bool fin=false) {
    char send_buf[head_len];
    memset(send_buf, 0, sizeof(send_buf));
    if (fin) {//���ͻ���ack
        send_buf[1] = FLAG_ACK | FLAG_FIN;
        send_buf[3] = seq+1;
    }
    else {//һ��ack
        send_buf[1] = FLAG_ACK;
        send_buf[3] = seq;
    }
    send_buf[0] = cksum(send_buf, head_len);
    sendto(s_socket, send_buf, head_len, 0, (sockaddr*)&client_addr, sizeof(client_addr));
    printf("����ACK\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־
}
void recv_message(char* message, int& len_recv) {
    //�������ݣ�д��message���䳤��д��len_recv
    char recv_buf[Max_pack_len + head_len];
    int lentmp = sizeof(client_addr);
    unsigned char last_order = -1;//��¼���к�
    len_recv = 0;//��¼�ļ����� 
    while (1) {
        //�������ݰ�
        memset(recv_buf, 0, sizeof(recv_buf));
        while (recvfrom(s_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&client_addr, &lentmp) == SOCKET_ERROR);
        int lenth = (unsigned char)recv_buf[4] * 256 + (unsigned char)recv_buf[5];
        printf("�������ݰ�\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], lenth);//��־
        if (cksum(recv_buf, head_len + lenth) == 0 && last_order != recv_buf[2]) {
            //����ȷ�������ݰ�
            len_recv += lenth;
            for (int i = 0; i < lenth; i++) {
                message[len_recv - lenth + i] = recv_buf[head_len + i];
            }
            last_order = recv_buf[2];
            //����ack
            send_ack(recv_buf[2]);
            if (recv_buf[1] & FLAG_FIN) {
                //�յ�����fin���󣬽�������
                send_ack(recv_buf[2],true);//����fin��ack
                closesocket(s_socket);
                WSACleanup();
                break;
            }
        }
        else {
            //������������ݰ�
            send_ack(last_order);//�����ϸ�ack
        }
        
    }
}

int main() {

    WSADATA wsadata;
    int nError = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (nError) {
        printf("start error\n");
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
        printf("create fail");
        closesocket(s_socket);
        return 0;
    }
    int retVAL = bind(s_socket, (sockaddr*)(&server_addr), sizeof(server_addr));
    if (retVAL == SOCKET_ERROR) {
        printf("bind fail");
        closesocket(s_socket);
        WSACleanup();
        return 0;
    }

    //1. �ȴ����ֽ���
    printf("�ȴ�����...\n");
    wait_shake_hand();
    printf("�û��ѽ��롣\n���ڽ�������...\n");

    string file_name("file.jpg");//���ô����ļ�·��
    
    //2. ��������ֱ���յ���������
    long long head, tail, freq, time;
    printf("��ʼ�����ļ�����...\n");
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    recv_message(buffer, buffer_len);
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);
    time = (tail - head) * 1000.0 / freq;
    printf("�ļ����ݽ��ճɹ���\n");

    ofstream fout(file_name.c_str(), ofstream::binary);
    for (int i = 0; i < buffer_len; i++)
        fout << buffer[i];
    fout.close();
    printf("���յ����ļ�bytes:%d\n", buffer_len);

    cout << "����ʱ��Ϊ��" << time << "ms" << endl;
    cout << "ƽ��������Ϊ��" << buffer_len * 8 / 1000 / ((double)time / 1000) << "kbps" << endl;
    cout << "--------------------------------------\n\n\n";//��־�ָ���
    //getchar();
    return 0;
}

