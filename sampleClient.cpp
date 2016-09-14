#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "header.hpp"

using namespace std;



//server ip and port will be overwritten by input arguments
in_port_t SERV_PORT = 8888;
string addrStr = "127.0.0.1";

void str_cli(FILE *fp, int sockfd);

int main(int argc ,char *argv[]) {
    int sockfd;
    struct sockaddr_in servaddr;
    
    if(argc != 4){
        printf("Command arguments ERROR!\n");
        printf("Usage: ./Client <username> <server_ip> <server_port>\n");
        return 0;
    }

    addrStr = string(argv[2]);
    SERV_PORT = stoi(string(argv[3]));
    const char *addr = addrStr.c_str();

    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, addr, &servaddr.sin_addr);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    connect(sockfd, (const struct sockaddr *)&servaddr, sizeof servaddr);
    
    //Initiate a JOIN withe the server using the username 
    char* attr = AttrGen(2, strlen(argv[1]), argv[1]);
    int lens = 0;
    char* SBCP = SBCPGen(3, JOIN, {attr}, lens);
    write(sockfd, SBCP, lens);
    free(attr);
    free(SBCP);
    

    str_cli(stdin, sockfd);
}

void str_cli(FILE *fp, int sockfd) {
    int maxfd, stdineof, n;
    fd_set rset;
    char buf[MAXLINE];
    FD_ZERO(&rset);
    stdineof = 0;
    while(1) {
        cout<< "I don't believe it." << endl;
        //如果不是已经输入结束,就继续监听终端输入
        if (stdineof == 0) FD_SET(fileno(fp), &rset);
        //监听来自服务器的信息
        FD_SET(sockfd, &rset);
        //maxfd设置为sockfd和stdin中较大的一个加1
        maxfd = (fileno(fp) > sockfd ? fileno(fp) : sockfd) + 1;
        //只关心是否有描述符读就绪,其他几个直接传NULL即可
        cout << "mark 1" << endl;
        select(maxfd, &rset, NULL, NULL, NULL);
        cout << "mark 2" << endl;

        //如果有来自服务器的信息可读
        if (FD_ISSET(sockfd, &rset)) {
            if ((n = read(sockfd, buf, MAXLINE)) == 0) {
                //如果这边输入了EOF之后服务器close掉连接说明正常结束，否则为异常结束
                if (stdineof == 1)
                    return;
                else
                    perror("terminated error\n");
            }
            //out put message 
            for(int i = 0; i < n; i++){
                //std::cout << u_char(buf[i]) << std::endl;
                printf("%c\n", u_char(buf[i]));
            }
            //write(fileno(stdout), buf, n);
        }
        //如果有来自终端的输入
        cout << "mark 3" << endl;
        if (FD_ISSET(fileno(fp), &rset)) {
            //终端这边输入了结束符
            if ((n = read(fileno(fp), buf, MAXLINE)) == 0) {
                //标记已经输入完毕，并只单端关闭写，因为可能还有消息在来客户端的路上尚未处理 
                stdineof = 1;
                shutdown(sockfd, SHUT_WR);
                //不再监听终端输入
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            
            cout << "mark 4" << endl;
            //send keyborad input to server
            char* attr = AttrGen(4, n, buf);
            int lens = 0;
            char* SBCP = SBCPGen(3, SEND, {attr}, lens);
            write(sockfd, SBCP, lens);
            free(attr);
            free(SBCP);
        }
    }
}
