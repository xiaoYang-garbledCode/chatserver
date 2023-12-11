#include "chatservice.hpp"
#include "chatserver.hpp"
#include "public.hpp"
#include <iostream>
#include <string>
#include <muduo/base/Logging.h>
#include <muduo/base/TimeZone.h>
#include <vector>
using namespace placeholders;

// 获取单例的接口函数
chatService *chatService::instance()
{
    static chatService service;
    return &service;
}

chatService::chatService()
{
    // 用户基本业务
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&chatService::Login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&chatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&chatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&chatService::addFriend, this, _1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&chatService::oneChat, this, _1, _2, _3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&chatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&chatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&chatService::groupChat, this, _1, _2, _3)});

    //连接redis服务器
    if(_redis.connect())
    {
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&chatService::handleRedisSubscribeMessage, this, _1,_2));
    }

}

// 服务器异常，业务重置方法
void chatService::reset()
{
    // 把online用户设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler chatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it != _msgHandlerMap.end())
    {
        return _msgHandlerMap[msgid];
    }
    else
    {
        // 返回一个空处理器
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid" << msgid << "can not find a handler!!!";
        };
    }
}

// 处理业务的函数
void chatService::Login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 用户不可重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 1;
            response["msg"] = "this account is using now, input another";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，添加连接到_userconnMap
            {
                lock_guard<mutex> lock(_connMutex);
                _userconnMap.insert({id, conn});
            }

            //登录成功，向redis订阅channel(id)
            _redis.subscribe(id);

            //登录成功，更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询该用户是否有离线消息
            vector<std::string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除
                _offlineMsgModel.remove(id);
            }

            //查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(User &user : userVec)
                {
                    nlohmann::json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            //查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty())
            {
                // group:[{groupid:[xxx,xxx,xxx]}]
                vector<std::string> groupV;
                for(Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user: group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getName();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    }
    // 用户存在但是密码错误 或者用户不存在 todo 业务待完善！！！
    else
    {
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["msg"] = "login failed";
        conn->send(response.dump());
    }
}

void chatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);
    // 注册成功
    if (_userModel.insert(user))
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        response["msg"] = " please remember your id, register success!";
        conn->send(response.dump());
    }
    // 注册失败
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["msg"] = " register failed";
        conn->send(response.dump());
    }
}

void chatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    // 在_userconnMap中删除该连接，需要线程安全
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userconnMap.find(userid);
        if (it != _userconnMap.end())
        {
            _userconnMap.erase(it);
        }
    }

    //用户注销，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    // 更改state
    //  User user =  _userModel.query(userid);
    //  user.setState("offline");
    //  _userModel.updateState(user);
    // 更好的做法
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 处理异常退出
void chatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    // 更改用户状态，删除连接
    {
        lock_guard<mutex> lock(_connMutex);
        
        for (auto it = _userconnMap.begin(); it != _userconnMap.end(); ++it)
        {
            if(it->second == conn)
            {   //从map表中删除用户连接
                //记录当前的用户，在后面更改状态，让互斥锁里执行的语句更少
                user.setId(it->first);
                _userconnMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());

    //更改状态      user.getId()!=-1   !!!等号不能和()贴一起
    if(user.getId() !=-1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

void chatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    Group group;
    group.setName(name);
    group.setDesc(desc);
    if(_groupModel.createGroup(group))
    {
        //存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
        json response;
        response["msgid"] = CREATE_GROUP_MSG;
        response["groupID"] = group.getId();
        response["errno"] = 0;
        response["msg"] = "create group success, please remember your groupID!";
        conn->send(response.dump());
    }
    else
    {
        json response;
        response["msgid"] = CREATE_GROUP_MSG;
        response["errno"] = 1;
        response["msg"] = "create group failed";
        conn->send(response.dump());
    }
}

void chatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid,"normal");
}
    

 void chatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
 {
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    _friendModel.insert(userid, friendid);
 }

 void chatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
 {
    int toid = js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userconnMap.find(toid);
        if(it != _userconnMap.end())
        {
            it->second->send(js.dump());
            return;
        }
    }

    //在本服务器没有查到该用户的消息，去数据库查询，如果该用户在线，说明是在其他服务器上登录的
    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        std:: cout<< "在本服务器没有查到该用户的消息，去数据库查询该用户在线,将消息发布" <<std::endl;
        _redis.publish(toid, js.dump());
        return;
    }
     _offlineMsgModel.insert(toid, js.dump());
 }

void chatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    js["time"] = time.toFormattedString();
    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec)
    {
        User user = _userModel.query(id);
        js["name"] = user.getName();
        auto it = _userconnMap.find(id);
        if(it != _userconnMap.end())
        {
            //转发群消息
            it->second->send(js.dump());
        }
        else
        {
             //在本服务器没有查到该用户的消息，去数据库查询
            //查询toid是否在线
            if(user.getState() == "offline")
            {
                _offlineMsgModel.insert(id,js.dump());
            }
            else
            {
                //该用户在线，说明是在其他服务器上登录的
                _redis.publish(id,js.dump());
            }
        }
    }
}

// 从redis消息队列中获取订阅的消息,发给客户端
void chatService::handleRedisSubscribeMessage(int userid, std::string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userconnMap.find(userid);
    if (it != _userconnMap.end())
    {
        std::cout<<"从redis消息队列中获取订阅的消息,发给客户端" <<std::endl;
        it->second->send(msg);
        return;
    }

    //存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}