/*这是一个TCP回射服务器程序，绑定套接字，每当有客户端过来就新开进程处理。处理
	内容是把客户端发过来的数据发回去 具体步骤如下
1创建监听套接字listenfd------------------listenfd = socket(PF_INET, SOCK_STREAM, 0)
2创建并初始化服务器IPv4结构体-------------struct sockaddr_in servaddr
3设置REUSEADDR选项以避免TIMEWAIT状态------setsockopt()
4绑定监听套接字和服务器地址结构（前两者）--bind (listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr)
5开始监听--------------------------------listen (listenfd, SOMAXCONN)
6创建对方IPv4结构体-----------------------struct sockaddr_in peeraddr
7接受连接并返回一个通信套接字--------------conn = accept(listenfd,(struct sockaddr*)&peeraddr,&peerlen)
8三次握手完成，新建子进程来处理------------doit(conn) close(listenfd)
doit()-----------readn(conn,&recvbuf.len,4)  n=ntohl(recvbuf.len);
readn(conn,recvbuf.buf,n); writen(conn,&recvbuf,4+n);
9如果对方关闭，read返回0，退出子进程 。否则在while循环中处理
10主进程关闭conn套接字 回到第7步循环
11退出主进程
要点
1每个函数都有错误检测和处理
2发的时候换成网络字节序htonl，收过来换成主机字节序ntohl
3使用readn writen封装读写函数
4发时直接发4+n；收时先收4字节包头，由包头得知包体长为n，再收n字节包体
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define ERR_EXIT(m) \
	do{\
		perror(m);\
		exit(EXIT_FAILURE);\
	}while(0)
/*
 *TCP是一种流协议，像流水一样，没有边界。收方不知道一次该读取多少，再加上TCP的复杂机制就会产生粘包问题。解决方案：
 *1.定长包：双方协定好每次收发多少字节
 *2.包尾加上\r\n，如ftp http协议：确定消息边界
 *3.包头加上包体长度：先接收4字节包头，再接收x字节包体，如下readn函数
 *4.更复杂的应用层协议
 *关键问题在于read/write函数，每次读取的字节数不一定是指定字节数
 *粘包问题只存在广域网中？？
 */
struct packet{//定义数据包格式
	int len;
	char buf[1024];
};
ssize_t readn(int fd,void *buf,size_t count){//每次读取n字节的函数
	size_t nleft=count;//剩余未被读取的字节数
	size_t nread;//已接收的字节数
	char *bufp=(char *)buf;//记录buf
	while(nleft>0){//只要没读完要求的字节数
		if((nread=read(fd,bufp,nleft))<0){
			//nread记录读取的字节数，不一定=nleft
			if(errno=EINTR) continue;//被信号中断
			return -1;//其他情况的读取失败，退出
		}
		else if(nread==0) return count-nleft;//对等方关闭 返回
		bufp+=nread;//指针偏移，指向未被读取的字节
		nleft-=nread;//剩余nleft未读，回到while循环开始
	}
	return count;//nleft为0  都读完了 返回
}
ssize_t writen(int fd,void *buf,size_t count){
	size_t nleft=count;//剩余未被写入的字节数
	size_t nwrite;//已写入的字节数
	char *bufp=(char *)buf;//记录buf
	while(nleft>0){
		if((nwrite=write(fd,bufp,nleft))<0){
			//nwrite记录写的字节数，不一定=nleft
			if(errno=EINTR) continue;//被信号中断
			return -1;//其他情况的读取失败，退出
		}
		else if(nwrite==0) continue;//什么都没写
		bufp+=nwrite;//指针偏移，指向未被读取的字节
		nleft-=nwrite;//剩余nleft未读，回到while循环开始
	}
return count;//nleft为0  都写完了 返回
}
void echo_server(int conn){//开始接客
	struct packet recvbuf;//接收消息队列
	int n;
	while(1){
		memset(&recvbuf,0,sizeof(recvbuf));//初始化数组
		int ret = readn(conn,&recvbuf.len,4);//先接收4字节
		if (ret == -1) ERR_EXIT("read");
		else if(ret<4){
			printf("客户端%d关闭\n",conn);
			break;
		}
		n=ntohl(recvbuf.len);//len是网络字节序，转换成主机字节序
		ret=readn(conn,recvbuf.buf,n);
		if (ret == -1) ERR_EXIT("read");
		else if(ret<4){
			printf("客户端%d关闭\n",conn);
			break;
		}
		printf("收：");
		fputs(recvbuf.buf,stdout);//输出到屏幕
		writen(conn,&recvbuf,4+n);//回射回去
	}

}
void handle_sigchld(int sig){
    while(waitpid(-1,NULL,WNOHANG)>0);//wait all child process
}
int main(){
    signal(SIGCHLD,handle_sigchld);
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
	pid_t pid;//新建进程
	while	(1){//开始操作
		if ((conn = accept(listenfd,(struct sockaddr*)&peeraddr,&peerlen)) < 0)
			ERR_EXIT("accept");//从ESTABLISHED队列里返回第一个连接 空则阻塞
		printf("和客户端%d,IP:%s::%d已进行三次握手\n",conn,inet_ntoa(peeraddr.sin_addr),htons(peeraddr.sin_port));
		pid=fork();
		if(pid == -1){
			ERR_EXIT("fork");//新建子进程失败 退出主进程
		}
		if(pid == 0){
			printf("新建子进程:%d\n",getpid());
			close(listenfd);
			echo_server(conn);
			exit(EXIT_SUCCESS);//一旦客户端实例关闭了 这个处理的进程就没有存在的价值 要退出 否则子进程也会去accept
		}else{
			close(conn);
		}
	}
	return 0;
}

