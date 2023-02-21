#include <winsock2.h>                
#include <stdio.h>  
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib") 
#include<time.h>
#include <conio.h>
#include <string>
#include <iostream>
using namespace std;

#define MAXBUFSIZE 1024 //接收或发送的data的最大长度为sizeof(char[1024])
char name[20];
int sendMessage(SOCKET clientSocket) {
    //从键盘读入消息，并给server发送一条处理好的（带有当前姓名、时间、消息）信息。
    //返回1代表当前用户退出群聊，返回0代表当前用户正常发送完成本条消息。

    char massage[900];
    char sendBuf[MAXBUFSIZE];

    memset(massage, 0, 900 * sizeof(char));
    printf("请发送你的消息:");
    cin.getline(massage, 900, '\n');
   // scanf_s("%s", &massage, 900);
    if (strcmp(massage, "q") == 0) {
        //发送q至srver
        memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
        strcpy_s(sendBuf, MAXBUFSIZE, "q");
        send(clientSocket, sendBuf, MAXBUFSIZE, 0);
        return 1;
    }

    time_t timep;
    time(&timep);
    struct tm p;
    gmtime_s(&p, &timep);//获取当前时间
    
    memset(sendBuf, 0, MAXBUFSIZE * sizeof(char));
    sprintf_s(sendBuf, "%s(%d-%d-%d %d:%d:%d):  %s\n", name, 1900 + p.tm_year, 1 + p.tm_mon, p.tm_mday, 8 + p.tm_hour, p.tm_min, p.tm_sec, massage);
    //发送socket数据  
    //printf("已发送:%s\n", sendBuf);
    //printf("已发送:%d\n", strlen(sendBuf));
    send(clientSocket, sendBuf, MAXBUFSIZE, 0);
    // printf("已发送:%s\n", sendBuf);
    return 0;
}

int recv_until_q(SOCKET clientSocket) {
    //不断接收来自server的消息并输出，直至接收到"q"，退出。
    char receiveBuf[MAXBUFSIZE];
    while (1) {
        //接收server发来的data  
        memset(receiveBuf, 0, MAXBUFSIZE * sizeof(char));
        recv(clientSocket, receiveBuf, MAXBUFSIZE, 0);
        //printf("来自server的消息:\n");
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
	err = WSAStartup(versionRequired, &wsaData);//协议库的版本信息 

    //通过WSACleanup的返回值来确定socket协议是否启动  
    if (!err)
    {
        printf("客户端嵌套字已经打开!\n");
    }
    else
    {
        printf("客户端的嵌套字打开失败!\n");
        return 0;//结束  
    }
    //创建socket这个关键词，这里想一下那个图形中的socket抽象层  
    //注意socket这个函数，他三个参数定义了socket的所处的系统，socket的类型，以及一些其他信息  
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    //socket编程中，它定义了一个结构体SOCKADDR_IN来存计算机的一些信息，像socket的系统，  
    //端口号，ip地址等信息，这里存储的是服务器端的计算机的信息  
    SOCKADDR_IN clientsock_in;
    inet_pton(AF_INET, "127.0.0.1",&clientsock_in.sin_addr.S_un.S_addr); 
    clientsock_in.sin_family = AF_INET;
    clientsock_in.sin_port = htons(6000);

    //前期定义了套接字，定义了服务器端的计算机的一些信息存储在clientsock_in中，  
    //准备工作完成后，然后开始将这个套接字链接到远程的计算机  
    //也就是第一次握手  

    printf("what's your name:");
    scanf_s("%s", &name,50);
    
    //开始连接  
    int con_err = connect(clientSocket, (SOCKADDR*)&clientsock_in, sizeof(SOCKADDR));
    
    //接收连接确认消息
    recv_until_q(clientSocket);

    while (1) {
        //读入并发送消息
        int send_err = sendMessage(clientSocket);
        if (send_err == 1) break;
        //不断接收消息直至收到q
        recv_until_q(clientSocket);
    }

    //关闭套接字  
    closesocket(clientSocket);
    //关闭服务  
    WSACleanup();
    return 0;
}
