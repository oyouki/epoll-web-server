#include "ser_cli_std.h"
#include "fd_hash.c"

#ifndef TOOL_H
#define TOOL_H

int set_sockaddr(int,void*,void*,struct sockaddr*); //IP和端口已经是网络字节序
void fdhash_dump(void);
int format_sockaddr(struct sockaddr *addr,char *buf);
int rd_lpmsg_buf(char *src_buf,size_t *src_pos,size_t preflen,\
    char *des_buf,size_t *des_buflen);              
    //从储存长度前缀包的缓冲区读取单条载荷  函数(源地址,&数据长度,前缀长度,目标地址,&目标地址容量)
    //包含pos调整操作,必须同时传入消息缓冲src_buf和位置指针pos
    //函数返回前用*des_buflen返回实际读取量;返回值:-1 错误 0 成功 1 条件不足
void build_lenpref(size_t payload_len,size_t preflen,char* buffer); //构造长度前缀
void pack_msg(char* pref,size_t preflen,char *payload,size_t payload_len);    //组装长度前缀到信息载荷
void build_lpmsg_inplace(char *payload,size_t len,size_t preflen);      //原位生成长度前缀

void build_lpmsg_inplace(char *payload,size_t payload_len,size_t preflen){
    char lp_buf[preflen];
    build_lenpref(payload_len,preflen,lp_buf);
    pack_msg(lp_buf,preflen,payload,payload_len);
}

void pack_msg(char* pref,size_t preflen,char *payload,size_t payload_len){
    memmove(payload+preflen,payload,payload_len);
    memcpy(payload,pref,preflen);
}

void build_lenpref(size_t payload_len,size_t preflen,char* buffer){
    size_t msg_len=payload_len+preflen;int shift;
    for(size_t i=0;i<preflen;++i){
        shift=8*(preflen-1-i);
        buffer[i]=(msg_len>>shift)&0xff;
    }
}

int set_sockaddr(int domain,void *ip,void *port,struct sockaddr *addr){
    if(domain==AF_INET){
        memset(addr,0,sizeof(struct sockaddr_in));  //初始化sockaddr
        struct sockaddr_in *ipv4_addr=(struct sockaddr_in*)addr;
        ipv4_addr->sin_family=AF_INET;
        memcpy(&ipv4_addr->sin_addr.s_addr,ip,sizeof(in_addr_t));
        memcpy(&ipv4_addr->sin_port,port,sizeof(in_port_t));
        return 0;
    }
    if(domain==AF_INET6){
        memset(addr,0,sizeof(struct sockaddr_in6));
        struct sockaddr_in6 *ipv6_addr=(struct sockaddr_in6*)addr;
        ipv6_addr->sin6_family=AF_INET6;
        memcpy(&ipv6_addr->sin6_addr,ip,16*sizeof(uint8_t));
        memcpy(&ipv6_addr->sin6_port,port,sizeof(in_port_t));
        return 0;
    }
    return -1;
}

void fdhash_dump(void){
    FdContext *p,*q;
    HASH_ITER(hh,state_hashmap,p,q){
        printf("fd = %d\t",p->fd);
        switch(p->type){
            case SERVER_SOCKET:
                printf("SERVER_SOCKET\n");
                break;
            case CLIENT_SOCKET:
                printf("CLIENT_SOCKET\n");
                printf("buffer:");
                for(int i=0;i<p->client.rd_buf_pos;++i){
                    putchar(p->client.rd_buf[i]);
                }
                putchar('\n');
                printf("rd_buf_pos = %u\tstatus:%d\n",(unsigned int)p->client.rd_buf_pos,p->client.status);
                char print_buf[29];
                format_sockaddr(&p->client.sockaddr,print_buf);
                printf("%s",print_buf);
                break;
            case IPC_FD:
                printf("IPC_FD\t");
                printf("type:%d\n",p->IPC.type);
                printf("buffer:");
                for(int i=0;i<p->IPC.pos;++i){
                    putchar(p->IPC.buffer[i]);
                }
                break;
            default:
        }
        putchar('\n');
    }
}

int format_sockaddr(struct sockaddr *addr,char *buf){   //buf容量>=29
    if(addr->sa_family==AF_INET){
        struct sockaddr_in *ad=(struct sockaddr_in*)addr;
        char ipbuf[INET_ADDRSTRLEN]={0}; in_port_t port=0;
        socklen_t len=INET_ADDRSTRLEN,r=0;
        inet_ntop(AF_INET,&ad->sin_addr,ipbuf,len);
        len=strlen(ipbuf);
        port=ntohs(ad->sin_port);   //完成ip和port的转换
        snprintf(buf,len+4,"IP:%s",ipbuf);
        len=len+3;  //如果len=len+4,则在"x.x.x.x \x00"之后写入'\t'
        buf[len++]='\t';
        r=snprintf(buf+len,7,"%s","port: "); len+=r;
        snprintf(buf+len,6,"%d",(unsigned short)port);
        return strlen(buf);
    }
    if(addr->sa_family==AF_INET6){
        struct sockaddr_in6 *ad=(struct sockaddr_in6*)addr;
        char ipbuf[INET6_ADDRSTRLEN]={0}; in_port_t port=0;
        socklen_t len=INET6_ADDRSTRLEN,r=0;
        inet_ntop(AF_INET6,&ad->sin6_addr,ipbuf,len);
        len=strlen(ipbuf);
        port=ntohs(ad->sin6_port);
        snprintf(buf,len+1,"%s",ipbuf);
        buf[len++]='\t';
        r=snprintf(buf+len,7,"%s","port: "); len+=r;
        snprintf(buf+len,6,"%d",(unsigned short)port);
        return strlen(buf);
    }
    return -1;
}

int rd_lpmsg_buf(char *src_buf,size_t *src_pos,size_t preflen,char *des_buf,size_t *des_buflen){
    if(preflen>4){
        errno=EINVAL ;return -1;               //不支持大于4字节长度前缀
    }
    size_t msg_length=0;
    if(*src_pos<preflen)  return 1;     //前缀不完整
    for(int i=preflen-1;i>=0;--i){
        msg_length+=src_buf[i]<<(8*(preflen-i-1));
    }
    size_t payload=msg_length-preflen;
    if(*src_pos<msg_length)   return 1;       //消息不完整
    if(*des_buflen<msg_length){
        errno=EINVAL ;return -1;      //输出buffer不够
    }
    memcpy(des_buf,src_buf+preflen,payload);
    memmove(src_buf,src_buf+msg_length,*src_pos-msg_length);
    *src_pos-=msg_length;
    *des_buflen=payload;
    return 0;
}

#endif