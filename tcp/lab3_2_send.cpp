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
int pre_seq = 0;//���к����ֵ
const int Max_pack_len = 1024;// 256 * 128 - 6;//ÿ�����ݱ��Ĵ�СΪchar[251],251���ֽ������洢����
const int head_len = 6;//ÿ�����ݱ�ͷ��СΪchar[6],6���ֽ�, �ֱ�ΪУ��͡�flags��seq��ack����Ч���ݶ��ܳ���(2�ֽ�)
const int TIMEOUT = 1000;//����
char buffer[1024 * 1024 * 256];//������1M
int buffer_len;//��������д����

const int N = 10;//GBN���ڴ�С
const int MAX_CHAR = 128;
SOCKET c_socket;
SOCKADDR_IN server_addr, client_addr;


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
    //cout << ~(ret & 0x00FF);
    return ~(ret & 0x00FF);
}

void wave_hand() {//���л��֣��Ͽ�����
    char send_buf[head_len], recv_buf[head_len];
    unsigned char seq = pre_seq++;
    pre_seq %= MAX_CHAR;
    int tot_fail = 0;//��¼ʧ�ܴ���
    while (1) {
        //��һ�λ��֣�����fin
        memset(send_buf, 0, sizeof(send_buf));
        send_buf[1] = FLAG_FIN | FLAG_SEQ;
        send_buf[2] = seq;
        send_buf[0] = cksum(send_buf, head_len);//��һλ��¼У���
        sendto(c_socket, send_buf, head_len, 0, (sockaddr*)&server_addr, sizeof(server_addr));//���͵���������
        std::printf("��%d�η���WAVE_1\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", tot_fail, send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־
        int begintime = clock();
        int lentmp = sizeof(server_addr);
        int fail_send = 0;//��ʶ�Ƿ���ʧ��
        memset(recv_buf, 0, sizeof(recv_buf));
        while (recvfrom(c_socket, recv_buf, head_len, 0, (sockaddr*)&server_addr, &lentmp) == SOCKET_ERROR)
            if (clock() - begintime > TIMEOUT) {//�����Ѿ���ʱ
                std::printf("��ʱ���·���WAVE_1\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־
                fail_send = 1;//����ʧ��
                //tot_fail++;//ʧ�ܴ�����һ
                break;
            }
        //�ڶ��λ��֣�����ack
        if (fail_send == 0 && cksum(recv_buf, head_len) == 0 && ((recv_buf[1] & (FLAG_ACK | FLAG_FIN))== (FLAG_ACK | FLAG_FIN)) && recv_buf[3] == send_buf[2]) {
            std::printf("����WAVE_2\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//��־
            break;
        }
        tot_fail++;
    }
}


bool send_package(char* message, int lent, char seq) {//��Ƭ����
    if (lent > Max_pack_len) {//��Ƭ���ȹ���
        return false;
    }
    char send_buf[Max_pack_len + head_len];

    memset(send_buf, 0, Max_pack_len + head_len);
    send_buf[1] = FLAG_SEQ;
    send_buf[2] = seq;//���к�
    send_buf[4] = lent / 256;//���ݶδ�С
    send_buf[5] = lent % 256;//���ݶδ�С
    for (int i = head_len; i < lent + head_len; i++)
        send_buf[i] = message[i - head_len];//��������
    send_buf[0] = cksum(send_buf, Max_pack_len + head_len);//У���
    sendto(c_socket, send_buf, Max_pack_len + head_len, 0, (sockaddr*)&server_addr, sizeof(server_addr));//��Ϣ����
    std::printf("�������ݱ�\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], (unsigned char)send_buf[4] * 256 + (unsigned char)send_buf[5]);//��־
    return true;
}

void shake_hand() {//���֣���������
    char send_buf[head_len], recv_buf[head_len];
    unsigned char seq = pre_seq++;
    pre_seq %= MAX_CHAR;

    while (1) {
        //��һ�����֣�����syn,��������
        memset(send_buf, 0, sizeof(send_buf));
        send_buf[1] = FLAG_SYN | FLAG_SEQ;
        send_buf[2] = seq;
        send_buf[0] = cksum(send_buf, head_len);
        sendto(c_socket, send_buf, sizeof(send_buf), 0, (sockaddr*)&server_addr, sizeof(server_addr));
        std::printf("����SHAKE_1\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־

        //�ڶ������֣�����syn ack seq��У��
        int begintime = clock();
        memset(recv_buf, 0, sizeof(recv_buf));
        int lentmp = sizeof(server_addr);
        int fail_send = 0;
        while (recvfrom(c_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&server_addr, &lentmp) == SOCKET_ERROR) {
            if (clock() - begintime > TIMEOUT) {
                //�ѳ�ʱ,���·���syn
                fail_send = 1;
                break;
            }
        }
        if (fail_send == 0 && cksum(recv_buf, head_len) == 0 && recv_buf[1] & (FLAG_ACK | FLAG_SEQ | FLAG_SYN) && recv_buf[3] == seq + 1) {
            {
                std::printf("����SHAKE_2\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//��־

                //���������֣�����ack
                memset(send_buf, 0, sizeof(send_buf));
                send_buf[1] = FLAG_ACK | FLAG_SEQ;
                send_buf[2] = seq + 1;
                send_buf[3] = recv_buf[2] + 1;
                send_buf[0] = cksum(send_buf, head_len);
                int lentmp = sizeof(server_addr);
                sendto(c_socket, send_buf, sizeof(send_buf), 0, (sockaddr*)&server_addr, lentmp);
                std::printf("����SHAKE_3\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", send_buf[1], send_buf[2], send_buf[3], send_buf[0], sizeof(send_buf));//��־
                break;
            }
        }
    }
}


void send_message(char* message, int message_length) {//�������ݰ�message���䳤��Ϊmessage_length
    int package_num = message_length / Max_pack_len + (message_length % Max_pack_len != 0);//ȷ�����������Ƿ�����һ����

    std::printf("����Ҫ����%d�����ݰ�\n", package_num);
    int package_i = 0;//��ǰ���͹����������-----���ں��
    int base = 0;//��ǰ�����еĵ�һ�������(��һ��δȷ�ϵİ�)-----����ǰ��
    int next_verified_seq = pre_seq;//��һ��Ҫȷ��ack��seq
    int timer_begin = clock();//��ʼ������ʱ��
    printf("���ڳ�ʼ��:%d\t����ĩβ��:%d\n", base, package_i);
    while (base < package_num) {
        //���͵�package_i����package_i+N-1����(�����ڷ���)�����ݷ���
        pre_seq = next_verified_seq;
        while (package_i - base < N && package_i < package_num) {
            std::printf("���ڷ��͵�%d�����ݰ�\n", package_i);
            int lent = (package_i == (package_num - 1)) ? (message_length % Max_pack_len) : Max_pack_len;//�������һ����������������
            int tmp_seq = ((int)pre_seq + (package_i - base)) % MAX_CHAR;
            send_package(message + package_i * Max_pack_len, lent, tmp_seq);
            package_i++;
        }
        //����ack
        char recv_buf[head_len];
        memset(recv_buf, 0, sizeof(recv_buf));
        int lentmp = sizeof(server_addr);
        bool timeout = false;
        while (recvfrom(c_socket, recv_buf, head_len, 0, (sockaddr*)&server_addr, &lentmp) == SOCKET_ERROR)
            if (clock() - timer_begin > TIMEOUT) {
                //��ʱ�������ش�N��
                timeout = true;
                package_i = base;//����λ������Ϊbase
                timer_begin = clock();//����������ʱ��
                printf("���ڳ�ʼ��:%d\t����ĩβ��:%d\n", base, package_i);
                break;
            }
		//move_num���յ���ack��Ե�ǰ�����׸������ƶ�����
        int move_num = (next_verified_seq > MAX_CHAR - N && recv_buf[3] < N) ? 
                        recv_buf[3] + MAX_CHAR - next_verified_seq : 
                        recv_buf[3] - next_verified_seq;
        move_num++;
        if (!timeout && cksum(recv_buf, head_len) == 0 && (recv_buf[1] & FLAG_ACK) && move_num>0) {
            //�ɹ�����,������ǰ�ƶ�
            base += move_num;
            next_verified_seq += move_num;
            next_verified_seq %= MAX_CHAR;
            timer_begin = clock();//����������ʱ��
            printf("���ڳ�ʼ��:%d\t����ĩβ��:%d\n", base, package_i);
            std::printf("����ACK\tflag:%x\tseq:%d\tack:%d\tУ���:%d\t���ĳ���:%dBytes\n", recv_buf[1], recv_buf[2], recv_buf[3], recv_buf[0], sizeof(recv_buf));//��־
        }
        //δ�ɹ����գ���ʱ�����䣬���ڲ��䣬�����ش�
    }
    pre_seq = next_verified_seq;
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
        std::printf("ip��ַ���Ϸ�!\n");
    }

    int port = 1111;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(sendip.c_str());

    c_socket = socket(AF_INET, SOCK_DGRAM, 0);

    //���÷�����
    int time_out = 1;//1ms��ʱ
    setsockopt(c_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));

    if (c_socket == INVALID_SOCKET) {
        std::printf("creat udp c_socket error");
        return 0;
    }



    string filename;
    while (1) {
        //std::printf("������Ҫ���͵��ļ�����");
        //cin >> filename;
        filename = "3.jpg";
        //filename = "1.jpg";
        ifstream fin(filename.c_str(), ifstream::binary);
        if (!fin) {
            continue;
            std::printf("�ļ�δ�ҵ�!\n");
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
    std::printf("���ӽ�����...\n");
    //1.�������ֽ���
    shake_hand();

    std::printf("���ӽ�����ɡ� \n���ڷ�����Ϣ...\n");
    long long head, tail, freq, time;
    //send_message((char*)(filename.c_str()), filename.length());
    //std::printf("%s�ļ���������ϣ����ڷ����ļ�����...\n", filename.c_str());

    //2. ��������
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    send_message(buffer, buffer_len);
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);
    time = (tail - head) * 1000.0 / freq;

    std::printf("���͵��ļ�bytes:%d\n", buffer_len);
    std::cout << "����ʱ��Ϊ��" << time << "ms" << endl;
    std::cout << "ƽ��������Ϊ��" << buffer_len * 8 / 1000 / ((double)time / 1000) << "     kbps" << endl;
    std::printf("%s�ļ����ݷ�����ϡ�\n", filename.c_str());

    //3. 2�λ��ֶϿ�����
    std::printf("��ʼ�Ͽ�����...\n");
    wave_hand();
    std::printf("�����ѶϿ���");

    closesocket(c_socket);
    WSACleanup();
    cout << "--------------------------------------\n\n\n";//��־�ָ���
    getchar();
    return 0;
}