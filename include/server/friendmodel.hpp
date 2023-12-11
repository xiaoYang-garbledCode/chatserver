#pragma once

#include "user.hpp"
#include<vector>

//维护好友信息的操作接口方法
class FriendModel
{
public:
    void insert(int userid, int friendid);

    vector<User> query(int userid);
};