#定义一个SRC——LIST变量，包含该目录下所有的源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./model MODEL_LIST)
aux_source_directory(./redis REDIS_LIST)

#指定生成可执行文件
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST})
#制定可执行文件需要依赖的库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient pthread hiredis)

message(STATUS "SRC_LIST: ${SRC_LIST}")
message(STATUS "DB_LIST: ${DB_LIST}")
message(STATUS "MODEL_LIST: ${MODEL_LIST}")
message(STATUS "REDIS_LIST: ${REDIS_LIST}")
