
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
// void *client_thread(void *arg){
//     int clientfd = *(int *)arg;
//     while(1){
//         char buffer[128]={0};
//         int count = recv(clientfd,buffer,128,0);//阻塞
//         if(count==0){
//             break;
//         }
//         send(clientfd,buffer,count,0);
//         printf("clientfd: %d,count: %d,buffer: %s\n",clientfd,count,buffer);
//     }
//     close(clientfd);
   
// }//一个请求一个线程实现多个客户端连一个服务器

//网络编程  面向IO流程
// 启动服务
// ├─ 创建监听套接字 (sockfd)
// ├─ 绑定端口 2048
// ├─ 开始监听
// └─ 进入 epoll 事件循环
//    ├─ 新连接到达 (sockfd 可读)
//    │  ├─ accept 获取 clientfd
//    │  └─ 将 clientfd 加入 epoll（ET 模式）
//    └─ 客户端数据到达 (clientfd 可读)
//       ├─ recv 读取数据（需处理分包）
//       ├─ send 回显数据
//       └─ 连接关闭时移除 clientfd 


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
#if 0
    struct sockaddr_in clientaddr;
    socklen_t len =sizeof(clientaddr);
    int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);//阻塞
    printf("accept\n");

#if 0
     char buffer[128]={0};
    int count = recv(clientfd,buffer,128,0);//阻塞
    send(clientfd,buffer,count,0);
    printf("sockfd: %d,clientfd: %d,count: %d,buffer: %s\n",sockfd,clientfd,count,buffer);//连接后sockfd为3，clientfd为4 文件描述符（int）依次增加，ls /dev/std* -l 可参看012对应stdin,stdout,stderr
#else
    while(1){
        char buffer[128]={0};
        int count = recv(clientfd,buffer,128,0);//阻塞
        if(count==0) break;
        send(clientfd,buffer,count,0);
        printf("sockfd: %d,clientfd: %d,count: %d,buffer: %s\n",sockfd,clientfd,count,buffer);//连接后sockfd为3，clientfd为4 文件描述符（int）依次增加，ls /dev/std* -l 可参看012对应stdin,stdout,stderr

    }
#endif
#elif 0
    while(1){
        struct sockaddr_in clientaddr;
        socklen_t len =sizeof(clientaddr);
        int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);//阻塞
        pthread_t threadid;
        pthread_create(&threadid,NULL,client_thread,&clientfd);
        printf("accept\n");
    }
#elif 0//select

    //select(maxfd,rset,wset,eset,timeout);
    // typedef struct {
    //     unsigned long fds_bits[1024/(8*sizeof(long))];
    // } _kernel_fd_set;


    //select实现单线程多客户端
    //缺点1.参数多（最大fd，可读集合，可写集合，错误集合，timeout轮询时间）2.rset每次都要拷贝到内核对性能有影响 3.对io数量有限制，每次需要遍历io
    fd_set rfds,rset;
    FD_ZERO(&rfds);
    FD_SET(sockfd,&rfds);
    int maxfd=sockfd;
    while(1){
        rset=rfds;
        int nready = select(maxfd+1,&rset,NULL,NULL,NULL);
        if(FD_ISSET(sockfd,&rset)){
            struct sockaddr_in clientaddr;
            socklen_t len =sizeof(clientaddr);
            int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
            printf("sockfd: %d\n",sockfd); 
            FD_SET(clientfd,&rfds);
            maxfd=clientfd;
        }
        int i=0;
        for(int i=sockfd+1;i<=maxfd;i++){
            if(FD_ISSET(i,&rset)){
                char buffer[128]={0};
                int count = recv(i,buffer,128,0);//阻塞
                if(count==0){
                    printf("disconnect\n");
                    FD_CLR(i,&rfds);
                    close(i);
                    break;
                }
                send(i,buffer,count,0);
                printf("clientfd: %d,count: %d,buffer: %s\n",i,count,buffer);
            }
        }
    }

#elif 0//poll将rset，wset，eset进行了整合，减少了接口，底层实现是相同的

    struct pollfd fds[1024]={0};
    fds[sockfd].fd=sockfd;
    fds[sockfd].events=POLLIN;
    int maxfd=sockfd;
    while(1){
        int nready = poll(fds,maxfd+1,-1);
        if(fds[sockfd].revents & POLLIN){
            struct sockaddr_in clientaddr;
            socklen_t len =sizeof(clientaddr);
            int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
            printf("client: %d\n",clientfd); 
            fds[clientfd].fd=clientfd;
            fds[clientfd].events=POLLIN;
            maxfd=clientfd;
        }
        int i=0;
        for(int i=sockfd+1;i<=maxfd;++i){
            if(fds[i].revents & POLLIN){
                char buffer[128]={0};
                int count = recv(i,buffer,128,0);//阻塞
                if(count==0){
                    printf("disconnect\n");
                    fds[i].fd=i;
                    fds[i].events=0;
                    close(i);
                    continue;
                }
                send(i,buffer,count,0);
                printf("clientfd: %d,count: %d,buffer: %s\n",i,count,buffer);
            }
        }
    }
    

#else //epoll 
    //epoll_create() epoll_ctl() epoll_wait()
    int epfd = epoll_create(1);
    //功能：创建一个 epoll 实例，参数 1 是历史遗留值（只需 >0）。返回值：epfd 是 epoll 的文件描述符。
    struct epoll_event ev;
    ev.events = EPOLLIN;//监听可读事件
    ev.data.fd=sockfd;//关联套接字
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);
    //功能：将监听套接字 sockfd 添加到 epoll 实例，监听其可读事件（新连接到达时会触发）。
    struct epoll_event events[1024]={0};
    while(1){
        int nready = epoll_wait(epfd,events,1024,-1);
        //功能：阻塞等待事件就绪，-1 表示无限等待。返回值：nready 是就绪事件的数量，events 数组存储具体事件。
        int i = 0;
        for(i=0;i<nready;++i){
            int connfd =events[i].data.fd;
            if(sockfd == connfd){
                //条件判断：如果就绪的 FD 是监听套接字 sockfd，表示有新连接。
                struct sockaddr_in clientaddr;
                socklen_t len =sizeof(clientaddr);
                int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
                //功能：接受新连接，返回客户端套接字 clientfd，clientaddr 存储客户端地址。
                ev.events = EPOLLIN | EPOLLET;//边沿触发：ET 来一次只触发一次
                ev.data.fd = clientfd;
                epoll_ctl(epfd,EPOLL_CTL_ADD,clientfd,&ev);//将 clientfd 加入 epoll 监听。
                printf("clientfd: %d\n",clientfd);
            }else if(events[i].events & EPOLLIN){
                char buffer[10]={0}; //水平触发:LT 有数据就继续传 eg:发40个分四组读 可以用于分包
                int count = recv(connfd,buffer,10,0);//阻塞
                if(count==0){
                    printf("disconnect\n"); 
                    epoll_ctl(epfd,EPOLL_CTL_DEL,connfd,NULL);               
                    close(connfd);
                    continue;
                }//关键点：recv 返回 0 表示客户端主动关闭连接。需从 epoll 移除并关闭 connfd。
                send(connfd,buffer,count,0);//回显数据
                printf("clientfd: %d,count: %d,buffer: %s\n",connfd,count,buffer);
            }
        }
    }

#endif


    getchar();
    //close(clientfd);
}