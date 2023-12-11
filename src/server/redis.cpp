#include "redis.hpp"
#include<iostream>

Redis::Redis()
    :_publish_context(nullptr), _subscribe_context(nullptr)
{
}

Redis::~Redis()
{
    if(_publish_context !=nullptr)
    {
        redisFree(_publish_context);
    }

    if(_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

//连接服务器
bool Redis::connect()
{
    //负责publish消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if(_publish_context ==nullptr)
    {
        std::cerr << "connect redis failed!" <<std::endl;
        return false;
    }
    //负责subscribe消息的上下连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if(nullptr == _subscribe_context)
    {
        std::cerr << "connect redis failed" << std::endl;
        return false;
    }

    //上单独的线程中监听通道上的事件，有消息给业务层进行上报
    std::thread t([&](){
        observer_channel_message();
    });
    t.detach();
    std::cout<< "connect redis-server success!" <<std::endl;

    return true;
}
// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, std::string message)
{
    redisReply *reply = (redisReply*)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if(nullptr == reply)
    {
        std::cerr << "publish command fail!";
        return false;
    }
    std::cout<< "channel:" << channel << "成功发布消息： " << message <<std::endl;
    freeReplyObject(reply);
    return true;
}
// 向指定的channel订阅消息
bool Redis::subscribe(int channel)
{
    //SUBSCRIBE 命令本身会造成线程阻塞等待通道里发生消息，这里只做订阅通道，不接收通道消息
    //通道消息的接收专门在observer_channel_message函数中独立运行
    //只负责发送命令，不阻塞接收redis server相应消息，否则和notifyMsg线程抢占相应资源。
    if(REDIS_ERR == redisAppendCommand(_subscribe_context, "SUBSCRIBE %d", channel))
    {
        std::cerr << "subscribe command failed!" <<std::endl;
        return false;
    }

    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done=0;
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(_subscribe_context, &done))
        {
            std::cerr << "subscribe command failed!" <<std::endl;
            return false;
        }
    }
    std::cout<<  "订阅通道：" << channel << "成功" << std::endl;
    return true;
}

//向指定channel取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if(REDIS_ERR == redisAppendCommand(_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        std::cerr << "unsubscribe command failed!" << std::endl;
        return false;
    }
    int done = 0;
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(_subscribe_context, &done))
        {
            std::cerr << "unsubscribe command failed!" <<std::endl;
            return false;
        }
    }
    return true;
}

//在独立线程中接收订阅通道的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    std::cout<<"在独立线程中接收订阅通道的消息" << std::endl;
    while(REDIS_OK== redisGetReply(_subscribe_context, (void**)&reply))
    {
        std::cout<<"从订阅的通道收到消息的格式是： 一个带三元素的数组" << std::endl;
        //从订阅的通道收到消息的格式是： 一个带三元素的数组
        if(reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            //给业务层上报通道发生的消息
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
            std::cout<<"成功向业务层上报消息" << "atoi(reply->element[1]->str): " <<reply->element[1]->str <<std::endl;
            std::cout<<"成功向业务层上报消息" << "reply->element[2]->str "<< reply->element[2]->str <<std::endl;
        }
        freeReplyObject(reply);
    }
    std::cerr <<"=================== observer_channel_message quit ======================"<<std::endl;
}

//初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(std::function<void(int, std::string)> func)
{
    _notify_message_handler = func;
}