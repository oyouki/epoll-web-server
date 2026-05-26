/*Common Libs*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include "uthash.h"

#ifndef SER_CLI_STD_H
#define SER_CLI_STD_H

//database.c
#define DB_RD_BUFLEN   256      //数据库进程读取缓冲
#define DB_TMP_BUFLEN  256      //数据库临时缓冲
//client.c
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
//fd_hash.c
#define SERVER_FDC_CLI_RD_BUFLEN 128    //服务器进程fdcontext中client读取缓冲
#define SERVER_FDC_CLI_TSK_BUFLEN 128
#define SERVER_FDC_IPC_RD_BUFLEN 128           //服务器进程fdc中IPC读取缓冲
#define SERVER_FDC_IPC_TSK_BUFLEN 128
//server.c
#define LOCAL_IPADDR "127.0.0.1"
#define LOCAL_PORT 8080
#define EPOLLWAIT_MAXEVENTS 5
#define SERVER_HANDLE_TMP_BUFLEN 64
#define CLI_STO_DIR "~/client_storage_directory/"
//tool.c
#define PREFIX_LENGTH 4

//发送账号密码时需要在末尾以\x00结尾
#define AC_LEN 11
#define PW_LEN 11

//指令码定义
#define CMD_CODELEN 1

#define CMD_CLI2SER_REG 0
#define CMD_CLI2SER_LOGIN 1
#define CMD_CLI2SER_GETINFO 2
#define CMD_CLI2SER_DOWNLOAD 3
#define CMD_SER2DB_REG 4
#define CMD_SER2DB_LOGIN 5
#define CMD_DB2SER_REG_FAIL 6
#define CMD_DB2SER_REG_SUC 7
#define CMD_DB2SER_LOGIN_FAIL 8
#define CMD_DB2SER_LOGIN_SUC 9
#define CMD_DB2SER_ILLCMD 10    //非法命令
#define CMD_SER2CLI_REG_FAIL 11
#define CMD_SER2CLI_REG_SUC 12

void set_cmd(char* buf,int cmd);
int get_cmd(char* buf);

#endif