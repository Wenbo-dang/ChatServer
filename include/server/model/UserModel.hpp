#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"
using namespace std;

class UserModel
{
    public:
        //用户表的增加方法
        bool insert(User &user);
        
        //根据用户号码查询用户信息
        User query(int id);

        //更新用户信息
        bool updateState(User user);

        //重置用户状态信息
        void resetState();
};

#endif