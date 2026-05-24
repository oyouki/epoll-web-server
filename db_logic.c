#include <mysql/mysql.h>
#include <openssl/sha.h>
#include "fd_hash.c"
#include "tool.c"
#include "ser_cli_std.h"

//去除长度前缀的载荷结构:[CMD(1 Byte)][...]

void get_pwhash(char* pwd,unsigned char *digest);
int db_parse_msg(char *buf,size_t *len,int(**func)(char*,size_t*,MYSQL*));
//解读消息并组装信息,返回-1:解析失败 0:查询 1:发给server
//用func返回下步所需处理函数的指针
int work_regis(char *buf,size_t* len,MYSQL* conn);  //工作函数正常都返回0
int work_login(char *buf,size_t* len,MYSQL* conn);
void get_regis_info(char *buf,char* acc,char* pwd);      //获取注册所需的ac和pw

void get_pwhash(char* pwd,unsigned char *digest){
    SHA256(pwd,PW_LEN,digest);
}

void get_regis_info(char *buf,char* acc,char* pwd){
    memcpy(acc,&buf[CMD_CODELEN],AC_LEN);
    memcpy(pwd,&buf[CMD_CODELEN+AC_LEN],PW_LEN);
}

int work_regis(char *buf,size_t* len,MYSQL* conn){
    MYSQL_RES *res; MYSQL_ROW row; int row_num=0;
    char acc[AC_LEN],pwd[PW_LEN];unsigned char pw_hash[SHA256_DIGEST_LENGTH];
    //有问题,AC_LEN和PW_LEN是账号密码本体长度
    get_regis_info(buf,acc,pwd);
    get_pwhash(pwd,pw_hash);
    char query[DB_TMP_BUFLEN];
    snprintf(query,DB_TMP_BUFLEN,"SELECT account,pw_hash FROM user_accounts WHERE account = '%s'",acc);
    if(mysql_query(conn,query)){
        fprintf(stderr,"work_regis mysql_query err\n");
    }
    res=mysql_store_result(conn);   //???有可能res==NULL!!!
    row_num=mysql_num_rows(res);
    if(row_num>0){    //存在账号,注册失败
        mysql_free_result(res);
        set_cmd(buf,CMD_DB2SER_REG_FAIL);
        build_lpmsg_inplace(buf,CMD_CODELEN,PREFIX_LENGTH);
        *len=CMD_CODELEN+PREFIX_LENGTH;
        return 0;
    }else{      //注册
        mysql_free_result(res);
        snprintf(query,DB_TMP_BUFLEN,"INSERT INTO user_accounts \
            (account,pw_hash)\nVALUES('%s','%s');",acc,pw_hash);
        if((mysql_query(conn,query))){
            fprintf(stderr,"work_regis mysql_query err\n");
            return -1;
        }
        snprintf(buf,DB_TMP_BUFLEN,"%c",CMD_DB2SER_REG_SUC);
        build_lpmsg_inplace(buf,CMD_CODELEN,PREFIX_LENGTH);
        *len=CMD_CODELEN+PREFIX_LENGTH;
        return 0;
    }
}

int work_login(char *buf,size_t* len,MYSQL* conn){
    MYSQL_RES *res; MYSQL_ROW row; int row_num=0;
    char acc[AC_LEN],pwd[PW_LEN],pw_hash[SHA256_DIGEST_LENGTH];
    get_regis_info(buf,acc,pwd);
    get_pwhash(pwd,pw_hash);
    char query[DB_TMP_BUFLEN];
    snprintf(query,DB_TMP_BUFLEN,"SELECT account,pwhash FROM user_accounts WHERE account = '%s'\
         AND pwhash = '%s';",acc,pw_hash);
    if(mysql_query(conn,query)){
        fprintf(stderr,"work_regis mysql_query err\n");
    }
    res=mysql_store_result(conn);
    row_num=mysql_num_rows(res);
    if(row_num==0){     //账号或密码错误
        snprintf(buf,DB_TMP_BUFLEN,"%c",CMD_DB2SER_LOGIN_FAIL);
        build_lpmsg_inplace(buf,CMD_CODELEN,PREFIX_LENGTH);
        *len=CMD_CODELEN+PREFIX_LENGTH;
        return 0;
    }else if(row_num==1){   //登录成功
        snprintf(buf,DB_TMP_BUFLEN,"%c",CMD_DB2SER_LOGIN_SUC);
        build_lpmsg_inplace(buf,CMD_CODELEN,PREFIX_LENGTH);
        *len=CMD_CODELEN+PREFIX_LENGTH;
        return 0;
    }
    return 1;   //异常
}

int db_parse_msg(char *buf,size_t *len,int(**func)(char*,size_t*,MYSQL*)){
    int cmd;
    cmd=get_cmd(buf);
    switch(cmd){
        case CMD_SER2DB_REG:
            *func=work_regis;
            return 0;
        case CMD_SER2DB_LOGIN:
            *func=work_login;
            return 0;
    }
    return -1;
}