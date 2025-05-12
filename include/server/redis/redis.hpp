#ifndef REDIS_H
#define REDIS_H

#include<string>
#include<functional>
#include<hiredis/hiredis.h>
#include<thread>
using namespace std;


class Redis
{

public:
    Redis();
    ~Redis();

    //连接redis服务器
    bool connect();

    //向redis指定的通道发布消息
    bool publish(int channel, string message);

    //向redis指定的通道subscribe订阅消息
    bool subscribe(int channel);

    //向redis指定的通道unsubscribe取消订阅消息
    bool unsubscribe(int channel);

    //在独立线程中接收订阅通道中的消息
    void observer_channel_message();

    void init_notify_handler(function<void(int,string)> fn);
private:
    //hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;
    //hiredis同步上下文对象，负责subscribe消息
    redisContext *_subscribe_context;
    //回调操作，收到订阅的消息，给server层上报
    function<void(int,string)> _notify_message_handler;

};




#endif