
#include<sys/socket.h>
#include<errno.h>
#include<stdio.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>
void *client_thread(void *arg){
    int clientfd = *(int *)arg;
    while(1){
        char buffer[128]={0};
        int count = recv(clientfd,buffer,128,0);//阻塞
        if(count==0){
            break;
        }
        send(clientfd,buffer,count,0);
        printf("clientfd: %d,count: %d,buffer: %s\n",clientfd,count,buffer);
    }
    close(clientfd);
   
}//一个请求一个线程实现多个客户端连一个服务器


int main(){
    //IO概念：fd，socket
    int sockfd = socket(AF_INET, SOCK_STREAM,0);
    struct sockaddr_in serveraddr;
    memset(&serveraddr,0,sizeof(struct sockaddr_in));
    serveraddr.sin_family =AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port=htons(2048);
    if(-1==bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(struct sockaddr))){
        perror("bind");
        return -1;
    }
    listen(sockfd,10);

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
#else

    while(1){
        struct sockaddr_in clientaddr;
        socklen_t len =sizeof(clientaddr);
        int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);//阻塞
        pthread_t threadid;
        pthread_create(&threadid,NULL,client_thread,&clientfd);
        printf("accept\n");
    }
#endif
    getchar();
    //close(clientfd);
}