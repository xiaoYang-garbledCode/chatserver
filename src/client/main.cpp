#include"json.hpp"
#include<iostream>
#include<thread>
#include <sys/types.h>         
#include <sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<semaphore.h>
#include<atomic>
#include "user.hpp"
#include "public.hpp"
#include "friendmodel.hpp"
#include"group.hpp"
//记录当前系统登录的用户信息
User g_currentUser;
//记录当前系统该用户的好友信息
std::vector<User> g_currentUserFriendList;
//记录当前系统该用户的群组信息
std::vector<Group> g_currentUserGroupList;
//用于读写线程之间的通信
sem_t rwsem;

//记录登入状态
atomic_bool g_isLoginSuccess(false);

//控制主菜单页面程序
bool isMainMenuRunning = false;

//接收线程
void readTaskHandler(int clientfd);
//主聊天页面程序
void mainMenu(int);
int main(int argc, char **argv)
{
    if (argc<3)
    {
        std::cout <<"command invalid! example: ./ChatClient 127.0.0.1 6000"<< std::endl;
        exit(-1);
    }
    // 解析通过命令行传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    //创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        std::cerr<< "socket create error"<< std::endl;
        exit(-1);
    }

    //填写client需要连接的server信息 ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    //clietn 和server进行连接
    if(-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        std::cerr << "connect server error" << std::endl;
        close(clientfd);
        exit(-1);
    }

    //初始化读写线程通信用的信号量
    sem_init(&rwsem,0,0);

    //连接服务器成功，启动接收子线程
    std::thread readTask(readTaskHandler,clientfd);
    readTask.detach();
    
    //main线程用于接收用户输入，负责发送数据
    for(;;)
    {
        //显示首页面菜单，登录，注册，退出
        std::cout<<"=========================="<<std::endl;
        std::cout <<"1. login" <<std::endl;
        std::cout <<"2. register" <<std::endl;
        std::cout <<"3. quit" <<std::endl;
        std::cout <<"=========================="<<std::endl;
        std::cout <<"choice:";
        int choice;
        std::cin >>choice;
        std::cin.get(); // 读掉缓冲区残留的回车。

        switch(choice)
        {
            case 1: //login业务
            {
                int id=0;
                char pwd[50] ={0};
                std::cout<< "userid:";
                std::cin >>id;
                std::cin.get();
                std::cout<<"userpassword:";
                std::cin.getline(pwd,50);

                nlohmann::json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id ;
                js["password"] = pwd;
                string request = js.dump();

                g_isLoginSuccess = false;

                int len = send(clientfd, request.c_str(), strlen(request.c_str())+1, 0);
                if(-1 == len)
                {
                    std::cerr<<"send login msg error, request is: " << request <<std::endl;
                }

                sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的相应消息后，通知这里。
                
                if(g_isLoginSuccess)
                {
                    //进入聊天主菜单页面
                    isMainMenuRunning = true;
                    mainMenu(clientfd);
                }
            }
            break;
            case 2: //register业务
            {
                char name[50] = {0};
                char pwd[50] = {0};
                std::cout << "input username:";
                std::cin.getline(name, 50);
                std::cout << "input userpassword:";
                std::cin.getline(pwd, 50);

                nlohmann::json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str())+1 ,0);
                if(-1 == len)
                {
                    std::cerr << "send register msg error, register request is: " << request << std::endl;
                    close(clientfd);
                }

                sem_wait(&rwsem); //等待信号量，子线程处理完注册消息会通知。
            }
            break;
            case 3: //quit业务
                close(clientfd);
                sem_destroy(&rwsem);
                exit(0);
            default:
                std::cerr << "invalid input!" <<std::endl;
                break;
        }
    }
    return 0;
}

//处理登录的相应逻辑
void doLoginResponse(nlohmann::json &js);
//处理注册的相应逻辑
void doRegResponse(nlohmann::json &js);

// 子线程 接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0); //阻塞等待服务器的消息
        if(-1 == len)
        {
            std::cerr << "thread recvmsg error!" <<std::endl;
            close(clientfd);
            exit(-1);
        }

        //接收Chatserver转发的数据，反序列化生成json数据对象
        nlohmann::json js = nlohmann::json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            std::cout<< js["time"].get<std::string>() <<"[" <<js["id"]<<"]" << js["name"].get<std::string>()
                <<"said: " <<js["msg"].get<std::string>() <<std::endl;
            continue;
        }

        if(GROUP_CHAT_MSG == msgtype)
        {
            std::cout<< "群消息[" <<js["groupid"] << "]" <<js["time"].get<std::string>() <<"[" << js["id"]
                << "]" << js["name"].get<std::string>()<< "said" <<js["msg"].get<std::string>() <<std::endl;
            continue;
        }

        if (LOGIN_MSG_ACK == msgtype)
        {
            doLoginResponse(js);
            sem_post(&rwsem);
            continue;
        }

        if(REG_MSG_ACK == msgtype)
        {
            doRegResponse(js);
            sem_post(&rwsem);
            continue;
        }
    }
}

void showCurrentUserData();

void doRegResponse(nlohmann::json &responsejs)
{
    if(0!= responsejs["errno"].get<int>()) //注册失败
    {
        std::cerr << "name is already exist, register error!" << std::endl;
    }
    else
    {
        std::cout<< "name register success, userid is: " << responsejs["id"]
            << ", do note forget it" <<std::endl;
    }
}

//处理登录的相应逻辑
void doLoginResponse(nlohmann::json &responsejs)
{
    if(0 != responsejs["errno"].get<int>()) // 登录失败
    {
        std::cerr << responsejs["msg"] << std::endl;
        g_isLoginSuccess = false;
    }
    else    //登录成功
    {
        //记录当前用户的id和name
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        //记录当前用户的好友列表信息
        if(responsejs.contains("friends"))
        {
            //初始化
            g_currentUserFriendList.clear();

            vector<std::string> vec = responsejs["friends"];
            for(std::string &str :vec)
            {
                nlohmann::json js = nlohmann::json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        //记录当前用户的群组列表信息
        if(responsejs.contains("groups"))
        {
            //初始化
            g_currentUserGroupList.clear();

            vector<string> vec1 = responsejs["groups"];
            for(std::string &groupstr : vec1)
            {
                nlohmann::json grpjs = nlohmann::json::parse(groupstr);
                Group group;
                group.setId(grpjs["id"].get<int>());
                group.setName(grpjs["groupname"]);
                group.setDesc(grpjs["groupdesc"]);

                vector<string> vec2 = grpjs["users"];
                for(string &userstr : vec2)
                {
                    GroupUser user;
                    nlohmann::json js = nlohmann::json::parse(userstr);
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(user);
                }
                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登入用户的基本信息
        showCurrentUserData();

        //显示当前用户的离线消息   个人聊天信息或者群组消息
        if (responsejs.contains("offlinemsg"))
        {
            vector<string> vec = responsejs["offlinemsg"];
            for(string &str : vec)
            {
                //反序列化
                nlohmann::json js = nlohmann::json::parse(str);
                // time + [id] + name + "said: " + xxx
                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                {
                    std::cout<< js["time"] << "[" << js["id"] << "]" << js["name"] 
                                << "said: " << js["msg"] << std::endl;
                }
                else
                {
                    // 群消息[groupid] + time + [id] + name + "said: " + xxx
                    std::cout<< "群消息[" << js["groupid"] << "]" << js["time"] << " ["
                        << js["id"] << "]" << js["name"] << "said: " <<js["msg"] << std::endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

//显示当前登录成功的用户基本信息
void showCurrentUserData()
{
    std::cout<< "============================Login user=================================" <<std::endl;
    std::cout<< "current login user => id: " << g_currentUser.getId() << "name: " << g_currentUser.getName()<<std::endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            std::cout<< user.getId() << " " << user.getName() << " " << user.getState() <<std::endl;
        }
    }
    std::cout<< "----------------------------group list-----------------------------------" <<std::endl;
    if(!g_currentUserGroupList.empty())
    {
        for(Group &group : g_currentUserGroupList)
        {
            std::cout<<group.getId() << " " << group.getName() << " " << group.getDesc() << " " <<std::endl;
            for(GroupUser &user: group.getUsers())
            {
                std::cout<< user.getId() <<" " << user.getName() << " " << user.getState()
                    <<" " << user.getRole() << std::endl;
            }
        }
    }
    std::cout<< "=====================================================" << std::endl;
}

//"help"  command handler
void help(int fd=0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);

//系统支持的客户端命令列表
unordered_map<string, string> commandMap ={
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend","添加好友,格式addfriend:friendid"},
    {"creategroup","创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组,格式addgroup:groupid"},
    {"groupchat","群聊,格式groupchat:groupid:message"},
    {"loginout","注销,格式loginout"}
};

//系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap ={
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout",loginout}
};

//主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; //存储命令
        int idx = commandbuf.find(":");
        if(-1 == idx)
        {
            command = commandbuf;  // help和logout命令没有:
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        std::unordered_map<std::string, 
            std::function<void (int, std::string)>>::iterator it = commandHandlerMap.find(command);
        if( it == commandHandlerMap.end())
        {
            std::cerr << "invalid input command" <<std::endl;
            continue;
        }
        it->second(clientfd, commandbuf.substr(idx+1, commandbuf.size() - idx)); // 调用命令的方法
    }
}

//help  command handler
void help(int, string)
{
    std::cout<< "show command list >>>" <<std::endl;
    for(auto &p : commandMap)
    {
        std::cout << p.first << ": " <<p.second <<std::endl;
    }
    std::cout<<endl;
}

string getCurrentTime();

//"chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if(-1 == idx)
    {
        std::cerr << "chat command invalid!" << std::endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    std::string message = str.substr(idx +1, str.size() - idx);

    nlohmann::json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["toid"] = friendid;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1==len)
    {
        std::cerr << "send chat msg error, msg is: " << buffer << std::endl;
        return;
    }
}

// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    nlohmann::json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["friendid"] = friendid;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);

    if(-1 == len)
    {
        std::cerr<<"send addfriend msg error, msg is:" << buffer << std::endl; 
        return;
    }
}

// "creategroup" command handler
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        std::cerr << "creategroup command invalid!"<<std::endl;
        return;
    }
    std::string groupname = str.substr(0,idx);
    std::string groupdesc = str.substr(idx+1, str.size()-idx);

    nlohmann::json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1,0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error, msg is:" << buffer << endl;

    }
}

// "addgroup" command handler
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());

    nlohmann::json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["groupid"] = groupid;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1,0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error, msg is:" << buffer << endl;
    }
}

// "groupchat" command handler
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        std::cerr << "groupchat command invalid!"<<std::endl;
    }
    int groupid = atoi(str.substr(0,idx).c_str());
    std::string message = str.substr(idx + 1, str.size() - idx);

    nlohmann::json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["groupid"] = groupid;
    js["id"] = g_currentUser.getId();
    js["msg"] = message;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1,0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error, msg is:" << buffer << endl;
    }
}

// "loginout" command handler
void loginout(int clientfd, string)
{
    nlohmann::json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);

    if(-1==len)
    {
        cerr << "send loginout msg error, msg is:" << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

//获取系统时间（聊天信息需要添加时间信息
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}
