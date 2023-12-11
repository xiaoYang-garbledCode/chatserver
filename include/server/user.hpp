#pragma once

#include<string>
using namespace std;

class User
{
public:
    User(int id =-1, string name = "", string password = "", string state= "offline")
    {
        this->id = id;
        this->name = name;
        this->password = password;
        this->state = state;
    }

    //Get方法
    int getId() { return this->id;}
    string getName() { return this->name; }
    string getPwd() { return this->password;}
    string getState() { return this->state;}

    //set方法
    void setId(int id) { this->id = id;}
    void setName(string name) { this->name = name; }
    void setPwd(string password) { this->password = password; }
    void setState(string state) { this->state = state; }

private:
    int id;
    string name;
    string password;
    string state;
};