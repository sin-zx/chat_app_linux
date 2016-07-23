#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "clientinfo.h"

#define FIFO_C "/tmp/xuzixin_command"
#define FIFO_M "/tmp/xuzixin_messaging"

struct clients{
	char name[20];
	char fifo[200];
};

/*守护进程*/
int init_daemon(int nochdir,int noclose)
{
	pid_t pid;
	pid = fork();
	
	if(pid<0)	{perror("fork");return -1;}
	if(pid!=0)	exit(0);

	pid = setsid();
	if(pid < -1)	{perror("setsid");return -1;}
	if(!nochdir)	chdir("/");

	if(!noclose){
		int fd;
		fd = open("/dev/null",O_RDWR,0);
		if(fd!= -1){
			dup2(fd,STDIN_FILENO);
			dup2(fd,STDOUT_FILENO);
			dup2(fd,STDERR_FILENO);
			if(fd > 2)	close(fd);
		}
	}
	umask(0027);
	return 0;
}

void handler(int sig){
	unlink(FIFO_C);
	unlink(FIFO_M);
	exit(1);
}

/*主函数*/
int main(void)
{
	pid_t pid;
	init_daemon(0,0);
	int fifo_c,fifo_m;
	int i,res,ret;
	CLIENTC client_c;
	CLIENTM client_m0,client_m;
	struct clients * clist;
	int * clsize;
	char temp_fifo[200];
	char buf[220];
	void * shared_memory = (void *)0;

	signal(SIGKILL,handler);
	signal(SIGINT,handler);
	signal(SIGTERM,handler);
	
	/*判断公共管道是否存在*/
	if(access(FIFO_C,F_OK) == -1){
		res = mkfifo(FIFO_C,0777);
		if(res!= 0){
			printf("FIFO_C was not created\n");
			exit(EXIT_FAILURE);
		}
	}
	if(access(FIFO_M,F_OK) == -1){
		res = mkfifo(FIFO_M,0777);
		if(res!= 0){
			printf("FIFO_M was not created\n");
			exit(EXIT_FAILURE);
		}
	}
	
	/*创建子进程*/
	pid = fork();	
	if(pid<0){perror("fork");return -1;}
	/*父进程管理登录、退出操作，维护在线队列*/
	else if(pid !=0){
		fifo_c = open(FIFO_C,O_RDONLY);
		if(fifo_c == -1){
			printf("Could not open fifo_c..\n");
			exit(EXIT_FAILURE);
		}
		
		/***********************共享内存********************/
		res = shmget((key_t)1234,sizeof(struct clients)*100,0666|IPC_CREAT);
		if(res == -1){
			printf("shmget failed..\n");
			exit(EXIT_FAILURE);
		}
		shared_memory = shmat(res,(void *)0,0);
		if(shared_memory == (void *)-1){
			printf("shmat failed..\n");
			exit(EXIT_FAILURE);
		}
		clist = (struct clients *)shared_memory;

		res = shmget((key_t)4321,sizeof(int),0666|IPC_CREAT);
		if(res == -1){
			printf("shmget failed..\n");
			exit(EXIT_FAILURE);
		}
		shared_memory = shmat(res,(void *)0,0);
		if(shared_memory == (void *)-1){
			printf("shmat failed..\n");
			exit(EXIT_FAILURE);
		}
		clsize = (int *)shared_memory;
		*clsize = 0;
		/********************************************/
		
		while(1){
			res = read(fifo_c,&client_c,sizeof(CLIENTC));
			if(res != 0){
				strcpy(client_m0.myname,client_c.name);
				sprintf(client_m0.myfifo,"/tmp/%s_fifo",client_c.name);
				//sprintf(temp_fifo,"/home/xujinke/Destop/King/9/client_fifo/%s_fifo",client_c.name);
				/*登录操作*/
				if(strcmp(client_c.op,"login")== 0){
					strcpy(clist[*clsize].name,client_m0.myname);
					strcpy(clist[*clsize].fifo,client_m0.myfifo);
					*clsize = *clsize + 1;
					strcpy(client_m0.myname,"\033[40;31m系统");
					sprintf(client_m0.say," %s已经上线！\033[0m",client_c.name);
					for(i = 0;i < *clsize -1 ;i++ ){
						strcpy(temp_fifo,clist[i].fifo);
						res = open(temp_fifo,O_RDWR);
						if(res == -1)printf("open %s fail..denglu\n",temp_fifo);
						write(res,&client_m0,sizeof(CLIENTM));
						close(res);
					}	
				/*退出操作*/		
				}else if(strcmp(client_c.op,"logoff")== 0){
					for(i = 0;i < *clsize;i++ ){
						if(strcmp(client_m0.myname,clist[i].name)== 0){
							res = i;break;
						}
					}
					for(i = res+1;i < *clsize; i++ ){
						strcpy(clist[i-1].name,clist[i].name);
						strcpy(clist[i-1].fifo,clist[i].fifo);
					}
					*clsize = *clsize -1;
					strcpy(client_m0.myname,"\033[40;31m系统");
					sprintf(client_m0.say," %s已经离开！\033[0m",client_c.name);
					for(i = 0;i < *clsize  ;i++ ){
						strcpy(temp_fifo,clist[i].fifo);
						res = open(temp_fifo,O_RDWR);
						if(res == -1)printf("open %s fail..\n",temp_fifo);
						write(res,&client_m0,sizeof(CLIENTM));
						close(res);
					}
				}else{printf("other op..\n");exit(EXIT_FAILURE);}
			}
		}
	/*子进程管理信息发送*/
	}else{
		fifo_m = open(FIFO_M,O_RDONLY);
		if(fifo_m == -1){
			printf("Could not open fifo_m..\n");
			exit(EXIT_FAILURE);
		}

		/***********************共享内存********************/
		res = shmget((key_t)1234,sizeof(struct clients)*100,0666|IPC_CREAT);
		if(res == -1){
			printf("shmget failed..\n");
			exit(EXIT_FAILURE);
		}
		shared_memory = shmat(res,(void *)0,0);
		if(shared_memory == (void *)-1){
			printf("shmat failed..\n");
			exit(EXIT_FAILURE);
		}
		clist = (struct clients *)shared_memory;

		res = shmget((key_t)4321,sizeof(int),0666|IPC_CREAT);
		if(res == -1){
			printf("shmget failed..\n");
			exit(EXIT_FAILURE);
		}
		shared_memory = shmat(res,(void *)0,0);
		if(shared_memory == (void *)-1){
			printf("shmat failed..\n");
			exit(EXIT_FAILURE);
		}
		clsize = (int *)shared_memory;
		/********************************************/
		
		while(1){
			ret = read(fifo_m,&client_m,sizeof(CLIENTM));
			if(ret!= 0){
				/*发送给所有人*/
				if(strcmp(client_m.toname,"all")== 0){
					for(i = 0;i < *clsize;i++ ){
						if(strcmp(client_m.myname,clist[i].name)== 0){
							res = i;break;
						}
					}
					for(i = 0;i < *clsize ;i++ ){
						if(i!= res){
							strcpy(temp_fifo,clist[i].fifo);
							ret = open(temp_fifo,O_RDWR);
							if(ret == -1)printf("open %s fail..suoyou\n",temp_fifo);
							write(ret,&client_m,sizeof(CLIENTM));
							close(ret);
						}
					}
				/*发送给单人*/
				}else{
					ret = open(client_m.tofifo,O_WRONLY);
					if(ret == -1)printf("open tofifo fail..\n");
					write(ret,&client_m,sizeof(CLIENTM));
					close(ret);
				}
			}	
		}
	}

	return 0;
}


