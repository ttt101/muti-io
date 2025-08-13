
#include<sys/socket.h>
#include<errno.h>
#include<stdio.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#define BUFFER_LENGTH 1024
typedef int(*RCALLBACK)(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int set_event(int fd,int event,int flag);
struct conn_item{
    int fd;
    char rbuffer[BUFFER_LENGTH];
    int rlen;
    char wbuffer[BUFFER_LENGTH];
    int wlen;
    union {
        RCALLBACK accept_callback;
        RCALLBACK recv_callback;
    }recv_t;
    RCALLBACK send_callback;

};
struct conn_item connlist[1024] = {0};

int epfd = 0;
 
int accept_cb(int fd){
    struct sockaddr_in clientaddr;
    socklen_t len =sizeof(clientaddr);
    int clientfd=accept(fd,(struct sockaddr*)&clientaddr,&len);
    //功能：接受新连接，返回客户端套接字 clientfd，clientaddr 存储客户端地址。
    if(clientfd<0){
        return -1;
    }
    set_event(clientfd,EPOLLIN,1);//将 clientfd 加入 epoll 监听。
    connlist[clientfd].fd = clientfd;
    memset(connlist[clientfd].rbuffer,0,BUFFER_LENGTH);
    connlist[clientfd].rlen=0;
    memset(connlist[clientfd].wbuffer,0,BUFFER_LENGTH);
    connlist[clientfd].wlen=0;
    connlist[clientfd].recv_t.recv_callback=recv_cb;
    connlist[clientfd].send_callback=send_cb;
    return clientfd;
}

int set_event(int fd,int event,int flag){
    if(flag==1){//1:add 0:mod
        struct epoll_event ev;
        ev.events = event;//监听可读事件
        ev.data.fd=fd;//关联套接字
        epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
    }else{
        struct epoll_event ev;
        ev.events = event;//监听可读事件
        ev.data.fd=fd;//关联套接字
        epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
    }
    
}
int recv_cb(int fd){
    char *buffer=connlist[fd].rbuffer;
    int idx=connlist[fd].rlen;
    int count = recv(fd,buffer+idx,BUFFER_LENGTH-idx,0);//阻塞
    if(count==0){
        printf("disconnect\n"); 
        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);               
        close(fd);
        return -1;
    }//关键点：recv 返回 0 表示客户端主动关闭连接。需从 epoll 移除并关闭 connfd。
    connlist[fd].rlen+=count;
    //send(fd,buffer,connlist[fd].idx,0);//回显数据
#if 0
    memcpy(connlist[fd].wbuffer,connlist[fd].rbuffer,connlist[fd].rlen);
    connlist[fd].wlen=connlist[fd].rlen;
#else
    http_request(&connlist[fd]);
    http_response(&connlist[fd]);
#endif
    set_event(fd,EPOLLOUT,0);
    return count;
}

int send_cb(int fd){
    char *buffer=connlist[fd].wbuffer;
    int idx=connlist[fd].wlen;
    int count = send(fd,buffer,idx,0);//回显数据
    set_event(fd,EPOLLIN,0);
    return count;
}

int main(){
    //IO概念：fd，socket
    int sockfd = socket(AF_INET, SOCK_STREAM,0);
    //功能：创建一个 IPv4 (AF_INET) 的 TCP 流套接字 (SOCK_STREAM)。返回值：sockfd 是套接字的文件描述符（FD），失败时返回 -1。
    struct sockaddr_in serveraddr;
    memset(&serveraddr,0,sizeof(struct sockaddr_in));
    //功能：初始化服务器地址结构体 serveraddr，并清零内存。
    serveraddr.sin_family =AF_INET;//IPv4
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//监听所有网口
    serveraddr.sin_port=htons(2048);//端口2048
    //关键点：INADDR_ANY 表示监听所有本地 IP。htons 将端口号转换为网络字节序（大端）。
    if(-1==bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(struct sockaddr))){
        perror("bind");
        return -1;
    }
    //功能：将套接字绑定到指定的 IP 和端口。失败处理：perror 打印错误信息并退出。
    listen(sockfd,10);
    //功能：将套接字设为监听状态，10 是等待连接队列的最大长度。
    connlist[sockfd].fd = sockfd;
    connlist[sockfd].recv_t.accept_callback=accept_cb;
    epfd = epoll_create(1);
    //功能：创建一个 epoll 实例，参数 1 是历史遗留值（只需 >0）。返回值：epfd 是 epoll 的文件描述符。

    struct epoll_event ev;
    ev.events = EPOLLIN;//监听可读事件
    ev.data.fd=sockfd;//关联套接字
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);
    set_event(sockfd,EPOLLIN,1);
    //功能：将监听套接字 sockfd 添加到 epoll 实例，监听其可读事件（新连接到达时会触发）。
    struct epoll_event events[1024]={0};
    while(1){
        int nready = epoll_wait(epfd,events,1024,-1);
        //功能：阻塞等待事件就绪，-1 表示无限等待。返回值：nready 是就绪事件的数量，events 数组存储具体事件。
        int i = 0;
        for(i=0;i<nready;++i){
            int connfd =events[i].data.fd;
            if(events[i].events & EPOLLIN){
                int count = connlist[connfd].recv_t.recv_callback(connfd);
                printf("recv count: %d <-- buffer: %s\n",count,connlist[connfd].rbuffer);
            }else if(events[i].events & EPOLLOUT){
                printf("send --> buffer: %s\n",connlist[connfd].wbuffer);
                int count = connlist[connfd].send_callback(connfd);
            }
        }
    }
    getchar();
}