#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#define ERR_EXIT(m) \
	do{\
		perror(m);\
		exit(EXIT_FAILURE);\
	}while(0)
/*perror()-print a system error message
	exit()-cause normal process termination*/

void handle(int sig){
		printf("读（子）进程recv a sig=%d，退出\n",sig);
		exit(EXIT_SUCCESS);
	}

int main(){
	printf("服务器主进程:%d\n",getpid());
	int listenfd;
	if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		ERR_EXIT("socket");
	printf("socket()-新建套接字成功\n");
	struct sockaddr_in servaddr;//IPv4地址结构
	memset(&servaddr,0,sizeof(servaddr));//用0填充

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(7777);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//任意本机地址
	//inet_aton("127.0.0.1",&servaddr.sin_addr);//方法2
	//servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//方法3
	int on = 1;
	if(setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on))<0)
		ERR_EXIT("setsocketopt");//设置REUSEADDR选项 避免 TIMEWAIT状态

	if (bind (listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0)
		ERR_EXIT("bind");//把IP地址结构强制转化为socket结构
	printf("bind()-监听套接字和服务器地址绑定成功\n");
	if (listen (listenfd, SOMAXCONN) < 0)//队列最大值 并发连接数
		ERR_EXIT("listen");
	printf("listen()-监听套接字开始监听\n");
	struct sockaddr_in peeraddr;//对方地址
	socklen_t peerlen = sizeof(peeraddr);//peerlen 要有初始值
	int conn;//conn记录新返回的套接字 -已连接套接字-从已连接队列移除
	if ((conn = accept(listenfd,(struct sockaddr*)&peeraddr,&peerlen)) < 0)//因为点对点聊天 只接受一次
		ERR_EXIT("accept");//从ESTABLISHED队列里返回第一个连接 空则阻塞
	printf("和对方%d,IP:%s::%d已进行三次握手\n",conn,inet_ntoa(peeraddr.sin_addr),htons(peeraddr.sin_port));
	pid_t pid;//新建进程
	pid=fork();
	if(pid==-1) ERR_EXIT("fork");//创建进程失败
	if(pid == 0){//子进程发
		signal(SIGUSR1,handle);//捕捉信号，调用函数处理
		char sendbuf[1024] = {0};		
		while((fgets(sendbuf,sizeof(sendbuf),stdin)) != NULL){/*如果输入流有数据就写入sendbuf 这就会导致一个问题：进程一直等stdin，stdin被关闭才返回空，然后执行下面的退出发进程代码。解决方法是用信号*/
			write(conn,sendbuf,strlen(sendbuf));
			memset(sendbuf,0,sizeof(sendbuf));
				
		}
	}else{//父进程收
		close(listenfd);
		char recvbuf[1024];
		while	(1){
			memset(recvbuf,0,sizeof(recvbuf));//初始化数组
			int ret = read(conn,recvbuf,sizeof(recvbuf));//读
			if(!ret){
				printf("对方%d关闭\n",conn);
				break;
			}else if (ret == -1) ERR_EXIT("read");
			printf("对方说:");
			fputs(recvbuf,stdout);//输出到屏幕
		}
	printf("读（父）进程退出\n");
	kill(pid,SIGUSR1);//读进程给写进程发送一个用户自定信号，来关闭写进程
	exit(EXIT_SUCCESS);//对方关闭，跳出循环，结束子进程	
	return 0;
	}
}

	
