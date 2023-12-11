#pragma once
#include<string>
#include<vector>
#include"groupuser.hpp"

using namespace std;

//Group表的ORM类
class Group
{
public:
    Group(int groupid=-1, string groupname="", string groupdesc="")
    {
        this->groupid = groupid;
        this->groupname = groupname;
        this->groupdesc = groupdesc;
    }

    int getId() { return this->groupid;}
    string getName() { return this->groupname;}
    string getDesc() { return this->groupdesc;}

    void setId(int id) { this->groupid = id;}
    void setName(string name) { this->groupname = name;}
    void setDesc(string desc) { this->groupdesc = desc; }

    vector<GroupUser> &getUsers() { return this->users; }

private:
    int groupid;
    string groupname;
    string groupdesc;
    vector<GroupUser> users;
};