#include "ser_cli_std.h"

#ifndef FD_HASH_H
#define FD_HASH_H

enum fd_type    {SERVER_SOCKET,CLIENT_SOCKET,IPC_FD};
enum client_status  {S_CLI_CONN,S_CLI_REG,S_CLI_STANDBY,S_CLIENT_BUSY,S_CLI_SENDING};
//
enum IPC_type   {DATABASE_UN,UPLOAD_UN,DOWNLOAD_UN};

/*
HASH_ADD_INT(head,id,p)     head:表头  id:字段名  p:待插入指针
HASH_FIND_INT(head,target_id,p)     targer_id:查找值的地址  p:查找结果
*/

typedef struct{
    enum fd_type type;
    int fd;
    union {
        struct client{
            uint32_t id;
            struct sockaddr sockaddr;
            enum client_status status;          //状态
            char rd_buf[SERVER_FDC_CLI_RD_BUFLEN];      //读取缓冲
            size_t rd_buf_pos;
            char task_buf[SERVER_FDC_CLI_TSK_BUFLEN];    //任务缓冲
            size_t task_buf_pos;
        }client;
        void* server;
        struct {
            enum IPC_type type;
            char buffer[IPC_FDC_RD_BUFLEN];
            size_t pos;
        }IPC;
    };
    UT_hash_handle hh;
}FdContext;

FdContext* fd_hash_find(int fd);
int fd_hash_add(int fd,FdContext *s);
void fd_hash_remove(int fd);


FdContext * state_hashmap;

FdContext* fd_hash_find(int fd){
    FdContext *s=NULL;
    HASH_FIND_INT(state_hashmap,&fd,s);
    return s;
}

int fd_hash_add(int fd,FdContext *s){
    if(fd_hash_find(fd)){
        return 1;
    }
    HASH_ADD_INT(state_hashmap,fd,s);
    return 0;
}


void fd_hash_remove(int fd){
    FdContext *s=fd_hash_find(fd);
    if(s){
        HASH_DEL(state_hashmap,s);
        free(s);
    }
}

#endif