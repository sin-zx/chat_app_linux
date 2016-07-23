#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
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

void * thread_function(void * arg);
char mypipename[200];
char myname[20];
pthread_mutex_t work_mutex;

void handler(int sig){
	unlink(mypipename);
	exit(1);
}


int main(int argc,char *argv[]){
	int res,len;
	int fifo_c,fifo_m;
	pid_t pid;
	CLIENTC client_c;
	CLIENTM client_m;
	char str[20];
	pthread_t thread;

	signal(SIGKILL,handler);
	signal(SIGINT,handler);
	signal(SIGTERM,handler);

	/*检查服务器公共管道是否存在*/
	if( (access(FIFO_C,F_OK)== -1) || (access(FIFO_M,F_OK)== -1)){
		printf("找不到服务...\n");
		exit(EXIT_FAILURE);
	}
	/*使用access()作用户认证方面的判断要特别小心，例如在access()后再作open()空文件可能会造成系统安全上的问题。 ??? */
	/*打开公共管道*/
	fifo_c = open(FIFO_C,O_RDWR);
	if(fifo_c == -1){
		printf("Could not open fifo_c..\n");exit(EXIT_FAILURE);
	}
	fifo_m = open(FIFO_M,O_RDWR);	
	if(fifo_m == -1){
		printf("Could not open fifo_m..\n");exit(EXIT_FAILURE);
	}
	/*登录*/
	while(1){
		printf("输入用户名登录：");
		scanf("%s",client_c.name);
		sprintf(mypipename,"/tmp/%s_fifo",client_c.name);  /*构造fifo名称*/
		if(access(mypipename,F_OK) != -1){
			printf("用户名已存在,请重新输入.\n");
		}else{ break;}
	}

	/*创建自己的fifo*/
	res = mkfifo(mypipename,0777);
	if(res != 0){
		printf("FIFO %s was not created\n",mypipename);
		exit(EXIT_FAILURE);
	}
	strcpy(client_c.op,"login");
	strcpy(myname,client_c.name);
	printf("登录成功！\n");
	sleep(2);
	printf("\033[2J");
	res = write(fifo_c,&client_c,sizeof(CLIENTC));

	if(pthread_mutex_init(&work_mutex,NULL)!=0){
		perror("Mutex init failed..");exit(1);
	}

	/*创建读线程*/
	if(pthread_create(&thread,NULL,thread_function,NULL)!= 0){
		printf("Thread creation failed..\n");exit(EXIT_FAILURE);
	}
	/*****/
	if(pthread_mutex_lock(&work_mutex)!=0){
		perror("Lock failed");exit(1);
	}
	printf("\033[1;0H\033[K");
	printf("\033[40;31m[%s]>>> \033[0m",client_c.name);
	printf("\033[s");
	printf("\033[2;0H\033[K");
	printf(" *** \033[K\n");
	printf(" *** \033[K\n");
	printf("*****\033[K\n");
	printf(" *** \033[K\n");
	printf("  *  \033[K\n");
	printf("\033[u");
	if(pthread_mutex_unlock(&work_mutex)!=0){
		perror("unlock failed");exit(1);
	}
	/*****/
	
	/*主线程写*/
	while(1){
		scanf("%s",str);

		if(strcmp(str,"logoff")== 0){
			strcpy(client_c.op,"logoff");
			strcpy(myname,client_c.name);
			res = write(fifo_c,&client_c,sizeof(CLIENTC));
			break;
		}else if(strcmp(str,"help")== 0){
			printf("helpsss");
			if(pthread_mutex_lock(&work_mutex)!=0){
				perror("Lock failed");exit(1);
			}
			printf("\033[1;0H\033[K");
			printf("\033[40;31m[%s]>>> \033[0m",client_c.name);
			printf("\033[s");
			printf("\033[2;0H\033[K");
			printf(" ***      \033[K\n");
			printf(" ***   1.消息格式：name message（eg：jinke hello）。 \033[K\n");
			printf("*****  2.群发格式：（all hello）。 \033[K\n");
			printf(" ***   3.输入 logoff 退出。 \033[K\n");
			printf("  *       \033[K\n");
			printf("\033[u");
			if(pthread_mutex_unlock(&work_mutex)!=0){
				perror("unlock failed");exit(1);
			}
		}else{
			strcpy(client_m.toname,str);
			scanf("%s",client_m.say);
			strcpy(client_m.myname,client_c.name);
			strcpy(client_m.myfifo,mypipename);
			sprintf(client_m.tofifo,"/tmp/%s_fifo",client_m.toname);
			res = write(fifo_m,&client_m,sizeof(CLIENTM));
	
			if(pthread_mutex_lock(&work_mutex)!=0){
				perror("Lock failed");exit(1);
			}
			printf("\033[1;0H\033[K");
			printf("\033[40;31m[%s]>>> \033[0m",client_c.name);
			printf("\033[s");
			printf("\033[2;0H\033[K");
			printf(" *** \033[K\n");
			printf(" *** \033[K\n");
			printf("*****\033[K\n");
			printf(" *** \033[K\n");
			printf("  *  \033[K\n");
			printf("\033[u");
			if(pthread_mutex_unlock(&work_mutex)!=0){
				perror("unlock failed");exit(1);
			}
		}
	}
	printf("\033[2J");
	pthread_cancel(thread);
	pthread_join(thread,NULL);
	return 0;
}

void * thread_function(void * arg){
	int count = 8;
	int myfifo;
	int res;
	int x;
	CLIENTM client_m;
	x = strlen(myname);
	myfifo = open(mypipename,O_RDONLY);
	/*注：此程序中open不能有O_NONBLOCK 参数,否则会发生重复读。
	管道读写规则
	当没有数据可读时
	O_NONBLOCK disable：read调用阻塞，即进程暂停执行，一直等到有数据来到为止。
	O_NONBLOCK enable：read调用返回-1，errno值为EAGAIN。
	当管道满的时候
	O_NONBLOCK disable： write调用阻塞，直到有进程读走数据
	O_NONBLOCK enable：调用返回-1，errno值为EAGAIN
	*/
	if(myfifo == -1){
		printf("Could not open myfifo..\n");
		exit(EXIT_FAILURE);
	}
	
	/*不断地读*/
	while(1){
		/*if(pthread_mutex_lock(&work_mutex)!=0){
			perror("Lock failed");exit(1);
		}
		printf("\033[1;%dH",x+7);
		printf("\033[K");
		if(pthread_mutex_unlock(&work_mutex)!=0){
			perror("unlock failed");exit(1);
		}*/
		/*异步取消*/
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
		pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
		
		res = read(myfifo,&client_m,sizeof(CLIENTM));
		if(res > 0){
			if(pthread_mutex_lock(&work_mutex)!=0){
				perror("Lock failed");exit(1);
			}
			printf("\033[s");
			if(count >= 18){
				count = 8;
				printf("\033[2J");
				printf("\033[1;0H\033[K");
				printf("\033[40;31m[%s]>>> \033[0m",client_m.toname);
				printf("\033[s");
				printf("\033[2;0H\033[K");
				printf(" *** \033[K\n");
				printf(" *** \033[K\n");
				printf("*****\033[K\n");
				printf(" *** \033[K\n");
				printf("  *  \033[K\n");
				printf("\033[u");
			}
			printf("\033[%d;0H ||%s:%s\n",count,client_m.myname,client_m.say);
			count++;
			printf("\033[1;%dH\033[K",x+7);
			if(pthread_mutex_unlock(&work_mutex)!=0){
				perror("unlock failed");exit(1);
			}
		}
	}
}
