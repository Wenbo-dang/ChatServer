#include "chatservice.hpp"
#include "public.hpp"
#include "offlinemessagemode.hpp"
#include<functional>
#include<muduo/base/Logging.h>
#include<string>
#include<iostream>
#include<vector>


using namespace muduo;
using namespace std;



//获取单例对象的接口函数
ChatService * ChatService::instance()
{
    static ChatService service;
    return &service;
}

//注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});

    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});

    if(_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}

MsgHandler ChatService::getHandler(int msgid)
{
    auto it = _msgHandlerMap.find(msgid);
    if(it==_msgHandlerMap.end())
    {
        //返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn,json &js, Timestamp time)
        {
            LOG_ERROR<<"msgid:"<<msgid<<" can not find handler!";
        };
        

    }
    else{
        return _msgHandlerMap[msgid];
    }
    
}


//处理登陆业务
void ChatService::login(const TcpConnectionPtr &conn,json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    LOG_INFO<<id<<":"<<pwd;
    LOG_INFO<<user.getId()<<":"<<user.getPassword();
    if(user.getId()==id && user.getPassword()==pwd)
    {
        if(user.getState()=="online")
        {
            //用户已经登录，不允许重复登录
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=2;
            response["errmsg"]="该账号已经登录，请重新输入新账号！";
            conn->send(response.dump());

        }
        else
        {
            //登陆成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn});
            }
            
            //登陆成功,更新用户状态信息 state offline ->online
            user.setState("online");
            _userModel.updateState(user);

            //登陆成功后，向redis订阅channel
            _redis.subscribe(id);

            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=0;
            response["id"]=user.getId();
            response["name"]=user.getName();

            //查询该用户是否有离线消息
            vector<string> vec=_offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"]=vec;
                //读取完用户的离线消息后，把该用户的所有离线消息删除
                _offlineMsgModel.remove(id);
            }

            //查询该用户的好友信息
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
               vector<string> vec2;
               for(User &user:userVec)
               {
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
               }
               response["friends"]=vec2;
            }

            conn->send(response.dump());
        }
        
    }
    else
    {
        //该用户不存在，登陆失败
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="用户名或密码错误";
        conn->send(response.dump());

    }
    //LOG_INFO<<"do login service!!";
}




//处理注册业务  name password
void ChatService::reg(const TcpConnectionPtr &conn,json &js, Timestamp time)
{
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    bool state = _userModel.insert(user);
    if(state)
    {
        //注册成功
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=0;
        response["id"]=user.getId();
        conn->send(response.dump());

    }
    else{
        //注册失败
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=1;
        conn->send(response.dump());

    }
    

    //LOG_INFO<<"do reg service!!";

}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    //用户注销，取消通道
    _redis.unsubscribe(userid);


    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}
//客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        
        for(auto it=_userConnMap.begin();it!=_userConnMap.end();++it)
        {
            if(it->second==conn)
            {
                user.setId(it->first);
                //从map表中删除连接
                _userConnMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());

    if(user.getId()!=-1)
    {
        //更新mysql中用户状态
        user.setState("offline");
        _userModel.updateState(user);
    }
}

void ChatService::oneChat(const TcpConnectionPtr &conn,json &js, Timestamp time)
{
    int toid=js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(toid);
        if(it!=_userConnMap.end())
        {
            //toid在线，转发消息 服务器主动推送消息给toid
            it->second->send(js.dump());
            return;
        }
    }

    //查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState()=="online")
    {
        _redis.publish(toid,js.dump());
        return;
    }
    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
}

void ChatService::reset()
{
    //把online用户信息重置为offline
    _userModel.resetState();

}

//添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    //存储好友信息
    _friendModel.insert(userid,friendid);

}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn,json &js, Timestamp time)
{
    int userid=js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1,name,desc);
    if(_groupModel.createGroup(group))
    {
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
}
//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn,json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn,json &js, Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    vector<int> useridVec=_groupModel.queryGroupUsers(userid,groupid);

    lock_guard<mutex> lock(_connMutex);
    for(int id:useridVec)
    {
        auto it = _userConnMap.find(id);
        if(it!=_userConnMap.end())
        {
            it->second->send(js.dump());
        }
        else{
            //查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState()=="online")
            {
                _redis.publish(id,js.dump());
                return;
            }
            _offlineMsgModel.insert(id,js.dump());
        }
    }
}

void ChatService::handleRedisSubscribeMessage(int userid, string message)
{
    json js=json::parse(message.c_str());

    lock_guard<mutex> lock(_connMutex);
    auto it=_userConnMap.find(userid);
    if(it!=_userConnMap.end())
    {
        it->second->send(js.dump());
        return;
    }

    _offlineMsgModel.insert(userid,message);
}