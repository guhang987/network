/*这是一个TCP回射客户端程序，绑定套接字，去连接服务器,收发消息

1创建主动套接字sock-----------------------sock = socket(PF_INET, SOCK_STREAM, 0))
2创建并初始化服务器IPv4结构体-------------struct sockaddr_in servaddr
3用自己的套接字连接服务器-----------------connect(sock,(struct sockaddr *)&servaddr,sizeof(servaddr))
4三次握手完成，进入while循环{read (sock) write(sock)}，循环条件是能从stdin读取字符（不会自动停止）
5退出进程
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
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
struct packet {
    int len;//包头 存放包体的实际长度
    char buf[1024];//包体 缓冲区
};
ssize_t readn(int fd,void *buf,size_t count) {
    size_t nleft=count;//剩余未被读取的字节数
    size_t nread;//已接收的字节数
    char *bufp=(char *)buf;//记录buf
    while(nleft>0) //只要没读完要求的字节数
    {
        if((nread=read(fd,bufp,nleft))<0)
        {
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
    while(nleft>0)
    {
        if((nwrite=write(fd,bufp,nleft))<0)
        {
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
void echo_client(int sock){
        struct packet sendbuf;//发送的数据包
        memset(&sendbuf,0,sizeof(sendbuf));
        struct packet recvbuf;//接收的数据包
        fd_set ret;
        FD_ZERO(&ret);
        int fd_stdin=fileno(stdin);
        int maxfd=fd_stdin>sock?fd_stdin:sock;
        int nready;//ready things number
        while(1){
            FD_SET(fd_stdin,&ret);//
            FD_SET(sock,&ret);
            nready=select(maxfd+1,&ret,NULL,NULL,NULL);
            if(nready==-1) ERR_EXIT("select");
            if(nready==0) continue;
            if(FD_ISSET(sock,&ret)){//检测到服务器的消息
                printf("select() 检测到服务器的可读事件\n");
                int ret = readn(sock,&recvbuf.len,4);//先接收4字节
                if (ret == -1) ERR_EXIT("read");
                else if(ret<4){
                    printf("客户端%d关闭\n",sock);
                    break;
                }
                printf("服务器说：");
                int n=ntohl(recvbuf.len);//len是网络字节序，转换成主机字节序
                ret=readn(sock,recvbuf.buf,n);
                if (ret == -1) ERR_EXIT("read");
                else if(ret<4)
                {
                    printf("server%d关闭\n",sock);
                    break;
                }
                fputs(recvbuf.buf,stdout);//输出到屏幕
                memset(&recvbuf,0,sizeof(recvbuf));
            }
            if(FD_ISSET(fd_stdin,&ret)){//检测到屏幕有输入
                printf("select() 检测到屏幕有输入\n");
                if(fgets(sendbuf.buf,sizeof(sendbuf.buf),stdin)==NULL) break;
                int n=strlen(sendbuf.buf);
                printf("发生长度为%d 发送内容为%s\n",n,sendbuf.buf);
                sendbuf.len=htonl(n);//换成网络字节序
                writen (sock,&sendbuf,4+n);//senbbuf写入套接字 包头四字节
                memset(&sendbuf,0,sizeof(sendbuf));
            }
        }
        close(sock);
}
int main()
{
    printf("note:at least input 3 byte chars\n");
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
    echo_client(sock);
    close(sock);
    printf("关闭套接字\n");
    return 0;
}


