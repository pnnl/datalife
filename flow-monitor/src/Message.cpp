#include "Message.h"
#include "Config.h"
#include "Connection.h"
// #include "Loggable.h"
//#include "ErrorTester.h"
#include <string.h>
#include <string>

void printMsgHeader(char *pkt) {
    msgHeader *header = (msgHeader *)pkt;
    std::stringstream ss;
    switch (header->type) {
    case OPEN_FILE_MSG:
        ss << "OPEN_FILE_MSG ";
        break;
    case REQ_FILE_SIZE_MSG:
        ss << "REQ_FILE_SIZE_MSG ";
        break;
    case FILE_SIZE_MSG:
        ss << "FILE_SIZE_MSG ";
        break;
    case REQ_BLK_MSG:
        ss << "REQ_BLK_MSG ";
        break;
    case SEND_BLK_MSG:
        ss << "SEND_BLK_MSG ";
        break;
    case CLOSE_FILE_MSG:
        ss << "CLOSE_FILE_MSG ";
        break;
    case CLOSE_CON_MSG:
        ss << "CLOSE_CON_MSG ";
        break;
    case PING_MSG:
        ss << "PING_MSG ";
        break;
    case WRITE_MSG:
        ss << "WRITE_MSG ";
        break;
    case ACK_MSG:
        ss << "ACK_MSG ";
        break;
    default:
        break;
    }
    ss << header->magic << " " << header->fileNameSize << " " << header->size << std::endl;
    PRINTF("%s", ss.str().c_str());
}

bool checkMsg(char *pkt, unsigned int size) {
    msgHeader *header = (msgHeader *)pkt;
    if (header->magic == MAGIC) {
        if (header->type >= OPEN_FILE_MSG && header->type <= ACK_MSG) {
            if (size == header->size) {
                unsigned int checkSize = header->fileNameSize;
                switch (header->type) {
                case OPEN_FILE_MSG:
                    checkSize += sizeof(openFileMsg);
                    break;
                case REQ_FILE_SIZE_MSG:
                    checkSize += sizeof(requestFileSizeMsg);
                    break;
                case FILE_SIZE_MSG:
                    checkSize += sizeof(fileSizeMsg);
                    break;
                case REQ_BLK_MSG:
                    checkSize += sizeof(requestBlkMsg);
                    break;
                case SEND_BLK_MSG: {
                    sendBlkMsg *packet = (sendBlkMsg *)pkt;
                    checkSize += sizeof(sendBlkMsg) + packet->dataSize;
                    break;
                }
                case CLOSE_FILE_MSG:
                    checkSize += sizeof(closeFileMsg);
                    break;
                case CLOSE_CON_MSG:
                    checkSize += sizeof(closeConMsg);
                    break;
                case WRITE_MSG: {
                    writeMsg *packet = (writeMsg *)pkt;
                    checkSize += sizeof(writeMsg) + packet->dataSize;
                    //                        PRINTF("sizeof(writeMsg): %u packet->dataSize %u recSize: %u\n", sizeof(writeMsg), packet->dataSize, checkSize);
                    break;
                }
                case PING_MSG:
                    checkSize += sizeof(msgHeader);
                    break;
                case ACK_MSG:
                    checkSize += sizeof(ackMsg);
                    break;
                case CLOSE_SERVER_MSG:
                    checkSize += sizeof(closeServerMsg);
                    break;
                default:
                    checkSize = 0;
                }
                return (checkSize == size);
            }
            else
                PRINTF("FAILED SIZE %u vs %u\n", size, header->size);
        }
        else
            PRINTF("FAILED TYPE\n");
    }
    else
        PRINTF("FAILED MAGIC\n");
    return false;
}

msgType getMsgType(char *pkt) {
    msgHeader *header = (msgHeader *)pkt;
    return header->type;
}

void fillMsgHeader(char *buff, msgType type, unsigned int fileNameSize, unsigned int size) {
    msgHeader *header = (msgHeader *)buff;
    header->magic = MAGIC;
    header->type = type;
    header->fileNameSize = fileNameSize;
    header->size = size;
}

bool clientSendRetry(Connection *connection, char *buff, unsigned int size) {
    bool ret = false;
    for (unsigned int i = 0; i < Config::socketRetry; i++) {
        ret = (size == connection->sendMsg(buff, size));
        if (ret)
            break;
        else {
            connection->restartSocket();
        }
    }
    return ret;
}

bool serverSendClose(Connection *connection, char *buff, unsigned int size) {
    if (size != connection->sendMsg(buff, size)) {
        connection->closeSocket();
        return false;
    }
    return true;
}

bool serverSendCloseNew(Connection *connection, sendBlkMsg *packet, std::string name, char *buff, unsigned int size) {

    if (sizeof(sendBlkMsg) != connection->sendMsg((char *)packet, sizeof(sendBlkMsg))) {
        connection->closeSocket();
        return false;
    }
    if ((int64_t)(name.length() + 1) != connection->sendMsg((char *)name.c_str(), name.length() + 1)) {
        connection->closeSocket();
        return false;
    }
    if (size != connection->sendMsg(buff, size)) {
        connection->closeSocket();
        return false;
    }
    return true;
}

int64_t clientRecRetry(Connection *connection, char **buff) {
    int64_t ret = connection->recvMsg(buff);
    if (ret < 0) {
        for (unsigned int i = 0; i < Config::socketRetry; i++) {
            if (connection->restartSocket())
                break;
        }
        return -1;
    }
    return ret;
}

int64_t clientRecRetryCount(Connection *connection, char *buff, int64_t size) {
    int64_t ret = connection->recvMsg(buff, size);
    if (ret != size) {
        for (unsigned int i = 0; i < Config::socketRetry; i++) {
            if (connection->restartSocket())
                break;
        }
        return -1;
    }
    return ret;
}

/*------------Client to server messages------------*/
//-------------Open a file
bool sendOpenFileMsg(Connection *connection, std::string name, unsigned int blkSize, bool compress, bool output) {
    unsigned int fileNameSize = name.size() + 1;
    unsigned int size = sizeof(openFileMsg) + fileNameSize;
    char *buff = new char[size];
    fillMsgHeader(buff, OPEN_FILE_MSG, fileNameSize, size);
    openFileMsg *packet = (openFileMsg *)buff;
    packet->blkSize = blkSize;
    packet->compress = compress;
    packet->output = output;
    name.copy((char *)(packet + 1), name.size());
    buff[size - 1] = '\0';
    //    bool ret = (size == connection->sendMsg(buff, size));
    bool ret = clientSendRetry(connection, buff, size);
    delete[] buff;
    return ret;
}

std::string parseOpenFileMsg(char *pkt, unsigned int &blkSize, bool &compress, bool &output) {
    openFileMsg *packet = (openFileMsg *)pkt;
    blkSize = packet->blkSize;
    compress = packet->compress;
    output = packet->output;
    std::string name(packet->name);
    return name;
}
//-------------Request a block
bool sendRequestBlkMsg(Connection *connection, std::string name, unsigned int start, unsigned int end) {
    unsigned int fileNameSize = name.size() + 1;
    unsigned int size = sizeof(requestBlkMsg) + fileNameSize;
    char *buff = new char[size];
    fillMsgHeader(buff, REQ_BLK_MSG, fileNameSize, size);
    requestBlkMsg *packet = (requestBlkMsg *)buff;
    packet->start = start;
    packet->end = end;
    name.copy((char *)(packet + 1), name.size());
    buff[size - 1] = '\0';
    //    bool ret = (size == connection->sendMsg(buff, size));
    bool ret = clientSendRetry(connection, buff, size);
    delete[] buff;
    return ret;
}

std::string parseRequestBlkMsg(char *pkt, unsigned int &start, unsigned int &end) {
    requestBlkMsg *packet = (requestBlkMsg *)pkt;
    start = packet->start;
    end = packet->end;
    std::string name(packet->name);
    return name;
}
//-------------Close a file
bool sendCloseFileMsg(Connection *connection, std::string name) {
    unsigned int fileNameSize = name.size() + 1;
    unsigned int size = sizeof(closeFileMsg) + fileNameSize;
    char *buff = new char[size];
    fillMsgHeader(buff, CLOSE_FILE_MSG, fileNameSize, size);
    closeFileMsg *packet = (closeFileMsg *)buff;
    name.copy((char *)(packet + 1), name.size());
    buff[size - 1] = '\0';
    //    bool ret = (size == connection->sendMsg(buff, size));
    bool ret = clientSendRetry(connection, buff, size);
    delete[] buff;
    return ret;
}

std::string parseCloseFileMsg(char *pkt) {
    closeFileMsg *packet = (closeFileMsg *)pkt;
    std::string name(packet->name);
    return name;
}
//-------------Request file size
bool sendRequestFileSizeMsg(Connection *connection, std::string name) {
    unsigned int fileNameSize = name.size() + 1;
    unsigned int size = sizeof(requestFileSizeMsg) + fileNameSize;
    char *buff = new char[size];
    fillMsgHeader(buff, REQ_FILE_SIZE_MSG, fileNameSize, size);
    requestFileSizeMsg *packet = (requestFileSizeMsg *)buff;
    name.copy((char *)(packet + 1), name.size());
    buff[size - 1] = '\0';
    //    bool ret = (size == connection->sendMsg(buff, size));
    bool ret = clientSendRetry(connection, buff, size);
    delete[] buff;
    return ret;
}

std::string parseRequestFileSizeMsg(char *pkt) {
    requestFileSizeMsg *packet = (requestFileSizeMsg *)pkt;
    std::string name(packet->name);
    return name;
}
//-------------Close socket
bool sendCloseConMsg(Connection *connection) {
    unsigned int size = sizeof(closeConMsg);
    closeConMsg packet;
    fillMsgHeader((char *)&packet, CLOSE_CON_MSG, 0, size);
    //    return (size == connection->sendMsg((char*)&packet, size));
    //Does this make sense... prob not just let it be closed
    return clientSendRetry(connection, (char *)&packet, size);
}
//-------------Close server
bool sendCloseServerMsg(Connection *connection) {
    unsigned int size = sizeof(closeConMsg);
    closeServerMsg packet;
    fillMsgHeader((char *)&packet, CLOSE_SERVER_MSG, 0, size);
    //    return (size == connection->sendMsg((char*)&packet, size));
    //Does this make sense... prob not just let it be closed
    return clientSendRetry(connection, (char *)&packet, size);
}

bool sendPingMsg(Connection *connection) {
    unsigned int size = sizeof(msgHeader);
    msgHeader packet;
    fillMsgHeader((char *)&packet, PING_MSG, 0, size);
    return clientSendRetry(connection, (char *)&packet, size);
}

bool sendWriteMsg(Connection *connection, std::string name, char *pkt, unsigned int dataSize, unsigned int compSize, uint64_t fp, unsigned int sn) {
    unsigned int fileNameSize = name.size() + 1;
    unsigned int size = sizeof(writeMsg) + fileNameSize + dataSize;
    fillMsgHeader(pkt, WRITE_MSG, fileNameSize, size);
    writeMsg *packet = (writeMsg *)pkt;
    packet->dataSize = dataSize;
    packet->compSize = compSize;
    packet->fp = fp;
    packet->sn = sn;
    PRINTF("Send fp size: %lu sn: %u\n", packet->fp, sn);
    name.copy((char *)(packet + 1), name.size());
    packet->data[fileNameSize - 1] = '\0';
    bool ret = clientSendRetry(connection, (char *)packet, size);
    delete[] pkt;
    return ret;
}

std::string parseWriteMsg(char *pkt, char **data, unsigned int &dataSize, unsigned int &compSize, uint64_t &fp) {
    writeMsg *packet = (writeMsg *)pkt;
    dataSize = packet->dataSize;
    compSize = packet->compSize;
    fp = packet->fp;
    PRINTF("Rec fp size: %lu vs %lu %u\n", fp, packet->fp, packet->sn);
    (*data) = &packet->data[packet->header.fileNameSize];
    std::string name(packet->data);
    return name;
}

/*------------Server to Client messages------------*/
//-------------Send file size
bool sendFileSizeMsg(Connection *connection, std::string name, uint64_t fileSize, uint8_t open) {
    unsigned int fileNameSize = name.size() + 1;
    unsigned int size = sizeof(fileSizeMsg) + fileNameSize;
    char *buff = new char[size];
    fillMsgHeader(buff, FILE_SIZE_MSG, fileNameSize, size);
    fileSizeMsg *packet = (fileSizeMsg *)buff;
    packet->fileSize = fileSize;
    packet->open = open;
    name.copy((char *)(packet + 1), name.size());
    buff[size - 1] = '\0';
    //    bool ret = (size == connection->sendMsg(buff, size));
    bool ret = serverSendClose(connection, buff, size);
    delete[] buff;
    return ret;
}

std::string parseFileSizeMsg(char *pkt, uint64_t &fileSize, uint8_t &open) {
    fileSizeMsg *packet = (fileSizeMsg *)pkt;
    fileSize = packet->fileSize;
    open = packet->open;
    std::string name(packet->name);
    return name;
}

bool recFileSizeMsg(Connection *connection, uint64_t &fileSize) {
    fileSize = 0;
    uint8_t opened;
    char *buff = NULL;
    int64_t ret = clientRecRetry(connection, &buff);
    if (ret > (int64_t)sizeof(struct fileSizeMsg)) { //if less, probably a malformed packet
        parseFileSizeMsg(buff, fileSize, opened);
        delete[] buff;
        return true;
    }
    else if (ret > 0 && buff) {
        delete[] buff;
    }
    return false;
}
//-------------Send block of file
bool sendSendBlkMsg(Connection *connection, std::string name, unsigned int blk, char *data, unsigned int dataSize) {
    unsigned int fileNameSize = name.size() + 1;
    unsigned int size = sizeof(sendBlkMsg) + fileNameSize + dataSize;
    char *buff = new char[size];
    fillMsgHeader(buff, SEND_BLK_MSG, fileNameSize, size);
    sendBlkMsg *packet = (sendBlkMsg *)buff;
    packet->blk = blk;

    //Copy file name
    char *ptr = (char *)(packet + 1);
    name.copy(ptr, name.size());
    ptr += name.size();

    //Place null terminator
    *ptr = '\0';
    ptr++;

    //Copy data
    memcpy(ptr, data, dataSize);
    //    bool ret = (size == connection->sendMsg(buff, size));
    bool ret = serverSendClose(connection, buff, size);
    delete[] buff;
    return ret;
}

//This is a special one that should eliminate an extra memcpy
std::string recSendBlkMsg(Connection *connection, char **data, unsigned int &blk, unsigned int &dataSize, unsigned int dataBufSize) {
    blk = 0;
    dataSize = 0;
    sendBlkMsg msg;
    //    int64_t retMsgSize = connection->recvMsg((char*)&msg, sizeof(sendBlkMsg));
    int64_t retMsgSize = clientRecRetryCount(connection, (char *)&msg, sizeof(sendBlkMsg));
    //    std::cout<<"[MONITOR] " << "Msg Size: " << retMsgSize << std::endl;
    if (retMsgSize < 0) //Make sure rec was successful
        return std::string();

    char *namebuf = new char[msg.header.fileNameSize];
    //    int retFileNameSize = connection->recvMsg(namebuf, msg.header.fileNameSize);
    int64_t retFileNameSize = clientRecRetryCount(connection, namebuf, msg.header.fileNameSize);
    //    std::cout<<"[MONITOR] " << "File Name Size: " << retFileNameSize << std::endl;
    if (retFileNameSize < 0) {
        delete[] namebuf;
        return std::string();
    }
    std::string fileName(namebuf, msg.header.fileNameSize);
    delete[] namebuf;

    bool created = (*data == NULL);
    if (created) {
        if (dataBufSize == 0) {
            dataBufSize = msg.dataSize;
        }
        *data = new char[dataBufSize];
    }
    if (msg.dataSize > dataBufSize){
        std::cout <<"Message size larger than expected: "<<msg.dataSize <<" "<<dataBufSize<<std::endl;
         if (created) {
            delete[] * data;
            *data = NULL;
        }
        return std::string();
    }

    //    int retDataSize = connection->recvMsg(*data, msg.dataSize);
    int64_t retDataSize = clientRecRetryCount(connection, *data, msg.dataSize);
    //    std::cout<<"[MONITOR] " << "Data Size: " << retDataSize << std::endl;
    if (retDataSize < 0) {
        if (created) {
            delete[] * data;
            *data = NULL;
        }
        return std::string();
    }
    if (retMsgSize == (int64_t)sizeof(sendBlkMsg) &&
        retFileNameSize == (int64_t)msg.header.fileNameSize &&
        retDataSize == (int64_t)msg.dataSize) { //this is the path we want to take?
        dataSize = msg.dataSize;
        blk = msg.blk;
        return fileName; 
    }

    if (created) { //No memory leak here!
        delete[] * data;
        *data = NULL;
    }
    return fileName;
}
//-------------Send an ack msg
bool sendAckMsg(Connection *connection, msgType ackType) {
    unsigned int size = sizeof(ackMsg);
    char *buff = new char[size];
    fillMsgHeader(buff, ACK_MSG, 0, size);
    ackMsg *packet = (ackMsg *)buff;
    packet->ackType = ackType;
    //    bool ret = (size == connection->sendMsg(buff, size));
    bool ret = serverSendClose(connection, buff, size);
    delete[] buff;
    return ret;
}

bool recAckMsg(Connection *connection, msgType expMstType) {
    ackMsg msg;
    int64_t retMsgSize = connection->recvMsg((char *)&msg, sizeof(ackMsg));
    return (retMsgSize == sizeof(ackMsg) && expMstType == msg.ackType);
}

int pollWrapper(Connection *connection) {
    return connection->pollMsg();
}

int64_t pollRecWrapper(Connection *connection, char **buff) {
    return connection->recvMsg(buff);
}