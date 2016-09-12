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
#define MAXLINE 4096

using namespace std;

typedef struct{
    // protocol version is 3
    // message type can be 2:JOIN, 3:FWD, 4:SEND
    // notice that vrsn is 9bits, type is 7bits
    u_char vrsn;
    u_char type;     
    uint16_t length;
}SBCPHeader;

typedef struct{
    uint16_t type;
    uint16_t length;
}SBCPAttrHeader;

char* AttrGen(SBCPAttrHeader* attrHeader, int payloadSize, char* payload){
    char *attr = (char*)malloc(sizeof(SBCPAttrHeader) + payloadSize);
    attrHeader->length = 4 + payloadSize;
    memcpy(attr, attrHeader, 4);
    memcpy(attr + 4, payload, payloadSize);
    return attr;
}

char* SBCPGen(SBCPHeader* header, vector<char*> payloads){
    int payloadsSize = 0;
    vector<int> sizeVec;
    for(auto& payload : payloads){
        sizeVec.push_back(int(payload[2] + (payload[3] << 8)));
        payloadsSize += sizeVec.back();
    }
    char* SBCP = (char*)malloc(sizeof(SBCPHeader) + payloadsSize);
    header->length = 4 + payloadsSize;
    memcpy(SBCP, header, 4);
    for(int i = 0; i < payloads.size(); ++i){
        memcpy(SBCP + i * 4 + 4, payloads[i], sizeVec[i]);
    }
    return SBCP;
}

in_port_t SERV_PORT = 8888;
string addrStr = "127.0.0.1";
const char *addr = addrStr.c_str();
void str_cli(FILE *fp, int sockfd);

int main(int argc ,char *argv[]) {
    int sockfd;
    struct sockaddr_in servaddr;
    
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, addr, &servaddr.sin_addr);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    connect(sockfd, (const struct sockaddr *)&servaddr, sizeof servaddr);
    
    str_cli(stdin, sockfd);
}

void str_cli(FILE *fp, int sockfd) {
    int maxfd, stdineof, n;
    fd_set rset;
    char buf[MAXLINE];
    FD_ZERO(&rset);
    stdineof = 0;
    while(1) {
        //如果不是已经输入结束,就继续监听终端输入
        if (stdineof == 0) FD_SET(fileno(fp), &rset);
        //监听来自服务器的信息
        FD_SET(sockfd, &rset);
        //maxfd设置为sockfd和stdin中较大的一个加1
        maxfd = (fileno(fp) > sockfd ? fileno(fp) : sockfd) + 1;
        //只关心是否有描述符读就绪,其他几个直接传NULL即可
        select(maxfd, &rset, NULL, NULL, NULL);
        
        //如果有来自服务器的信息可读
        if (FD_ISSET(sockfd, &rset)) {
            if ((n = read(sockfd, buf, MAXLINE)) == 0) {
                //如果这边输入了EOF之后服务器close掉连接说明正常结束，否则为异常结束
                if (stdineof == 1)
                    return;
                else
                    perror("terminated error\n");
            }
            //输出到终端
            for(int i = 0; i < n; i++){
                //std::cout << u_char(buf[i]) << std::endl;
                printf("%x\n", u_char(buf[i]));
            }
            //write(fileno(stdout), buf, n);
        }
        //如果有来自终端的输入
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
            //将输入信息发送给服务器
            SBCPAttrHeader* attrheader = new SBCPAttrHeader;
            attrheader->type = 4;

            SBCPHeader* packheader = new SBCPHeader;
            packheader->vrsn = 1;
            packheader->type = (1 << 7) + 2;
            //packheader->type = 0x82;
            
            char* attr = AttrGen(attrheader, n, buf);
            char* SBCP = SBCPGen(packheader, {attr});
            int lens = int(packheader->length);
            write(sockfd, SBCP, lens);
            free(attrheader);
            free(packheader);
            free(attr);
            free(SBCP);
        }
    }
}