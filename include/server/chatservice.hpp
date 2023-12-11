#pragma once
#include<muduo/net/TcpConnection.h>
#include<unordered_map>
#include<functional>
#include<mutex>

using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "usermodel.hpp"
#include"groupmodel.hpp"
#include "friendmodel.hpp"
#include"offlinemessagemodel.hpp"
#include "json.hpp"
#include "redis.hpp"
using json = nlohmann::json;

//表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json&js, Timestamp)>;

//业务类
class chatService
{
public:
    //获取单例对象的接口函数
    static chatService* instance();
    //处理业务的函数
    void Login(const TcpConnectionPtr&, json&, Timestamp);
    void reg(const TcpConnectionPtr&, json&, Timestamp);
    void oneChat(const TcpConnectionPtr&, json&, Timestamp);

    void addFriend(const TcpConnectionPtr&, json&, Timestamp);

    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    
    //处理异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    //服务器异常，业务重置
    void reset();

    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);

    //从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);

private:
    //单例对象，构造函数私有化
    chatService();

    //存储id与其映射方法
    unordered_map<int, MsgHandler> _msgHandlerMap;
    //存储用户连接
    unordered_map<int, TcpConnectionPtr> _userconnMap;
    //定义互斥锁，维护_userconnMap的线程安全
    mutex _connMutex;
    User _user;
    userModel _userModel;
    GroupModel _groupModel;
    FriendModel _friendModel;
    offlineMsgModel _offlineMsgModel;

    Redis _redis;
};