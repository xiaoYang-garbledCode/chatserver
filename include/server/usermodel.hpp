#pragma once

#include"user.hpp"

//uer表的数据操作类
class userModel{
public:
    //添加方法
    bool insert(User &user);

    //根据用户id查询用户信息
    User query(int userid);

    //更新用户状态信息
    bool updateState(User user);

    //重置用户状态
    void resetState();
};