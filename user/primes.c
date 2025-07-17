#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

void prime(int pip[2]){
    int p;
    read(pip[0],&p,sizeof(p));
    if(p==-1){
        exit(0);
    }
    printf("prime:%d\n",p);

    int pip2[2];
    pipe(pip2);
    if(fork()==0){
        close(pip2[1]);
        close(pip[0]);
        prime(pip2);
    }else{
        close(pip2[0]);
        int buf;
        while(read(pip[0],&buf,sizeof(buf))&&buf!=-1){
            if(buf%p!=0){
                write(pip2[1],&buf,sizeof(buf));
            }
        }

        buf=-1;
        write(pip2[1],&buf,sizeof(buf));
        wait(0);
        exit(0);
    }
}

int main(int argc,char *argv[]){
    int input_pip[2];
    pipe(input_pip);
    if(fork()==0){
        close(input_pip[1]);
        prime(input_pip);
        exit(0);
    }else{
        close(input_pip[0]);
        int i;
        for(i=2;i<=35;i++){
            write(input_pip[1],&i,sizeof(i));
        }
        i=-1;
        write(input_pip[1],&i,sizeof(i));
    }
    wait(0);
    exit(0);
}