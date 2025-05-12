#ifndef CHATSERVER_H
#define CHATSERVER_H

#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
using namespace std;
using namespace muduo::net;
using namespace muduo;

//聊天服务器主类
class ChatServer
{
    public:
    //构造函数，初始化聊天服务器对象
    ChatServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const string& nameArg);
    //启动服务
    void start();
    private:
    //回调函数，上报连接相关信息
    void onConnection(const TcpConnectionPtr&conn);
    //回调函数，上报读写相关信息
    void onMessage(const TcpConnectionPtr&conn,
        Buffer *buffer,
        Timestamp time);
    TcpServer _server;//组合的muduo库，实现服务器功能的类对象
    EventLoop *_loop; //指向事件循环对象的指针
};

#endif