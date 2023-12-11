#include"usermodel.hpp"
#include"db.h"
#include<iostream>
using namespace std;

bool userModel::insert(User &user)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name, password, state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());
    Mysql mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            //获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getconnection()));
            return true;
        }
    }
    return false;
}

//根据用户id查询用户信息
User userModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select * from User where id=%d",userid);
    Mysql mysql;
    User user;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res !=nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            user.setId(atoi(row[0]));
            user.setName(row[1]);
            user.setPwd(row[2]);
            user.setState(row[3]);
            mysql_free_result(res);
            return user;
        }
    }
    //函数返回时优先返回一个临时对象，而不是一个定义过的对象。
    return User();
}

//更新用户状态信息
bool userModel::updateState(User user)
{
    char sql[1024] = {0};
    sprintf(sql, "update User set state='%s' where id=%d ",user.getState().c_str(),user.getId());
    Mysql mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            return true;
        }
    }
    return false;
}

//重置用户状态
void userModel::resetState()
{
    char sql[1024] = "update User set state = 'offline' where state = 'online'";
    Mysql mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}