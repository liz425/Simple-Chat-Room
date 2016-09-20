#include "header.hpp"
using namespace std;

int state = 0;
#define INIT 0
#define WAITACK 1


in_port_t SERV_PORT = 8888;
string addrStr = "127.0.0.1";

unordered_map<string, int> userToSock;
unordered_map<int, string> sockToUser;


int main(int argc ,char *argv[]){
	int i;
	int listenfd, connfd, sockfd;
	int maxfd, maxi, nready, client[FD_SETSIZE];
	char buf[MAXLINE];
	struct sockaddr_in cliaddr, servaddr;
	socklen_t clilen;
	ssize_t n;
	fd_set rset, allset;

    if(argc != 4){
        printf("Command arguments ERROR!\n");
        printf("Usage: ./Server <server_ip> <server_port> <max_clients>\n");
        return 0;
    }

    addrStr = string(argv[1]);
    const char *SERV_ADDR = addrStr.c_str();
    SERV_PORT = stoi(string(argv[2]));

#define MAXCLIENTS atoi(argv[3])

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&servaddr, 0, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, SERV_ADDR, &servaddr.sin_addr);
	
	int s = ::bind(listenfd, (struct sockaddr*)(&servaddr), sizeof(servaddr));
    if(s != 0)
		printf("bind error\n");
    
	
	listen(listenfd, 1024);
	
	//客户端描述符存储在client中,maxi表示该数组最大的存有客户端描述符的数组下标
	maxfd = listenfd;
	maxi = -1;
	memset(client, -1, sizeof client);
	//初始化读就绪的fd_set数组，并监听listen描述符
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);

	for (;;) {
		//allset是监控的描述符列表，rset是可读描述符列表
		rset = allset;
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);
		//如果listen描述符可读，说明有客户端连接
		if (FD_ISSET(listenfd, &rset)) {
			clilen = sizeof cliaddr;
			connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
			if (connfd == -1){
                perror("accept error\n");
                return 0;
            }else{ 
                printf("%d accepted!\n", connfd);
            }

			//扫描client数组，找到下标最小的未用的来存客户端描述符
			for (i = 0; i < FD_SETSIZE; i++) if (client[i] < 0) {
				client[i] = connfd;
				break;
			}
			if (i == FD_SETSIZE) perror("Too many clients\n");
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
					if (n < 0) 
                        perror("read error\n");
				} else {
                    int SBCPType = 0;
                    vector<char*> inAttrs = unpackMessage(buf, SBCPType);
                    int attrType = 0;
                    int payloadLen = 0;
                    char* payload = unpackAttr(inAttrs[0], attrType, payloadLen);
                    
                    vector<char*> outAttrs;
                    int resType = 0;
                    string username;

                    if(SBCPType == JOIN){
                        if(userToSock.size() < MAXCLIENTS){
                            username = string(payload);
                            if(userToSock.find(username) == userToSock.end()){
                                //register user
                                userToSock[username] = sockfd;
                                sockToUser[sockfd] = username;
                                cout << "Client \'" + username + "\' connected." << endl;
                                //accept with ACK, including user count
                                resType = ACK;
                                char* resBuf = (char*)malloc(2);
                                uint16_t clinum = htons((uint16_t)userToSock.size());
                                memcpy(resBuf, &clinum, 2);
                                char* attr = AttrGen(ATTRCLICNT, 2, resBuf);
                                outAttrs.push_back(attr);
                                for(auto& user : userToSock){
                                    string name = user.first;
                                    char* nameBuf = (char*)name.c_str();
                                    attr = AttrGen(ATTRUSER, name.size(), nameBuf);
                                    outAttrs.push_back(attr);
                                }
                            }else{
                                //refuse with NAK: user already logged in
                                resType = NAK;
                                string tmp = "User \'" + username + "\' already logged in.";
                                char* resBuf = (char*)tmp.c_str();
                                char* attr = AttrGen(ATTRREASON, tmp.size(), resBuf);
                                outAttrs.push_back(attr);
                            }
                        }else{
                            //refuse with NAK: exceed maximum user number
                            resType = NAK;
                            string tmp = "Maximum clients limited.";
                            char* resBuf = (char*)tmp.c_str();
                            char* attr = AttrGen(ATTRREASON, tmp.size(), resBuf);
                            outAttrs.push_back(attr);
                        }
                    }else if(SBCPType == SEND){
                        cout << "Received SEND message, FWDing" << endl;
                        resType = FWD;
                        username = sockToUser[sockfd];
                        char* resBuf = (char*)username.c_str();
                        char* attr = AttrGen(ATTRREASON, username.size(), resBuf);
                        outAttrs = inAttrs;
                        outAttrs.insert(outAttrs.begin(), attr);
                    }
                    
                    int lens = 0;
                    char* SBCP = SBCPGen(3, resType, outAttrs, lens);
                    if(resType != FWD){
                        write(sockfd, SBCP, lens);
                        for(auto& item : inAttrs){
                            free(item);
                        }
                    }else{
                        for(auto& item : sockToUser){
                            if(item.first == sockfd)
                                continue;
                            write(item.first, SBCP, lens);
                        }
                    }
                    free(SBCP);
                    for(auto& item : outAttrs){
                        free(item);
                    }
				}
				if (--nready <= 0) break;
			}
		}
	}
	return 0;
}