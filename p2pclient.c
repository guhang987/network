#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define ERR_EXIT(m) \
	do{\
		perror(m);\
		exit(EXIT_FAILURE);\
	}while(0)
/*perror()-print a system error message
	exit()-cause normal process termination*/
void handle(int sig){
	printf("读（父）进程recv a sig=%d，退出\n",sig);
	exit(EXIT_SUCCESS);
}
int main(){
	printf("本客户端进程：%d\n",getpid());
	int sock;
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		ERR_EXIT("socket");
	printf("socket()-新建套接字成功\n");
	struct sockaddr_in servaddr;//IPv4地址结构
	memset(&servaddr,0,sizeof(servaddr));//用0填充
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(7777);
	//servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//任意本机地址
	//inet_aton("127.0.0.1",&servaddr.sin_addr);//方法2
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//方法3
	if((connect(sock,(struct sockaddr *)&servaddr,sizeof(servaddr))) < 0)	
		ERR_EXIT("connect");
	printf("和服务器%s:%d已进行三次握手\n",inet_ntoa(servaddr.sin_addr),htons(servaddr.sin_port));
	pid_t pid;
	pid=fork();	
	if(pid==-1) ERR_EXIT("fork");
	if(pid==0){//子进程收
		char recvbuf[1024] = {0};
		while(1){
			int ret=read (sock,recvbuf,sizeof(recvbuf));//从套接字读取字符串，放到recvbuf
			if(ret== -1) ERR_EXIT("read");
			else if (ret == 0){
				printf("对方%d关闭\n",sock);
				break;
			}
			printf("对方说:");
			fputs(recvbuf,stdout);//输出到屏幕
			memset(recvbuf,0,sizeof(recvbuf));
		}	
		close(sock);
		printf("读（子）进程退出\n");
		kill(getppid(),SIGUSR1);
		exit(EXIT_SUCCESS);
	}else{//父进程发
		signal(SIGUSR1,handle);
		char sendbuf[1024] = {0};
		while(fgets(sendbuf,sizeof(sendbuf),stdin)!=NULL ){//屏幕输入到sendbuf
			write (sock,sendbuf,strlen(sendbuf));//senbbuf写入套接字
			memset(sendbuf,0,sizeof(sendbuf));
		}
		close(sock);
	}
	printf("关闭套接字\n");
	return 0;}


