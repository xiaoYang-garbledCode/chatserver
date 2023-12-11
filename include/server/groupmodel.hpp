#pragma once
#include"group.hpp"
#include<string>
#include<vector>
using namespace std;
//维护群组信息的操作接口方法
class GroupModel
{
public:
    bool createGroup(Group &group);
    void addGroup(int userid, int groupid, string role);

    //查询用户所在的组
    vector<Group> queryGroups(int userid);
    //根据指定的groupid查询用户id列表
    vector<int> queryGroupUsers(int userid, int groupid);

};