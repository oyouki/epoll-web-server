#define __GNU_SOURCE__
#include "fd_hash.c"
#include "tool.c"
#include "server_logic.c"
#include "ser_cli_std.h"

int local_socket=0; //监听套接字
int epfd;   //epoll对象fd
struct epoll_event event,ev[EPOLLWAIT_MAXEVENTS]; //epoll事件与缓存
int unfd_db[2],unfd_up[2],unfd_down[2];     //所有IPC Unix socket

time_t t;   //时间戳

/*  处理逻辑   */

int handle_new_connect(int local_sockfd);       //主sockfd可读的处理
int handle_client_data(FdContext *fdc);         //client socket epollin的处理
int handle_IPC_data(FdContext *fdc);             //IPC socket epollin的处理
int server_init_IPC(int unfd,enum IPC_type type);     //设置IPC哈希
int server_init_listener(int local_socket);
void server_init_new_connect(FdContext *fdc,int cli_fd);

void server_init_new_connect(FdContext *fdc,int cli_fd){
    fdc->fd=cli_fd;
    fdc->client.id=0; fdc->client.rd_buf_pos=0;
    fdc->client.status=S_CLI_CONN; fdc->client.task_buf_pos=0;
    fdc->type=CLIENT_SOCKET;
}

int handle_new_connect(int local_sockfd){
    struct sockaddr addr;
    int fd,len=sizeof(struct sockaddr);
    while((fd=accept(local_sockfd,&addr,&len))!=-1){  //循环accept
        if(fd==-1){
            perror("accept problem");
            return -1;
        }
        if(fcntl(fd,F_SETFL,O_NONBLOCK)){   //新套接字无需先GETFL
            perror("handle_new_connect fcntl err");
        }   //设置客户端socket不阻塞
        FdContext *s=malloc(sizeof(FdContext));
        //设置fdc结构体
        server_init_new_connect(s,fd);
        memcpy(&s->client.sockaddr,&addr,sizeof(struct sockaddr));
        fd_hash_add(fd,s);       //插入哈希表
        //设置可读事件ET触发,插入epfd
        event.data.ptr=s,event.events=EPOLLIN|EPOLLET;
        if((epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&event))==-1){
            perror("newsock epoll add failed");
        }
    }
    return 0;
}

int handle_client_data(FdContext *fdc){
    //问题:如果存在一条信息长度>SERVER_HANDLE_TMP_BUFLEN,将会溢出
    while(1){       //连续recv直到阻塞或读到FIN或recv故障
        char tmp_buf[SERVER_HANDLE_TMP_BUFLEN]; ssize_t tmp_pos;int r=-2;
        tmp_pos=recv(fdc->fd,tmp_buf,SERVER_HANDLE_TMP_BUFLEN,0);   
        //注:一次recv最多读到MAX_MSG_SIZE长度信息
        if(tmp_pos==0){     //对端关闭写或全关
            int fd=fdc->fd;
            fd_hash_remove(fdc->fd);
            epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
            close(fd);
            return 0;
        }else if(tmp_pos==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){      //读完或阻塞
                break;
            }else{
                perror("handle client data/recv failed");
                return -1;
            }
        }//以下为tmp_pos>0,读到内容的分支
        memcpy(&fdc->client.rd_buf[fdc->client.rd_buf_pos],tmp_buf,tmp_pos);
        //???潜在问题:写入client buffer可能超过缓冲区长度
        fdc->client.rd_buf_pos+=tmp_pos;
        int br=0,aim=-2;
        while(1){       //连续提取载荷
            tmp_pos=SERVER_HANDLE_TMP_BUFLEN;
            r=rd_lpmsg_buf(fdc->client.rd_buf,&fdc->client.rd_buf_pos,PREFIX_LENGTH,\
                tmp_buf,&tmp_pos);
                switch(r){
                    case 1:         //缓冲区内没有可提取载荷
                        br=1;
                        break;
                    case -1:        //提取错误
                        perror("handle_client_data/rd_lpmsg_buf func error");
                        return -1;
                    case 0:         //success
                        aim=server_parse_msg(tmp_buf,&tmp_pos,fdc,NULL);    //来自client的请求不会传递给client
                        switch(aim){
                            case 1: //传给db
                                send(unfd_db[0],tmp_buf,tmp_pos,0);    //对IPC阻塞发送
                                break;
                            case 0: //传给client
                                ssize_t sdl=0;
                                sdl=send(fdc->fd,tmp_buf,tmp_pos,0);        //client_fd本就是非阻塞
                                if(sdl!=tmp_pos){
                                    if(sdl==-1||errno==EAGAIN){     //没发送
                                        fdc->client.status=S_CLI_SENDING;
                                        memcpy(&fdc->client.task_buf[fdc->client.rd_buf_pos],tmp_buf,tmp_pos);
                                        fdc->client.task_buf_pos+=tmp_pos;
                                        struct epoll_event ev = {0};
                                        ev.events|=EPOLLOUT|EPOLLIN|EPOLLET;
                                        ev.data.ptr=fdc;
                                        epoll_ctl(epfd,EPOLL_CTL_ADD,fdc->fd,&ev);  //等待发送
                                    }else{      //没发完

                                    }
                                }
                            default:;
                        }
                        break;
                    default:    ;
                }
            if(br==1)   break;
        }
    }
    return -1;
}

int handle_IPC_data(FdContext *fdc){
    while(1){
        char tmp_buf[SERVER_HANDLE_TMP_BUFLEN]; ssize_t tmp_pos;int r=-2;
        tmp_pos=recv(fdc->fd,tmp_buf,SERVER_HANDLE_TMP_BUFLEN,MSG_DONTWAIT);
        if(tmp_pos==0){     //对端关闭
            perror("handle_IPC_data-IPC shutdown");
            return 1;
        }else if(tmp_pos==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){      //读尽
                break;
            }else{
                perror("handle_IPC_data/recv err");
                return 1;
            }
        }//读到一条信息
        memcpy(&fdc->IPC.rd_buf[fdc->IPC.rd_buf_pos],tmp_buf,tmp_pos);
        fdc->IPC.rd_buf_pos+=tmp_pos;
        int br=0,aim=-1;
        while(1){
            tmp_pos=SERVER_HANDLE_TMP_BUFLEN;
            r=rd_lpmsg_buf(fdc->IPC.rd_buf,&fdc->IPC.rd_buf_pos,PREFIX_LENGTH,tmp_buf,&tmp_pos);
            switch(r){
                case 1:     //没有可提取载荷
                    br=1; break;
                case -1:
                    errno=EINVAL;
                    perror("handle_IPC_data/rd_lpmsg_buf err");
                case 0:
                    FdContext *cli_fdc;
                    aim=server_parse_msg(tmp_buf,&tmp_pos,fdc,&cli_fdc);
                    switch(aim){
                        case -1:    //非法指令
                        case 0:     //传给client
                            ssize_t sdl=-2;
                            sdl=send(cli_fdc->fd,tmp_buf,tmp_pos,0);
                            if(sdl==-1){    //未发送
                                memcpy(&cli_fdc->client.task_buf[cli_fdc->client.task_buf_pos],tmp_buf,tmp_pos);
                                cli_fdc->client.status=S_CLI_SENDING;
                                struct epoll_event ev={0};
                                ev.data.ptr=cli_fdc,ev.events|=EPOLLIN|EPOLLOUT|EPOLLET;
                                if((epoll_ctl(epfd,EPOLL_CTL_ADD,cli_fdc->fd,&ev))==-1){
                                    perror("handle_IPC_data/epoll_ctl err");
                                    return -1;
                                }else if(sdl<tmp_pos){  //没发完
                                    memcpy(&cli_fdc->client.task_buf[cli_fdc->client.task_buf_pos],tmp_buf+sdl,tmp_pos-sdl);
                                    cli_fdc->client.status=S_CLI_SENDING;
                                    struct epoll_event ev={0};
                                    ev.data.ptr=cli_fdc,ev.events|=EPOLLIN|EPOLLOUT|EPOLLET;
                                    if((epoll_ctl(epfd,EPOLL_CTL_ADD,cli_fdc->fd,&ev))==-1){
                                        perror("handle_IPC_data/epoll_ctl err");
                                        return -1;
                                    }
                                }
                            }
                            break;
                    }
                    break;
                default:
                ;
            }
            if(br==1)   break;
        }
    }
    return 0;
}

int server_init_IPC(int unfd,enum IPC_type type){
    FdContext *s=malloc(sizeof(FdContext));
    if(!s){
        perror("server_init_IPC malloc failed");
        return -1;
    }
    s->fd=unfd,s->type=IPC_FD;s->IPC.type=type;
    memset(s->IPC.rd_buf,0,SERVER_FDC_IPC_RD_BUFLEN);  //初始化IPC缓存
    memset(s->IPC.task_buf,0,SERVER_FDC_IPC_TSK_BUFLEN);
    s->IPC.rd_buf_pos=0,s->IPC.task_buf_pos=0;
    HASH_ADD_INT(fdc_hashmap,fd,s);
    event.data.ptr=s,event.events=EPOLLIN|EPOLLET;
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,unfd,&event)){
        perror("server_init_IPC err"); exit(1);
    }
    return 0;
}

int server_init_listener(int local_socket){
    FdContext *s=malloc(sizeof(FdContext));
    if(!s){
        perror("server_init_listener failed");
        return -1;
    }
    s->fd=local_socket;
    s->type=SERVER_SOCKET;
    fd_hash_add(local_socket,s);
    event.data.ptr=s,event.events=EPOLLIN|EPOLLET;   //local_sock进入epoll
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,local_socket,&event)){
        perror("listener init err"); exit(1);
    }
    return 0;
}


int main(int argc,char** argv,char **env){
    fdc_hashmap=NULL;     //初始化socket状态哈希表头
    /*
    if((mkdir(CLI_STO_DIR,0744))==-1){
        perror("CLI_STO_DIR mkdir failed"); exit(1);
    }
    */
    int db_pid,upload_pid,download_pid;
    //编号(id)依次为0,1,2;对于unix套接字,主进程使用[0]
    socketpair(AF_UNIX,SOCK_STREAM,0,unfd_db);
    socketpair(AF_UNIX,SOCK_STREAM,0,unfd_up);
    socketpair(AF_UNIX,SOCK_STREAM,0,unfd_down);
    if((db_pid=fork())<0){
        perror("create database process failed");
    }else{
        if(db_pid==0){
            //db子进程
            printf("database child process setup\n");
            dup2(unfd_db[1],STDIN_FILENO);
            execl("/home/oyoki/program/gdb_debug/output/db.out","db",NULL);
            return -1;
        }
    }

    if((upload_pid=fork())<0){
        perror("create upload process failed");
    }else{
        if(upload_pid==0){
            //upload子进程 ===
            printf("upload child process setup");
            exit(0);
        }
    }

    if((download_pid=fork())<0){
        perror("create download process failed");
    }else{
        if(download_pid==0){
            //download子进程 ===
            printf("download child process setup");
            exit(0);
        }
    }
    //
    struct sockaddr general_addr={0};
    uint16_t general_port; struct in_addr general_ipv4; u_int8_t general_ipv6[16]={0};
    //通用变量general_addr,general_port,general_ipv4,general_ipv6
    inet_pton(AF_INET,LOCAL_IPADDR,&general_ipv4);   //
    general_port=htons(LOCAL_PORT);
    set_sockaddr(AF_INET,&general_ipv4,&general_port,&general_addr);

    local_socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);   //创建listen socket
    if(fcntl(local_socket,F_SETFL,O_NONBLOCK)){ //listen socket设为非阻塞
        perror("main-fcntl err");
        exit(1);
    }
    //设置地址可重用
    int optval=1;
    if (setsockopt(local_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt SO_REUSEADDR failed");
        close(local_socket);
        exit(1);
    }

    if(bind(local_socket,&general_addr,sizeof(struct sockaddr_in))){
        perror("local socket bind failed");
        exit(1);
    }
    if((listen(local_socket,256))){
       perror("local socket listen failed"); 
       exit(1);
    }
    //创建epoll实例,插入主socket和3个IPC_fd
    int count=0; 
    if((epfd=epoll_create1(EPOLL_CLOEXEC))==-1){
        perror("main-epfd err");
        exit(1);
    }
    //子进程IPC_fd进入epoll
    server_init_IPC(unfd_db[0],DATABASE_UN);
    server_init_IPC(unfd_up[0],UPLOAD_UN);
    server_init_IPC(unfd_down[0],DOWNLOAD_UN);
    //为主进程设置fdc结构体并进入epoll
    server_init_listener(local_socket);
    /*  主进程逻辑部分  */
    while(1){
        count=epoll_wait(epfd,ev,EPOLLWAIT_MAXEVENTS,100);
        if(count==-1){
            perror("epoll wait failed"); continue;
        }
        for(int i=0;i<count;i++){   //遍历所有事件
            FdContext *s=ev[i].data.ptr;
            switch(s->type){
                case SERVER_SOCKET:
                    if(ev[i].events & EPOLLIN){ 
                        handle_new_connect(local_socket);
                    } break;
                case CLIENT_SOCKET:
                    if(ev[i].events & EPOLLIN){
                        handle_client_data(ev[i].data.ptr);
                    } break;
                case IPC_FD:
                    if(ev[i].events & EPOLLIN){
                        handle_IPC_data(ev[i].data.ptr);
                    }
                    break;
                default:
            }
        }
    }
    return 0;
}