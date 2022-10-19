#include <winsock2.h>                
#include <stdio.h>  
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib") 
#include<time.h>
#include <conio.h>
#include <string>
#include <iostream>
using namespace std;

#define MAXBUFSIZE 1024 //���ջ��͵�data����󳤶�Ϊsizeof(char[1024])
char name[20];
int sendMessage(SOCKET clientSocket) {
    //�Ӽ��̶�����Ϣ������server����һ������õģ����е�ǰ������ʱ�䡢��Ϣ����Ϣ��
    //����1����ǰ�û��˳�Ⱥ�ģ�����0����ǰ�û�����������ɱ�����Ϣ��

    char massage[900];
    char sendBuf[MAXBUFSIZE];

    memset(massage, 0, 900 * sizeof(char));
    printf("�뷢�������Ϣ:");
    cin.getline(massage, 900, '\n');
   // scanf_s("%s", &massage, 900);
    if (strcmp(massage, "q") == 0) {
        //����q��srver
        memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
        strcpy_s(sendBuf, MAXBUFSIZE, "q");
        send(clientSocket, sendBuf, MAXBUFSIZE, 0);
        return 1;
    }

    time_t timep;
    time(&timep);
    struct tm p;
    gmtime_s(&p, &timep);//��ȡ��ǰʱ��
    
    memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
    sprintf_s(sendBuf, "%s(%d-%d-%d %d:%d:%d):  %s\n", name, 1900 + p.tm_year, 1 + p.tm_mon, p.tm_mday, 8 + p.tm_hour, p.tm_min, p.tm_sec, massage);
    //����socket����  
    //printf("�ѷ���:%s\n", sendBuf);
    //printf("�ѷ���:%d\n", strlen(sendBuf));
    send(clientSocket, sendBuf, MAXBUFSIZE, 0);
    // printf("�ѷ���:%s\n", sendBuf);
    return 0;
}

int recv_until_q(SOCKET clientSocket) {
    //���Ͻ�������server����Ϣ�������ֱ�����յ�"q"���˳���
    char receiveBuf[MAXBUFSIZE];
    while (1) {
        //����server������data  
        memset(receiveBuf, 0, MAXBUFSIZE * sizeof(char));
        recv(clientSocket, receiveBuf, MAXBUFSIZE, 0);
        //printf("����server����Ϣ:\n");
        if (strcmp(receiveBuf, "q") == 0) break;
        printf("%s", receiveBuf);
    }
    return 0;
}

int main()
{
	int err;
	WORD versionRequired;
	WSADATA wsaData;
	versionRequired = MAKEWORD(1, 1);
	err = WSAStartup(versionRequired, &wsaData);//Э���İ汾��Ϣ 

    //ͨ��WSACleanup�ķ���ֵ��ȷ��socketЭ���Ƿ�����  
    if (!err)
    {
        printf("�ͻ���Ƕ�����Ѿ���!\n");
    }
    else
    {
        printf("�ͻ��˵�Ƕ���ִ�ʧ��!\n");
        return 0;//����  
    }
    //����socket����ؼ��ʣ�������һ���Ǹ�ͼ���е�socket�����  
    //ע��socket�������������������������socket��������ϵͳ��socket�����ͣ��Լ�һЩ������Ϣ  
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    //socket����У���������һ���ṹ��SOCKADDR_IN����������һЩ��Ϣ����socket��ϵͳ��  
    //�˿ںţ�ip��ַ����Ϣ������洢���Ƿ������˵ļ��������Ϣ  
    SOCKADDR_IN clientsock_in;
    inet_pton(AF_INET, "127.0.0.1",&clientsock_in.sin_addr.S_un.S_addr); 
    clientsock_in.sin_family = AF_INET;
    clientsock_in.sin_port = htons(6000);

    //ǰ�ڶ������׽��֣������˷������˵ļ������һЩ��Ϣ�洢��clientsock_in�У�  
    //׼��������ɺ�Ȼ��ʼ������׽������ӵ�Զ�̵ļ����  
    //Ҳ���ǵ�һ������  

    printf("what's your name:");
    scanf_s("%s", &name,50);
    
    //��ʼ����  
    int con_err = connect(clientSocket, (SOCKADDR*)&clientsock_in, sizeof(SOCKADDR));
    
    //��������ȷ����Ϣ
    recv_until_q(clientSocket);

    while (1) {
        //���벢������Ϣ
        int send_err = sendMessage(clientSocket);
        if (send_err == 1) break;
        //���Ͻ�����Ϣֱ���յ�q
        recv_until_q(clientSocket);
    }

    //�ر��׽���  
    closesocket(clientSocket);
    //�رշ���  
    WSACleanup();
    return 0;
}
