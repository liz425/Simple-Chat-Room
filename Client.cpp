#include "header.hpp"
using namespace std;


int state = 0;
//FSM, 
//state = 0: initial
//state = 1: waiting for ACK
//state = 2: ACK received, sending message
//state = 3: NAK received, then stop
//state = 4: Error received.

#define INIT 0
#define WAITACK 1
#define CHAT 2
#define STOP 3
#define ERROR 4


//server ip and port will be overwritten by input arguments
in_port_t SERV_PORT = 8888;
string addrStr = "127.0.0.1";

void str_cli(FILE *fp, int sockfd);
int timerfd = timerfd_create(CLOCK_REALTIME, 0);
struct itimerspec new_value, curr_value;
uint64_t expTime;

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
    state = 1;

    //timer
    new_value.it_value.tv_sec = 5;
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    str_cli(stdin, sockfd);
}

void str_cli(FILE *fp, int sockfd) {
    int maxfd, stdineof, n;
    fd_set rset;
    char buf[MAXLINE];
    FD_ZERO(&rset);
    stdineof = 0;
    //setter timer
    if (timerfd_settime(timerfd, 0, &new_value, NULL) == -1)
        perror("timerfd_settime");
    
    while(1) {
        //cout<< "I don't believe it." << endl;
        //如果不是已经输入结束,就继续监听终端输入
        if (stdineof == 0) 
            FD_SET(fileno(fp), &rset);
        //listen info from server
        FD_SET(sockfd, &rset);
        //listen if timerout
        FD_SET(timerfd, &rset);
        //maxfd set to the largest fd + 1
        maxfd = max(max(fileno(fp), sockfd), timerfd) + 1;
        //只关心是否有描述符读就绪,其他几个直接传NULL即可
        //cout << "mark 1" << endl;
        select(maxfd, &rset, NULL, NULL, NULL);
        //cout << "mark 2" << endl;

        //如果有来自服务器的信息可读
        if (FD_ISSET(sockfd, &rset)) {
            if ((n = read(sockfd, buf, MAXLINE)) == 0) {
                //如果这边输入了EOF之后服务器close掉连接说明正常结束，否则为异常结束
                if (stdineof == 1){
                    printf("Server closed.\n");
                    close(sockfd);
                    return;
                }else{
                    printf("Server terminated.\n");
                    close(sockfd);
                    return;
                }
            }
            //unpack SBCPAttrHeader
            int SBCPType = 0;
            vector<char*> attrs = unpackMessage(buf, SBCPType);
            //cout << "Type: " << type << endl;
            //out put message 
            switch(state){
                case WAITACK:
                    if(SBCPType == ACK){
                        state = CHAT;
                        //printf("ACK received.\n");
                        int attrType = 0;
                        int payloadLen = 0;
                        for(int i = 0; i < attrs.size(); ++i){
                            char* payload = unpackAttr(attrs[i], attrType, payloadLen);
                            if(i == 0){
                                int clientCnt = ntohs(((uint16_t*)payload)[0]);
                                printf("Clients number: %d\n", clientCnt);
                                printf("Clients list: \n");
                            }else{
                                for(int j = 0; j < payloadLen; j++){
                                    printf("%c", u_char(payload[j]));
                                }
                                printf(" | ");
                            }
                        }
                        printf("\n-------------------------------------------------------\n");
                        break;
                    }else if(SBCPType == NAK){
                        state = STOP;
                        printf("NAK received.\nReason: ");
                        int attrType = 0;
                        int payloadLen = 0;
                        char* payload = unpackAttr(attrs[0], attrType, payloadLen);
                        for(int i = 0; i < payloadLen; i++){
                            printf("%c", u_char(payload[i]));
                        }
                        printf("\nQuit.\n");
                        close(sockfd);
                        return;
                    }else{
                        printf("ACK not recognized. QUIT.\n");
                        close(sockfd);
                        return;
                    }
                case CHAT:
                    if(SBCPType == FWD || SBCPType == ONLINE || SBCPType == OFFLINE || SBCPType == IDLE){
                        int attrType = 0;
                        int payloadLen = 0;
                        if(SBCPType == ONLINE || SBCPType == OFFLINE){
                            printf("User \'");
                        }
                        for(int i = 0; i < attrs.size(); ++i){
                            char* payload = unpackAttr(attrs[i], attrType, payloadLen);
                            for(int j = 0; j < payloadLen; j++){
                                printf("%c", u_char(payload[j]));
                            }
                            if(i == 0 && SBCPType == FWD)
                                printf(": ");
                        }
                        if(SBCPType == ONLINE){
                            printf("\' is online.\n");
                        }else if(SBCPType == OFFLINE){
                            printf("\' is offline.\n");
                        }
                        //printf("-------------------------------------------\n");

                    }
                default:
                    break;
            }

            for(int i = 0; i < n; i++){
                //std::cout << u_char(buf[i]) << std::endl;
                //printf("%c\n", u_char(buf[i]));
            }
            //write(fileno(stdout), buf, n);
        }
        //如果有来自终端的输入
        //cout << "mark 3" << endl;
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
            //reset timer
            new_value.it_value.tv_sec = 5;
            if (timerfd_settime(timerfd, 0, &new_value, NULL) == -1)
                perror("timerfd_settime");

            //cout << "mark 4 " << endl;
            //send keyborad input to server
            //cout << "Input payload size: " << n << endl; 
            char* attr = AttrGen(4, n, buf);
            int lens = 0;
            char* SBCP = SBCPGen(3, SEND, {attr}, lens);
            write(sockfd, SBCP, lens);
            free(attr);
            free(SBCP);
        }

        //If timer out, set IDLE
        if(FD_ISSET(timerfd, &rset)){
            ssize_t s = read(timerfd, &expTime, sizeof(uint64_t));
            cout << "Client is IDLE!!!!" << endl;
            new_value.it_value.tv_sec = 0;
            if (timerfd_settime(timerfd, 0, &new_value, NULL) == -1)
                perror("timerfd_settime");
        }
    }
}
