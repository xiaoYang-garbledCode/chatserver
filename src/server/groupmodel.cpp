#include "groupmodel.hpp"
#include "db.h"
bool GroupModel::createGroup(Group &group)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into AllGroup(groupname, groupdesc) values('%s','%s')",
            group.getName().c_str(), group.getDesc().c_str());
    Mysql mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            group.setId(mysql_insert_id(mysql.getconnection()));
            return true;
        }
    }
    return false;
}
void GroupModel::addGroup(int userid, int groupid, string role)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into GroupUser(groupid,userid,grouprole) values(%d,%d,'%s')",
            groupid, userid,role.c_str());
    Mysql mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户所在的群组信息（即用户所属的所有群，以及每个群中的用户信息）
// 先根据userid在groupuser表中查出该用户所属的群组信息
// 在群组信息，查询属于该群组的所有用户的userid，并与user表进行多表联合查询，查出用户的详细信息。

vector<Group> GroupModel::queryGroups(int userid)
{
    char sql[1024] = {0};
    // 查出当前userid对应的群组信息
    sprintf(sql, "select a.id, a.groupname, a.groupdesc from AllGroup a inner join GroupUser b on \
                a.id = b.groupid where b.userid=%d",
            userid);
    Mysql mysql;
    vector<Group> groupVec;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 查询该群组下所有用户信息
    for (Group &group : groupVec)
    {
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from User a \
            inner join GroupUser b on b.userid = a.id where b.groupid=%d",
                group.getId());
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return groupVec;
}

// 根据指定的groupid查询用户id列表
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    char sql[1024] = {0};
    // 查出当前userid对应的群组信息
    sprintf(sql, "select userid from GroupUser where groupid=%d and userid !=%d", groupid, userid);
    vector<int> idVec;
    Mysql mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                idVec.push_back(atoi(row[0]));
            }
        }
        mysql_free_result(res);
    }
    return idVec;
}