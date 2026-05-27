### epoll并发服务器框架
project/
├── include/
│   ├── ser_cli_std.h
│   └── tool.h
├── src/
│   ├── fd_hash.c
│   ├── tool.c
│   ├── server.c
│   ├── server_logic.c
│   └── db_logic.c
│   ├── db.c
│   ├── client.c
└── README.md

    服务器使用一个epoll对象统一管理listen socket、client socket、子进程IPC————Unix Domain Socket。
服务器主进程启动后为所有套接字和后续accept所得套接字(设置为非阻塞)分配保存状态、说明信息、读写缓冲的结构体FdContext，
并使用现成的uthash.h库保存到哈希表，随后将FdContext的指针随着套接字文件描述符以ET模式注册到epoll。
    所有套接字之间的通信遵循以下自定义长度前缀协议：[LENGTH_PREFIX(长度前缀)][CMD(指令)][payload(信息载荷)]
    服务器主循环使用epoll_wait等待事件。套接字的EPOLLIN事件返回时，持续读到对应FdContext中的缓冲区，直到
EAGAIN/EWOULDBLOCK。内层循环中，使用能够处理半包粘包的提取函数，尝试从缓冲区读出完单条整条去掉长度前缀的信息。信息
连同套接字信息(FdContext)传递到parse解析函数。内层循环不断使用提取函数直到无法提取。
    解析函数parse根据指令和套接字信息，调用合适的工作函数检查和设置状态，并处理载荷内容并把结果封装、原位回填到实参
数组，并处理结果和发送目标到parse函数。parse函数带着结果再返回到主循环。服务器直接根据目标发送结果报文。
    如果服务器发送不全，则将此次发送任务剩余部分和等待发送状态回写到FdContext中，新增EPOLLOUT注册到epoll中。

当前完成数据库查询子进程的逻辑，上传与下载数据功能开发中
