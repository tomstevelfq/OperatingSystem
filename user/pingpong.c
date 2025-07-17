#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

int main(int argc,char *argv[]){
    int pip1[2],pip2[2];
    pipe(pip1);
    pipe(pip2);
    if(fork()!=0){
        write(pip1[1],"hello world",11);
        close(pip1[1]);
        char buff[20]={0};
        read(pip2[0],buff,11);
        printf("%s\n",buff);
        wait(0);
    }else{
        char buff[20]={0};
        read(pip1[0],buff,11);
        printf("receive:%s\n",buff);
        write(pip2[1],buff,11);
        close(pip2[1]);
    }
    close(pip1[0]);
    close(pip2[0]);
    exit(0);
}