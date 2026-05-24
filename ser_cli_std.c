void set_cmd(char* buf,int cmd);
int get_cmd(char* buf);

void set_cmd(char* buf,int cmd){
    buf[0]=cmd;
}

int get_cmd(char* buf){
    return buf[0];
}