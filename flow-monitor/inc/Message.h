#ifndef MESSAGE_H
#define MESSAGE_H

#include "Connection.h"

#define MAGIC 0x32ab4fd

enum msgType {
    OPEN_FILE_MSG = 0,
    REQ_FILE_SIZE_MSG,
    FILE_SIZE_MSG,
    REQ_BLK_MSG,
    SEND_BLK_MSG,
    CLOSE_FILE_MSG,
    CLOSE_CON_MSG,
    CLOSE_SERVER_MSG,
    PING_MSG,
    WRITE_MSG,
    ACK_MSG
};

#pragma pack(push, 1)

struct msgHeader {
    uint64_t magic;
    msgType type;
    uint32_t fileNameSize;
    uint32_t size;
};

struct openFileMsg {
    msgHeader header;
    uint32_t blkSize;
    uint8_t compress;
    uint8_t output;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    char name[];
#pragma GCC diagnostic pop
};

struct fileSizeMsg {
    msgHeader header;
    uint64_t fileSize;
    uint8_t open;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    char name[];
#pragma GCC diagnostic pop
};

struct requestFileSizeMsg {
    msgHeader header;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    char name[];
#pragma GCC diagnostic pop
};

struct requestBlkMsg {
    msgHeader header;
    uint32_t start;
    uint32_t end;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    char name[];
#pragma GCC diagnostic pop
};

struct sendBlkMsg {
    msgHeader header;
    int compression;
    uint32_t blk;
    uint32_t dataSize;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    char data[];
#pragma GCC diagnostic pop
};

struct closeFileMsg {
    msgHeader header;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    char name[];
#pragma GCC diagnostic pop
};

struct closeConMsg {
    msgHeader header;
};

struct closeServerMsg {
    msgHeader header;
};

struct writeMsg {
    msgHeader header;
    uint32_t dataSize;
    uint32_t compSize;
    uint64_t fp;
    unsigned int sn;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    char data[];
#pragma GCC diagnostic pop
};

struct ackMsg {
    msgHeader header;
    msgType ackType;
};

#pragma pack(pop)

void printMsgHeader(char *pkt);
msgType getMsgType(char *msg);
bool checkMsg(char *pkt, unsigned int size);
void fillMsgHeader(char *buff, msgType type, unsigned int fileNameSize, unsigned int size);

//These are wrappers to support fault tolerance support stuff...
bool clientSendRetry(Connection *connection, char *buff, unsigned int size);
bool serverSendClose(Connection *connection, char *buff, unsigned int size);
bool serverSendCloseNew(Connection *connection, sendBlkMsg *packet, std::string name, char *buff, unsigned int size);
int64_t clientRecRetry(Connection *connection, char **buff);
int64_t clientRecRetryCount(Connection *connection, char *buff, int64_t size);

bool sendOpenFileMsg(Connection *connection, std::string name, unsigned int blkSize, bool compress, bool output);
std::string parseOpenFileMsg(char *pkt, unsigned int &blkSize, bool &compress, bool &output);

bool sendRequestBlkMsg(Connection *connection, std::string name, unsigned int start, unsigned int end);
std::string parseRequestBlkMsg(char *pkt, unsigned int &start, unsigned int &end);

bool sendCloseFileMsg(Connection *connection, std::string name);
std::string parseCloseFileMsg(char *pkt);

bool sendCloseConMsg(Connection *connection);

bool sendFileSizeMsg(Connection *connection, std::string name, uint64_t fileSize, uint8_t open);
std::string parseFileSizeMsg(char *pkt, uint64_t &fileSize, uint8_t &open);
bool recFileSizeMsg(Connection *connection, uint64_t &fileSize);

bool sendSendBlkMsg(Connection *connection, std::string name, unsigned int blk, char *data, unsigned int dataSize);
std::string recSendBlkMsg(Connection *connection, char **data, unsigned int &blk, unsigned int &dataSize, unsigned int dataBufSize = 0);

bool sendAckMsg(Connection *connection, msgType ackType);
bool recAckMsg(Connection *connection, msgType expMstType);

bool sendRequestFileSizeMsg(Connection *connection, std::string name);
std::string parseRequestFileSizeMsg(char *pkt);

bool sendCloseServerMsg(Connection *connection);

bool sendPingMsg(Connection *connection);

bool sendWriteMsg(Connection *connection, std::string name, char *data, unsigned int dataSize, unsigned int compSize, uint64_t fp, unsigned int sn);
std::string parseWriteMsg(char *pkt, char **data, unsigned int &dataSize, unsigned int &compSize, uint64_t &fp);

int pollWrapper(Connection *connection);
int64_t pollRecWrapper(Connection *connection, char **buff);
#endif /* MESSAGE_H */
