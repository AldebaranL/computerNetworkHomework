#include <winsock2.h>  
#include <stdio.h>  
#include <string.h>
#pragma comment(lib,"ws2_32.lib") 
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<vector>
#include<iostream>

#define MAX_P_NUM 10
std::vector<SOCKET> ClientSockets;//���ڼ�¼ȫ��client��SOCKET
int ClientSocket_num = 0;
#define MAXBUFSIZE 1024 //���ջ��͵�data����󳤶�Ϊsizeof(char[1024])

DWORD WINAPI handlerRequest(LPVOID lparam)
{
    int ClientSocket_id = ClientSocket_num;
    ClientSocket_num++;
    if (ClientSocket_num > MAX_P_NUM) {
        printf("Ⱥ�������Ѵ�����\n");
        return 0;
    }
    SOCKET socket = (SOCKET)(LPVOID)lparam;
    ClientSockets.push_back(socket);
    std::vector<SOCKET>::iterator socket_it = ClientSockets.end() - 1;
   // std::cout << socket == ClientSockets.back();
   // ClientSockets[ClientSocket_id] = (SOCKET)(LPVOID)lparam;
    char sendBuf[MAXBUFSIZE];

    //���ͳ�ʼȷ����Ϣ  
    memset(sendBuf, 0, MAXBUFSIZE*sizeof(char));
    strcpy_s(sendBuf,MAXBUFSIZE,"�ɹ�����\n\0");
    printf("%s", sendBuf);
    send(socket, sendBuf, MAXBUFSIZE, 0);
    memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
    strcpy_s(sendBuf, MAXBUFSIZE, "q");
    send(socket, sendBuf, MAXBUFSIZE, 0);
    printf("������ӵ�%d\n", ClientSocket_id);

    while (1) {
        //����
        char receiveBuf[MAXBUFSIZE];  
        memset(receiveBuf, 0, MAXBUFSIZE * sizeof(char));
        recv(socket, receiveBuf, MAXBUFSIZE, 0);
        printf("�յ�����%d: %s", ClientSocket_id, receiveBuf);

        //�رղ��Ͽ�����
        if (strcmp(receiveBuf, "q") == 0) {
            printf("%d���˳�Ⱥ��\n", ClientSocket_id);
            closesocket(socket);
            //ClientSockets.erase(socket_it);
            ClientSocket_num--;
            break;
        }
        
        //���ʹ���message������client
        if (strcmp(receiveBuf, "\n") == 0) break;
        std::vector<SOCKET>::iterator it;
        int i;
        for (it = ClientSockets.begin(),i=0; it != ClientSockets.end(); it++,i++)
        {
            send(*it, receiveBuf, MAXBUFSIZE, 0);
            printf("���͵�%d:  %s", i, receiveBuf);
        }

        //����qʹ��ǰclientֹͣ����
        memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
        strcpy_s(sendBuf, MAXBUFSIZE, "q");
        send(socket, sendBuf, MAXBUFSIZE, 0);
    }
    return 0;
}

int main()
{
    //�����׽��֣�socketǰ��һЩ��鹤�����������������  
    WORD myVersionRequest;
    WSADATA wsaData;
    myVersionRequest = MAKEWORD(1, 1);
    int err;
    err = WSAStartup(myVersionRequest, &wsaData);
    if (!err)
    {
        printf("�Ѵ��׽���\n");
    }
    else
    {
        //��һ�����׽���  
        printf("Ƕ����δ��!");
        return 0;
    }
    SOCKET serSocket = socket(AF_INET, SOCK_STREAM, 0);//�����˿�ʶ���׽���  
    //��Ҫ�󶨵Ĳ�������Ҫ�Ǳ��ص�socket��һЩ��Ϣ��  
    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//ip��ַ  
    addr.sin_port = htons(6000);//�󶨶˿�  

    bind(serSocket, (SOCKADDR*)&addr, sizeof(SOCKADDR));//�����  
    listen(serSocket, MAX_P_NUM);//���еڶ������������ܹ����յ�����������  

    SOCKADDR_IN clientsocket;
    int len = sizeof(SOCKADDR);
    while (1)
    {
        //�ڶ������֣�ͨ��accept�����ܶԷ����׽��ֵ���Ϣ  
        SOCKET serConn = accept(serSocket, (SOCKADDR*)&clientsocket, &len);//������ﲻ��accept����conection�Ļ������ͻ᲻�ϵļ���  
        HANDLE hThread = CreateThread(NULL, NULL, handlerRequest, LPVOID(serConn), 0, NULL);
        CloseHandle(hThread);
    }

    closesocket(serSocket);//�ر�  
    WSACleanup();//�ͷ���Դ�Ĳ��� 
    return 0;
}
