#include <mysql/mysql.h>
#include "db_logic.c"
#include "fd_hash.c"
#include "tool.c"
#include "ser_cli_std.h"

#define USERNAME "db_process"
#define PASSWD "7u3_lKxa38."
#define DATABASE_NM "server_db"

/*
    database进程
    主进程fork后,将unix_pipe读端连接到stdin,写端连接到stdout
    然后调用exec函数启动本进程
*/

int main(){
    MYSQL *conn=mysql_init(NULL);
    setvbuf(stdin,NULL,_IONBF,0);   //设置stdin(实际为unix socket)不缓冲
    int unfd=dup(STDIN_FILENO);
    if(!conn){
        fprintf(stderr,"mysql_init failed\n");
        return -1;
    }
    if (mysql_real_connect(conn,\
        "localhost",USERNAME,PASSWD,DATABASE_NM,0,NULL,0)==NULL) {
        fprintf(stderr, "连接失败: %s\n", mysql_error(conn));
        exit(1);
    }
    if(mysql_set_character_set(conn,"utf8")){
        fprintf(stderr,"mysql_set_charset err\n");
    }

    char buf[DB_RD_BUFLEN]={0};size_t buf_pos=0;
    char tmp_buf[DB_TMP_BUFLEN]; size_t tmp_pos=0;
    int r=-2;
    printf("数据库进程就绪,开始循环.\n");
    while(1){
        tmp_pos=read(unfd,tmp_buf,DB_TMP_BUFLEN);   //阻塞读
        /*测试*/
        for(int i=0;i<tmp_pos;++i){
            putchar(tmp_buf[i]);
        }
        printf("\n");
        if(tmp_pos==0){
            perror("unix_socket may shutdown");
            exit(1);
        }
        if(tmp_pos==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){  //读完或不可读
                ;   //不用做
            }else{
                perror("db read err");
                return -1;
            }
        }
        memcpy(&buf[buf_pos],tmp_buf,tmp_pos);
        buf_pos+=tmp_pos;
        int br=0,r,aim=-1; int(*func)()=NULL;  //br:连续跳出标记 r:读取结果 aim:处理方式 func:所需函数
        while(1){
            tmp_pos=DB_TMP_BUFLEN;
            r=rd_lpmsg_buf(buf,&buf_pos,PREFIX_LENGTH,tmp_buf,&tmp_pos);
            switch(r){
                case 1:     //无可解析信息
                    br=1;
                    break;
                case -1:
                    errno=EINVAL;
                    perror("main-while-while rd_lpmsg_buf error");
                    snprintf(buf,DB_TMP_BUFLEN,"%c",CMD_DB2SER_ILLCMD);
                    build_lpmsg_inplace(buf,CMD_CODELEN,PREFIX_LENGTH);
                    buf_pos=CMD_CODELEN+PREFIX_LENGTH;
                    write(unfd,tmp_buf,tmp_pos);    //回报:非法命令
                    break;
                case 0:     //正常读到一条,处理
                    aim=db_parse_msg(tmp_buf,&tmp_pos,&func);
                    if(aim==0){     //查询类
                        if(func(tmp_buf,&tmp_pos,conn)){    //使用func查询
                            errno=EINVAL;
                            perror("func() err");
                            return -1;
                        }
                        write(unfd,tmp_buf,tmp_pos);    //通过unfd返回结果到主进程
                    }
                    if(aim==1){     //非查询类
                        /*   对DB的指令   */
                    }
                    break;
                default:
                    ;
            }
            if(br==1)   break;
        }
    }

    mysql_close(conn);
    return 0;
}