#include <winsock2.h>  
#include <stdio.h>  
#include <string.h>
#pragma comment(lib,"ws2_32.lib") 
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<vector>
#include<iostream>

#define MAX_P_NUM 10
std::vector<SOCKET> ClientSockets;//用于记录全部client的SOCKET
int ClientSocket_num = 0;
#define MAXBUFSIZE 1024 //接收或发送的data的最大长度为sizeof(char[1024])

DWORD WINAPI handlerRequest(LPVOID lparam)
{
    int ClientSocket_id = ClientSocket_num;
    ClientSocket_num++;
    if (ClientSocket_num > MAX_P_NUM) {
        printf("群聊人数已达上限\n");
        return 0;
    }
    SOCKET socket = (SOCKET)(LPVOID)lparam;
    ClientSockets.push_back(socket);
    std::vector<SOCKET>::iterator socket_it = ClientSockets.end() - 1;
   // std::cout << socket == ClientSockets.back();
   // ClientSockets[ClientSocket_id] = (SOCKET)(LPVOID)lparam;
    char sendBuf[MAXBUFSIZE];

    //发送初始确认信息  
    memset(sendBuf, 0, MAXBUFSIZE*sizeof(char));
    strcpy_s(sendBuf,MAXBUFSIZE,"成功连接\n\0");
    printf("%s", sendBuf);
    send(socket, sendBuf, MAXBUFSIZE, 0);
    memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
    strcpy_s(sendBuf, MAXBUFSIZE, "q");
    send(socket, sendBuf, MAXBUFSIZE, 0);
    printf("完成连接到%d\n", ClientSocket_id);

    while (1) {
        //接收
        char receiveBuf[MAXBUFSIZE];  
        memset(receiveBuf, 0, MAXBUFSIZE * sizeof(char));
        recv(socket, receiveBuf, MAXBUFSIZE, 0);
        printf("收到来自%d: %s", ClientSocket_id, receiveBuf);

        //关闭并断开连接
        if (strcmp(receiveBuf, "q") == 0) {
            printf("%d已退出群聊\n", ClientSocket_id);
            closesocket(socket);
            //ClientSockets.erase(socket_it);
            ClientSocket_num--;
            break;
        }
        
        //发送此条message给所有client
        if (strcmp(receiveBuf, "\n") == 0) break;
        std::vector<SOCKET>::iterator it;
        int i;
        for (it = ClientSockets.begin(),i=0; it != ClientSockets.end(); it++,i++)
        {
            send(*it, receiveBuf, MAXBUFSIZE, 0);
            printf("发送到%d:  %s", i, receiveBuf);
        }

        //发送q使当前client停止接收
        memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
        strcpy_s(sendBuf, MAXBUFSIZE, "q");
        send(socket, sendBuf, MAXBUFSIZE, 0);
    }
    return 0;
}

int main()
{
    //创建套接字，socket前的一些检查工作，包括服务的启动  
    WORD myVersionRequest;
    WSADATA wsaData;
    myVersionRequest = MAKEWORD(1, 1);
    int err;
    err = WSAStartup(myVersionRequest, &wsaData);
    if (!err)
    {
        printf("已打开套接字\n");
    }
    else
    {
        //进一步绑定套接字  
        printf("嵌套字未打开!");
        return 0;
    }
    SOCKET serSocket = socket(AF_INET, SOCK_STREAM, 0);//创建了可识别套接字  
    //需要绑定的参数，主要是本地的socket的一些信息。  
    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//ip地址  
    addr.sin_port = htons(6000);//绑定端口  

    bind(serSocket, (SOCKADDR*)&addr, sizeof(SOCKADDR));//绑定完成  
    listen(serSocket, MAX_P_NUM);//其中第二个参数代表能够接收的最多的连接数  

    SOCKADDR_IN clientsocket;
    int len = sizeof(SOCKADDR);
    while (1)
    {
        //第二次握手，通过accept来接受对方的套接字的信息  
        SOCKET serConn = accept(serSocket, (SOCKADDR*)&clientsocket, &len);//如果这里不是accept而是conection的话。。就会不断的监听  
        HANDLE hThread = CreateThread(NULL, NULL, handlerRequest, LPVOID(serConn), 0, NULL);
        CloseHandle(hThread);
    }

    closesocket(serSocket);//关闭  
    WSACleanup();//释放资源的操作 
    return 0;
}
