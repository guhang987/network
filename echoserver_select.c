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
#include <sys/wait.h>
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
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//任意本机地址,方法1
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

	int maxfd=listenfd;//最大套接字描述符
	fd_set rset,allset;
	FD_ZERO(&rset);
	FD_ZERO(&allset);//要管理的所有套接字，包括listenfd 每个客户端的conn
	FD_SET(listenfd,&allset);
	int client[FD_SETSIZE];//client数组表示目前连接上的客户端，-1表示空，其他数字代表客户端编号（套接字）
	for(int i=0;i<FD_SETSIZE;i++){
        client[i]=-1;//初始化为-1
	}
	while(1){//主处理程序
        rset=allset;//allset每次添加了conn 赋值给rset来监听 不是很懂
        int nready=select(maxfd+1,&rset,NULL,NULL,NULL);//开始探路
        if(nready == -1) {
            if(errno==EINTR) continue;//EINTR信号可以忽略
            ERR_EXIT("select");//其他问题导致select出错
        }
        if(nready==0) continue;//未检测到任何待处理套接字，就回到开始
        if(FD_ISSET(listenfd,&rset)){//检测到了，是不是listenfd,有连接过来了？
           printf("select 检测到listenfd不为空\n");
           conn = accept(listenfd,(struct sockaddr*)&peeraddr,&peerlen);
			//这个时候调用accept就不可能阻塞了，然后从ESTABLISHED队列里返回第一个连接
           if(conn==-1) ERR_EXIT("accept");
           int i;
           for(i=0;i<FD_SETSIZE;i++){
             if(client[i]<0){//如果client数组里住了一个客户端，就不等于-1，找下一个为-1的元素
                client[i]=conn;//存放一个连接到客户端列表数组中
                break;//跳出for循环
             }
            }
            if(i==FD_SETSIZE){//一直往后找，找到最大值了（这个最大值怎么看？）
                printf("too many conn\n");//说明连接已满
           }
           printf("和客户端%d,IP:%s::%d已进行三次握手\n",conn,inet_ntoa(peeraddr.sin_addr),htons(peeraddr.sin_port));
           FD_SET(conn,&allset);//放到allset里同时更新最大fd
           if(conn>maxfd) maxfd=conn;
           if(--nready<=0) continue;/*已经处理了一个套接字，nready--，如果小于0 说明没有需要处理的，回到开头；如果
		   大于0 说明还有别的conn */
        }
        for(int i=0;i<FD_SETSIZE;i++){//现在确定conn有响应了（消息到来）,但是这么多conn，我不知道是哪个啊，所以遍历
            conn=client[i];
            if(conn==-1) continue;//这个是空的 就再往后找
            if(FD_ISSET(conn,&rset)){//找到一个不为空，而且这个conn确实有消息到来了
                printf("select 检测到%d号已连接套接字有消息到来\n",conn);
                struct packet recvbuf;//接收消息队列
                int ret = readn(conn,&recvbuf.len,4);//先接收4字节
                if (ret == -1) ERR_EXIT("read");
                else if(ret<4){
                    printf("客户端%d关闭\n",conn);
                    FD_CLR(conn,&allset);//移除一个conn
                    break;
                }
                int n=ntohl(recvbuf.len);//len是网络字节序，转换成主机字节序
                ret=readn(conn,recvbuf.buf,n);//再接收n字节
                if (ret == -1) ERR_EXIT("read");
                else if(ret<4){
                    printf("客户端%d关闭\n",conn);
                    FD_CLR(conn,&allset);
                    break;
                }
                printf("%d号套接字说：",conn);
                fputs(recvbuf.buf,stdout);//输出到屏幕
				strcat(recvbuf.buf, "  received");
                writen(conn,&recvbuf,4+n);//回射回去
                memset(&recvbuf,0,sizeof(recvbuf));
                if(--nready<=0) continue;//处理完一个conn了 nready减一，如果小于0都处理完了，返回select，
				//否则说明还有conn没处理 返回for循环
            }
        }
	}
	return 0;
}

