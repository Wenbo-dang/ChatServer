# ChatServer
可以工作在nginx tcp负载均衡环境中的集群聊天服务器和客户端 基于muduo库 使用redis中间件 使用mysql数据库
#编译方式
cd build
rm -f *
cmake ..
make

需要nginx负载均衡，redis 、mysql 使用json第三方库

在bin中存放 ChatServer ChatClient可执行文件

