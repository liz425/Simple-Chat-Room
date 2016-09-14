#include <vector>
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
