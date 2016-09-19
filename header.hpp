#include <vector>
using namespace std;

#define MAXLINE 4096
#define JOIN 2
#define FWD 3
#define SEND 4
#define NAK 5
#define OFFLINE 6
#define ACK 7
#define ONLINE 8
#define IDLE 9


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

char* AttrGen(uint16_t type, uint16_t payloadSize, char* payload){
    char *attr = (char*)malloc(sizeof(SBCPAttrHeader) + payloadSize);
    SBCPAttrHeader attrheader;
    attrheader.type = htons(type);
    attrheader.length = htons(4 + payloadSize);
    memcpy(attr, &attrheader, 4);
    memcpy(attr + 4, payload, payloadSize);
    return attr;
}

char* SBCPGen(uint16_t version, uint16_t type, vector<char*> payloads, int& length){
    uint16_t payloadsSize = 0;
    vector<uint16_t> sizeVec;
    for(auto& payload : payloads){
        SBCPAttrHeader attrheader;
        memcpy(&attrheader, payload, 4);
        uint16_t len = ntohs(attrheader.length);
        sizeVec.push_back(len);
        payloadsSize += sizeVec.back();
    }
    char* SBCP = (char*)malloc(sizeof(SBCPHeader) + payloadsSize);
    SBCPHeader header;
    header.vrsn = (version & 0xfffe) >> 1;
    header.type = ((version & 0x1) << 7) + type;
    header.length = htons(4 + payloadsSize);
    //printf("%x\n", header.length);
    length = payloadsSize + 4;
    memcpy(SBCP, &header, 4);
    for(int i = 0; i < payloads.size(); ++i){
        memcpy(SBCP + i * 4 + 4, payloads[i], sizeVec[i]);
    }
    return SBCP;
}

vector<char*> unpackMessage(char* SBCP, int& type){
    SBCPHeader header;
    memcpy(&header, SBCP, 4);
    int SBCPLen = ntohs(header.length);
    vector<char*> packs;
    
    type = static_cast<int>((header.type) & 0x7f);
    int version = static_cast<int>((header.vrsn << 1) + ((header.type & 0x80) >> 7));
    if(version != 3)
        return packs;
    int cnt = 4;
    cout << "SCBPLen: " << SBCPLen << endl;
    while(cnt < SBCPLen){
        SBCPAttrHeader attrheader;
        memcpy(&attrheader, SBCP + cnt, 4);
        int attrLen = ntohs(attrheader.length);
        char* attr = (char*)malloc(attrLen);
        memcpy(attr, SBCP + cnt, attrLen);
        packs.push_back(attr);
        cnt += attrLen;
    }
    cout << "SBCP unpack success." << endl;
    return packs;
}

char* unpackAttr(char* attr, int& type, int& payloadLen){
    SBCPAttrHeader header;
    memcpy(&header, attr, 4);
    type  = ntohs(header.type);
    payloadLen = ntohs(header.length) - 4;
    char* payload = (char*)malloc(payloadLen);
    memcpy(payload, attr + 4, payloadLen);
    return payload;
}