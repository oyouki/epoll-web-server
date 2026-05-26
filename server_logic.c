#include "fd_hash.c"
#include "tool.c"
#include "ser_cli_std.h"

/*
    自定义长度前缀协议:
            先在server.c中定义PREFIX_LENGTH常量,取值范围1~4,表示长度前缀的长度
        消息开头应当是长度为PREFIX_LENGTH的长度前缀,采用大端序(MSB在低索引),
        其后紧跟长度等同于长度前缀数值的有效载荷.
            服务器与客户端,服务器与子进程的双向通讯皆采用此格式
        [LENGTH_PREFIX][CMD_CODE][CONTEXT]
*/
#ifndef SERVER_LOGIC_H
#define SERVER_LOGIC_H

int server_parse_msg(char *buf,size_t *len,FdContext *fdc,FdContext **cli_fdc);    
//形参buf传入载荷信息地址,len传入载荷长度.用buf和buflen将组装完成的报文以及长度返回
//如果发送目标是client,用cli_fdc返回对应fdc指针
//返回值 -1:非法指令  0:传给fdc对应client  1:传给db 2:传给upload进程    3:传给download进程

int work_client_download(char *buf,size_t len);
void work_ser2db_reg(char*buf,size_t *len,FdContext *fdc);
void work_ser2cli_reg_fail(char *buf,size_t *len,FdContext **cli_fdc);

void work_ser2db_reg(char*buf,size_t *len,FdContext *fdc){
    fdc->client.status=S_CLI_REG;
    memcpy(&fdc->client.task_buf[fdc->client.task_buf_pos],&buf[CMD_CODELEN],AC_LEN+PW_LEN);
    fdc->client.task_buf_pos+=AC_LEN+PW_LEN;
    set_cmd(buf,CMD_SER2DB_REG);
    *len=AC_LEN+PW_LEN+CMD_CODELEN;
    memcpy(buf+*len,&fdc,sizeof(void*));   //???查询请求者的fdc最好考虑字节序
    *len+=sizeof(void*);
    build_lpmsg_inplace(buf,*len,PREFIX_LENGTH);
    *len+=PREFIX_LENGTH;
}

void work_ser2cli_reg_fail(char *buf,size_t *len,FdContext **cli_fdc){
    memcpy(cli_fdc,buf+CMD_CODELEN,sizeof(void*));  //提取目标fdc
    (*cli_fdc)->client.task_buf_pos=0;
    (*cli_fdc)->client.status=S_CLI_CONN;
    set_cmd(buf,CMD_SER2CLI_REG_FAIL);
    *len=CMD_CODELEN;
    build_lpmsg_inplace(buf,*len,PREFIX_LENGTH);
    *len+=PREFIX_LENGTH;
}

void work_ser2cli_reg_suc(char *buf,size_t *len,FdContext **cli_fdc){
    memcpy(cli_fdc,buf+CMD_CODELEN,sizeof(void*));
    (*cli_fdc)->client.task_buf_pos=0;
    (*cli_fdc)->client.status=S_CLI_CONN;
    set_cmd(buf,CMD_SER2CLI_REG_SUC);
    *len=CMD_CODELEN;
    build_lpmsg_inplace(buf,*len,PREFIX_LENGTH);
    *len+=PREFIX_LENGTH;
}

int server_parse_msg(char *buf,size_t *len,FdContext* fdc,FdContext **cli_fdc){   //载荷:buf  len:载荷长度
    int cmd=get_cmd(buf);
    switch(cmd){
        case CMD_CLI2SER_REG:
            if(fdc->client.status!=S_CLI_CONN)  return -1;
            work_ser2db_reg(buf,len,fdc);
            return 1;
            break;
        case CMD_CLI2SER_LOGIN:         //client身份认证

            return 1;
            break;
        case CMD_CLI2SER_GETINFO:         //client获取信息
            break;
        case CMD_CLI2SER_DOWNLOAD:         //client下载
            //此处需要读取文件编号,以及文件路径和编号的映射转换
        case CMD_DB2SER_REG_FAIL:
            work_ser2cli_reg_fail(buf,len,cli_fdc);
            return 0;
        case CMD_DB2SER_REG_SUC:
            work_ser2cli_reg_suc(buf,len,cli_fdc);
            return 0;
        default:
            ;
    }
    return -1;
}

#endif