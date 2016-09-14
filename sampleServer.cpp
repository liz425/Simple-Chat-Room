#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#define MAXLINE 4096

in_port_t SERV_PORT = 8888;



int main(int argc ,char *argv[]){
    int i;
    int listenfd, connfd, sockfd;
    int maxfd, maxi, nready, client[FD_SETSIZE];
    char buf[MAXLINE];
    struct sockaddr_in cliaddr, servaddr;
    socklen_t clilen;
    ssize_t n;
    fd_set rset, allset;
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    
    if (-1 == bind(listenfd, (struct sockaddr*)&servaddr, sizeof servaddr))
        printf("bind error\n");
    
    listen(listenfd, 1024);
    
    //客户端描述符存储在client中,maxi表示该数组最大的存有客户端描述符的数组下标
    maxfd = listenfd;
    maxi = -1;
    memset(client, -1, sizeof client);
    //初始化读就绪的fd_set数组，并监听listen描述符
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    
    while (1) {
        //allset是监控的描述符列表，rset是可读描述符列表
        rset = allset;
        nready = select(maxfd+1, &rset, NULL, NULL, NULL);
        //如果listen描述符可读，说明有客户端连接
        if (FD_ISSET(listenfd, &rset)) {
            clilen = sizeof cliaddr;
            connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
            if (connfd == -1) perror("accept error\n");
            else printf("%d accepted!\n", connfd);
            
            //扫描client数组，找到下标最小的未用的来存客户端描述符
            for (i = 0; i < FD_SETSIZE; i++) if (client[i] < 0) {
                client[i] = connfd;
                break;
            }
            if (i == FD_SETSIZE) perror("too many clients\n");
            //将客户端描述符放到监视的fd_set中，并更新maxfd和maxi
            FD_SET(connfd, &allset);
            if (connfd > maxfd) maxfd = connfd;
            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;
        }
        //扫描所有的客户端，查看是否有描述符读就绪
        for (i = 0; i <= maxi; i++) {
            if ((sockfd = client[i]) < 0) continue;
            if (FD_ISSET(sockfd, &rset)) {
                //读到EOF或错误,清除该描述符
                if ((n = read(sockfd, buf, MAXLINE)) <= 0) {
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                    if (n < 0) perror("read error\n");
                    //回显给客户端
                } else {
                    write(sockfd, buf, n);
                }
                if (--nready <= 0) break;
            }
        }
    }
    return 0;
}