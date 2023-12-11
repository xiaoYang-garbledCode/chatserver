#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    chatService::instance()->reset();
}

int main(int argc, char **argv)
{
    // 解析通过命令行参数传递的ip和port
    // EventLoop loop;
    // InetAddress addr("127.0.0.1", 6000);
    // ChatServer server(&loop, addr, "ChatServer");
    // server.start();
    // loop.loop();
    // return 0;
    if(argc<3)
    {
        std::cerr << "command invalid example ./ChatServer 127.0.0.1 6000" <<std::endl;
        exit(-1);
    }

    //解析命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    signal(SIGINT, resetHandler);

    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");
    server.start();
    loop.loop();
    return 0;
}