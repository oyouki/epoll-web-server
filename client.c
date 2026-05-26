#define _GNU_SOURCE
#include "tool.c"
#include "ser_cli_std.h"

#define CLI_ARG_BUFLEN 100
#define CLI_TMP_BUFLEN 256

int str_to_cmd(char*s){
    if(!strcmp(s,"reg"))    return CMD_CLI2SER_REG;
}

int main(){
    int fd;
    in_port_t port=htons(SERVER_PORT);
    struct in_addr ipd; struct sockaddr addr;
    inet_pton(AF_INET,SERVER_IP,&ipd);
    if(set_sockaddr(AF_INET,&ipd,&port,&addr))  perror("set sockaadr failed.");
    char cmd[CLI_ARG_BUFLEN],arg1[CLI_ARG_BUFLEN],arg2[CLI_ARG_BUFLEN];
    char buf[CLI_TMP_BUFLEN];ssize_t len=0;
    int cmd_code=-1;int arg_num=0;

    if((fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))==-1)    perror("socket func error.");
    if(connect(fd,&addr,sizeof(struct sockaddr)))    perror("connect failed.");
    //
    while(1){
        len=0;memset(arg1,0,CLI_ARG_BUFLEN);memset(arg2,0,CLI_ARG_BUFLEN);
        arg_num=fscanf(stdin,"%s %s %s",cmd,arg1,arg2);
        cmd_code=str_to_cmd(cmd);
        switch(cmd_code){
            case CMD_CLI2SER_REG:
                if(arg_num!=3||strlen(arg1)>AC_LEN||strlen(arg2)>PW_LEN){   //本体长度
                    errno=EINVAL; perror("input cmd invalid"); break;
                }
                set_cmd(buf,CMD_CLI2SER_REG);
                len+=CMD_CODELEN;
                memcpy(buf+len,arg1,AC_LEN);
                len+=AC_LEN;
                memcpy(buf+len,arg2,PW_LEN);
                len+=PW_LEN;
                build_lpmsg_inplace(buf,len,PREFIX_LENGTH);
                len+=PREFIX_LENGTH;
                if(send(fd,buf,len,0)==-1){
                    perror("main-send err");
                }
                if((len=recv(fd,buf,CLI_TMP_BUFLEN,0))==-1){
                    perror("main-recv err");
                }
                switch(buf[4]){
                    case CMD_SER2CLI_REG_FAIL:
                        printf("REG FAILED\n");
                        break;
                    case CMD_SER2CLI_REG_SUC:
                        printf("REG SUCCESS\n");
                        break;
                }
        }
    }
    return 0;
}