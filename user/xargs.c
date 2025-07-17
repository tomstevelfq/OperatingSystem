#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"
#include"kernel/fs.h"
#include"kernel/param.h"

void execCommand(char* Command,char* Argv[MAXARG]){
    if(fork()==0){
        exec(Command,Argv);
    }else{
        wait(0);
    }
}

int main(int argc,char* argv[]){
    char path[512]="";
    char * ptr;
    char *Argv[MAXARG];
    int start=argc-1;

    ptr=path+strlen(path);
    memmove(ptr,argv[1],strlen(argv[1]));
    ptr[strlen(argv[1])]=0;

    for(int i=1;i<argc;i++){
        uint len=strlen(argv[i])+1;
        Argv[i-1]=malloc(len);
        memmove(Argv[i-1],argv[i],len);
    }

    Argv[start+1]=0;
    char readchar;
    Argv[start]=malloc(512);
    ptr=Argv[start];
    while(read(0,&readchar,1)){
        if(readchar!='\n'){
            *ptr=readchar;
            ++ptr;
        }else{
            *ptr=0;
            printf("argv:%s\n",Argv[start]);
            execCommand(path,Argv);
            ptr=Argv[start];
        }
    }

    for(int i=0;i<=start;++i){
        free(Argv[i]);
    }

    exit(0);
}