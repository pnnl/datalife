#include "Connection.h"
#include "Config.h"
//#include "ErrorTester.h"
#include "Message.h"
#include "Timer.h"
#include <signal.h>
#include <sstream>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define TIMEON(...) __VA_ARGS__
//#define TIMEON(...)

thread_local int _tlSocket = -1;
thread_local int _tlSocketsClosed = 0;

thread_local uint64_t _tlSocketTime = 0;
thread_local uint64_t _tlSocketBytes = 0;

thread_local std::vector<char> _buffer(Config::defaultBufferSize);
thread_local size_t _bufferSize = Config::defaultBufferSize;

Connection::Connection(std::string hostAddr, int port) : //Client
                                                         Loggable(Config::ClientConLog, "ClientConLog"),
                                                         _numSockets(0),
                                                         _nextSocket(0),
                                                         _inMsgs(-1),
                                                         _addr(hostAddr),
                                                         _port(port),
                                                         _addrport(hostAddr + ":" + std::to_string(port)),
                                                         _isServer(false),
                                                         _consecutiveCnt(0) {
}

Connection::Connection(int sock, std::string clientAddr, int port) : //Server
                                                                     Loggable(Config::ServerConLog, "ServerConLog"),
                                                                     _numSockets(0),
                                                                     _nextSocket(0),
                                                                     _inMsgs(-1),
                                                                     _addr(clientAddr),
                                                                     _port(port),
                                                                     _addrport(clientAddr + ":" + std::to_string(port)),
                                                                     _isServer(true),
                                                                     _consecutiveCnt(0) {
    std::unique_lock<std::mutex> lock(_sMutex);
    addSocket(sock); //Add socket to poll
    lock.unlock();
    log(this) << "created: " << _addr << ":" << _port << std::endl;
}

Connection::~Connection() {
    //No one should have access to this but lets lock it anyways
    // std::cout << "deleting connection" << std::endl;
    std::unique_lock<std::mutex> lock(_sMutex);
    unsigned int socketsClosed = 0;
    if (isServer()) {
        log(this) << _addr << " Server closing " << _pfds.size() << std::endl;
        for (unsigned int i = 0; i < _pfds.size(); i++) {
            if (closeSocket(_pfds[i].fd) == 0)
                socketsClosed++;
        }
        log(this) << "closed: " << _addr << ":" << _port << " num sockets" << socketsClosed << std::endl;
    }
    else {
        while (_numSockets.load() && _sockets.size()) {
            log(this) << _addr << " Client closing " << _numSockets.load() << " " << _sockets.size() << std::endl;
            if (closeSocket(_sockets.front()) == 0)
                socketsClosed++;
            _sockets.pop();
        }
    }
    lock.unlock();
    log(this) << _addr << " Destroying connection closed " << socketsClosed << " sockets" << std::endl;
}

/*This lock is special and works differently depending on if it is client/server
 * Server: Works like a regular lock
 * Client: Pops a socket and sends thread local _tlSocket for sends and recvs
 * */
void Connection::lock() {

    if (isServer()) {
        _sMutex.lock();
        _tlSocketsClosed = 0;
    }
    else {
        _tlSocket = -1;
        while (_tlSocket < 0) {
            std::unique_lock<std::mutex> lock(_sMutex);
            if (_sockets.empty()) {
                _cv.wait(lock);
            }

            if (!_sockets.empty()) {
                _tlSocket = _sockets.front();
                _sockets.pop();
            }
            lock.unlock();
        }
        TIMEON(_tlSocketBytes = 0);
        TIMEON(_tlSocketTime = Timer::getCurrentTime());
    }
}

/*This lock is special and works differently depending on if it is client/server
 * Server: Works like a regular lock, returns number of sockets closed
 * Client: Pushes the thread local _tlSocket to socket queue and sets _tlSocket = -1
 * */
int Connection::unlock() {
    if (isServer()) {
        _sMutex.unlock();
        return _tlSocketsClosed;
    }
    else {
        TIMEON(_tlSocketTime = Timer::getCurrentTime() - _tlSocketTime);
        if (_tlSocket >= 0) {
            std::unique_lock<std::mutex> lock(_sMutex);
            _sockets.push(_tlSocket);
            lock.unlock();
            _cv.notify_one();
            _tlSocket = -1;
        }
    }
    return 0;
}

void Connection::incCnt() {
    _consecutiveCnt++;
}

void Connection::setCnt(uint64_t cnt) {
    _consecutiveCnt = cnt;
}

void Connection::clearCnt() {
    _consecutiveCnt = 0;
}

uint64_t Connection::consecutiveCnt() {
    return _consecutiveCnt;
}

int Connection::initializeSocket() {
    unsigned int retry = 0;
    int sock = -1;
    while (sock < 0) {
        if (retry > Config::maxConRetry) {
            log(this) << _addr << " ERROR: max retrys reached: exiting application " << std::endl;
            return -1;
        }
        sock = getOutSocket(_addr, _port);
        log(this) << "RETRYING Socket Connection: " << sock << " " << retry << " " << Config::maxConRetry << " " << isClient() << std::endl;
        retry++;
    }
    return sock;
}

//Must lock _sMutex
bool Connection::addSocket() {
    bool ret = false;
    if (isClient()) { //Client only
        int sock = initializeSocket();
        if (sock > 0) {
            log(this) << "New Socket: " << sock << std::endl;
            _sockets.push(sock);
            _numSockets.fetch_add(1);
            ret = true;
        }
    }
    return ret;
}

//Must lock _sMutex
bool Connection::addSocket(int socket) {
    bool ret = false;
    if (isServer()) { //Server only
        if (_pfds.size()) {
            for (unsigned int i = 0; i < _pfds.size(); i++) {
                if (_pfds[i].fd == -1) {
                    _pfds[i].fd = socket;
                    _pfds[i].events = POLLIN;
                    _pfds[i].revents = 0;
                    ret = true;
                    break;
                }
            }
        }

        if (!ret) {
            struct pollfd pfd;
            pfd.fd = socket;
            pfd.events = POLLIN;
            pfd.revents = 0;
            _pfds.push_back(pfd);
            ret = true;
        }
        _numSockets.fetch_add(1);
        log(this) << "adding socket " << socket << std::endl;
    }
    return ret;
}

int Connection::forceCloseSocket(int &socket) {
    log(this) << "Actually closing socket " << socket << std::endl;
    return rclose(socket);
}

//Closes both server/client sockets
//Server should lock
int Connection::closeSocket(int &socket) {
    int localSocket = socket;
    log(this) << "closing socket? " << localSocket << std::endl;
    int ret = -1;
    if (localSocket != -1) {
        if (isServer()) { //Stop polling this socket if on server
            //No need to lock since we already locked in the server!
            for (unsigned int i = 0; i < _pfds.size(); i++) { //Iterate and look for pfd to turn off
                if (_pfds[i].fd == localSocket) {
                    socket = -1;
                    _pfds[i].fd = -1;
                    _pfds[i].events = 0;
                    _pfds[i].revents = 0;
                    _tlSocketsClosed++;
                    break;
                }
            }
            if (!rshutdown(localSocket, SHUT_WR)) { //Send a shutdown method to TCP
                log(this) << _addr << " Shutting down server socket " << localSocket << std::endl;
            }
        }
        else {
            int old = _tlSocket;
            _tlSocket = localSocket;
            if (sendCloseConMsg(this)) {
                log(this) << _addr << " " << _port << " Sent close socket msg sent" << localSocket << std::endl;
            }
            _tlSocket = old;
            if (!rshutdown(localSocket, SHUT_RD)) { //Send a shutdown method to TCP
                log(this) << _addr << " Shutting down client socket " << localSocket << std::endl;
            }
        }
        ret = forceCloseSocket(localSocket);
        _numSockets.fetch_sub(1);
    }
    return ret;
}

int Connection::closeSocket() {
    return closeSocket(_tlSocket);
}

bool Connection::restartSocket() {
    bool ret = false;
    if (_tlSocket > 0) {
        log(this) << "Restarting socket: " << _tlSocket << std::endl;
        forceCloseSocket(_tlSocket);
        int socket = initializeSocket();
        if (socket > 0)
            _tlSocket = socket;
        else {
            log(this) << "Failed to restart socket" << std::endl;
        }
    }
    return ret;
}

bool Connection::initiate(unsigned int numConnections) {
    if (!numConnections)
        numConnections = Config::socketsPerConnection;

    bool ret = false;
    std::unique_lock<std::mutex> lock(_sMutex);
    for (unsigned int i = 0; i < numConnections; i++) {
        ret |= addSocket();
    }
    lock.unlock();
    _cv.notify_all();
    return ret;
}

//Lock before using
int64_t Connection::sendMsg(void *msg, int64_t msgSize) {
    //    RANDOMERROR(-1);
    unsigned int retryCnt = 0;
    int64_t sentSize = 0;
    if (_tlSocket > -1) {
        while (sentSize < msgSize && retryCnt <= Config::messageRetry) {
            void *ptr = (void *)((char *)msg + sentSize);
            int64_t ret = rsend(_tlSocket, ptr, msgSize - sentSize, SOCKETSENDFLAGS);
            log(this) << "SEND " << isServer() << ": " << ret << " Retry " << retryCnt << " of " << Config::messageRetry << std::endl;
            if (ret < 0) {
                std::stringstream ss;
                ss << "Error: " << errno << " ";
                switch (errno) {
                case EACCES:
                    ss << "EACCES" << std::endl;
                    break;
                case EAGAIN:
                    ss << "EAGAIN" << std::endl;
                    break;
                case EBADF:
                    ss << "EBADF" << std::endl;
                    break;
                case ECONNRESET:
                    ss << "ECONNRESET" << std::endl;
                    break;
                case EDESTADDRREQ:
                    ss << "EDESTADDRREQ" << std::endl;
                    break;
                case EFAULT:
                    ss << "EFAULT" << std::endl;
                    break;
                case EINTR:
                    ss << "EINTR" << std::endl;
                    break;
                case EINVAL:
                    ss << "EINVAL" << std::endl;
                    break;
                case EISCONN:
                    ss << "EISCONN" << std::endl;
                    break;
                case EMSGSIZE:
                    ss << "EMSGSIZE" << std::endl;
                    break;
                case ENOBUFS:
                    ss << "ENOBUFS" << std::endl;
                    break;
                case ENOMEM:
                    ss << "ENOMEM" << std::endl;
                    break;
                case ENOTCONN:
                    ss << "ENOTCONN" << std::endl;
                    break;
                case ENOTSOCK:
                    ss << "ENOTSOCK" << std::endl;
                    break;
                case EOPNOTSUPP:
                    ss << "EOPNOTSUPP" << std::endl;
                    break;
                case EPIPE:
                    ss << "EPIPE" << std::endl;
                    break;
                default:
                    ss << "Unknown" << std::endl;
                }
                log(this) << ss.str();
                return -1;
            }
            else if (!ret) {
                retryCnt++;
                usleep(100);
            }
            else {
                sentSize += ret;
                retryCnt = 0;
            }
        }
    }
    else {
        log(this) << "Send: Thread Local Socket Not Set!!!" << std::endl;
        debug()<<"EXITING!!!!"<<__FILE__<<" "<<__LINE__<<std::endl;
        raise(SIGSEGV);
    }
    TIMEON(_tlSocketBytes += ((sentSize > 0) ? sentSize : 0));
    return sentSize;
}

//Lock before using
int64_t Connection::recvMsg(char *buf, int64_t bufSize) {
    unsigned int retryCnt = 0;
    int64_t recvSize = 0;
    if (_tlSocket > -1) {
        while (recvSize < bufSize && retryCnt <= Config::messageRetry) {
            void *ptr = (void *)(buf + recvSize);
            int64_t ret = rrecv(_tlSocket, ptr, bufSize - recvSize, 0);
            log(this) << "REC " << isServer() << ": " << ret << " Retry " << retryCnt << " " << Config::messageRetry << std::endl;
            if (ret < 0) {
                std::stringstream ss;
                ss << "Error: " << errno << " ";
                switch (errno) {
                case EAGAIN:
                    ss << "EAGAIN" << std::endl;
                    break;
                case EBADF:
                    ss << "EBADF" << std::endl;
                    break;
                case ECONNREFUSED:
                    ss << "ECONNREFUSED" << std::endl;
                    break;
                case EFAULT:
                    ss << "EFAULT" << std::endl;
                    break;
                case EINTR:
                    ss << "EINTR" << std::endl;
                    break;
                case EINVAL:
                    ss << "EINVAL " << _tlSocket << " " << bufSize << " " << recvSize << std::endl;
                    debug()<<"EXITING!!!!"<<__FILE__<<" "<<__LINE__<<std::endl;
                    raise(SIGSEGV);
                    break;
                case ENOMEM:
                    ss << "ENOMEM" << std::endl;
                    break;
                case ENOTCONN:
                    ss << "ENOTCONN" << std::endl;
                    break;
                case ENOTSOCK:
                    ss << "ENOTSOCK" << std::endl;
                    break;
                default:
                    ss << "Unknown" << std::endl;
                }
                log(this) << ss.str();
                if (errno != EINTR) {
                    return -1;
                }
                else { //ignore EINTR, retry
                    retryCnt++;
                    usleep(100);
                }
            }
            else if (!ret) {
                retryCnt++;
                usleep(100);
            }
            else {
                retryCnt = 0;
                recvSize += ret;
            }
        }
    }
    else {
        log(this) << "Recv: Thread Local Socket Not Set!!!" << std::endl;
        debug()<<"EXITING!!!!"<<__FILE__<<" "<<__LINE__<<std::endl;
        raise(SIGSEGV);
    }
    TIMEON(_tlSocketBytes += ((recvSize > 0) ? recvSize : 0));
    return recvSize;
}

//Lock before using
int64_t Connection::recvMsg(char **dataPtr) {
    *dataPtr = NULL;

    log(this) << "__buffer: " << _bufferSize << " sizeof: " << sizeof(msgHeader) << std::endl;
    //Read header only
    int64_t recSize = 0;
    int64_t temp = recvMsg(_buffer.data(), (int64_t)sizeof(msgHeader));
    if (temp == -1)
        return -1;
    recSize += temp;

    if (recSize > 0) {
        //Lets check to see if the buffer is big enough
        msgHeader *header = (msgHeader *)_buffer.data();
        log(this) << "Message size ----------- " << header->size << " " << header->type << " " << header->fileNameSize << " " << header->magic << std::endl;
        if (header->size > _bufferSize) {
            //Resize the vector to the needed size
            log(this) << _addr << " Resizing buffer to: " << header->size << " " << header->type << std::endl;
            _buffer.resize(header->size);
            _bufferSize = header->size;

            header = (msgHeader *)_buffer.data();
        }
        //Receive the rest of the message
        temp = recvMsg(_buffer.data() + sizeof(msgHeader), header->size - sizeof(msgHeader));
        if (temp == -1)
            return -1;
        recSize += temp;

        if (checkMsg(_buffer.data(), recSize)) {
            //Copy the data into a new buffer and return to caller
            *dataPtr = new char[header->size];
            memcpy(*dataPtr, _buffer.data(), header->size);
        }
        else {
            log(this) << _addr << " Bad message" << std::endl;
            recSize = -1;
        }
    }
    return recSize;
}

//Lock before using
int Connection::pollMsg() {
    if (isServer()) { //Only Server polls
        _tlSocket = -1;

        if (_inMsgs < 0) { //Poll in steps since we can only read 1024 at a time
            _inMsgs = 0;
            struct pollfd *pfds = _pfds.data();
            for (unsigned int i = 0; i < _pfds.size(); i += Config::socketStep) {
                unsigned int nfds = (i + Config::socketStep > _pfds.size()) ? _pfds.size() - i : Config::socketStep;
                //                log(this) << "Polling i: " << i << " nfds: " << nfds << " size: " << _pfds.size() << std::endl;
                int temp = rpoll(&pfds[i], nfds, 0);
                if (temp == -1) {
                    for (unsigned int j = 0; j < nfds; j++) {
                        closeSocket(_pfds[i + j].fd);
                        _pfds[i + j].fd = -1;
                        _pfds[i + j].events = 0;
                        _pfds[i + j].revents = 0;
                    }
                }
                else
                    _inMsgs += temp;
            }
        }

        if (_inMsgs)
            log(this) << _addr << " Polled " << _inMsgs << " messages!" << std::endl;

        while (_inMsgs > 0) { //Lets find what sockets have messages and set _tlSocket
            int index = _nextSocket++ % _pfds.size();
            if (_pfds[index].revents) {
                if (_pfds[index].revents == POLLIN) {
                    _tlSocket = _pfds[index].fd; //Set the thread local _tlSocket for the recv call
                    _nextSocket = index + 1;
                    _pfds[index].revents = 0;
                    log(this) << _addr << " Reading socket " << _tlSocket << std::endl;
                    break;
                }
                else if (_pfds[index].revents) { //This means we need to close socket

                    std::stringstream ss;
                    ss << _addr << " Warning: Unexpected revent: " << _pfds[index].revents << " " << index << " " << _pfds[index].fd << std::endl;
                    if (_pfds[index].revents & POLLPRI) {
                        ss << "POLLPRI " << std::endl;
                    }
                    if (_pfds[index].revents & POLLRDHUP) {
                        ss << "POLLRDHUP " << std::endl;
                    }
                    if (_pfds[index].revents & POLLERR) {
                        ss << "POLLERR " << std::endl;
                    }
                    if (_pfds[index].revents & POLLHUP) {
                        ss << "POLLHUP " << std::endl;
                    }
                    if (_pfds[index].revents & POLLNVAL) {
                        ss << "POLLNVAL " << std::endl;
                    }

                    ss << std::endl;
                    log(this) << ss.str();

                    //If server socket fails just close socket and let client reconnect
                    closeSocket(_pfds[index].fd);

                    _pfds[index].fd = -1;
                    _pfds[index].events = 0;
                    _pfds[index].revents = 0;

                    _inMsgs--; //poll should return a value for each pollfd if revents is set
                }
            }
        }

        return _inMsgs--;
    }
    return -1;
}

bool Connection::open() {
    return (_numSockets.load() > 0);
}

std::string Connection::addr() {
    return _addr;
}

unsigned int Connection::port() {
    return _port;
}

std::string Connection::addrport() {
    return _addrport;
}

unsigned int Connection::numSockets() {
    return _numSockets.load();
}

bool Connection::isServer() {
    return (_isServer == true);
}

bool Connection::isClient() {
    return (_isServer == false);
}

int Connection::localSocket() {
    return _tlSocket;
}

Connection *Connection::addNewClientConnection(std::string hostAddr, int port, unsigned int numConnections) {
    return Trackable<std::string, Connection *>::AddTrackable(hostAddr + std::to_string(port), [=] {
        Connection *ret = new Connection(hostAddr, port);
        if (ret->initiate(numConnections))
            return ret;
        delete ret;
        return (Connection *)NULL;
    });
}

Connection *Connection::addNewHostConnection(std::string clientAddr, int port, int socket, bool &created) {
    return Trackable<std::string, Connection *>::AddTrackable(
        clientAddr + std::to_string(port),
        [=] { return new Connection(socket, clientAddr, port); },
        [=](Connection *connection) {
            connection->lock();
            connection->addSocket(socket);
            connection->unlock();
        },
        created);
}

bool Connection::removeConnection(std::string addr, unsigned int dec) {
    return Trackable<std::string, Connection *>::RemoveTrackable(addr, dec);
}

bool Connection::removeConnection(Connection *connection, unsigned int dec) {
    return removeConnection(connection->addr() + std::to_string(connection->port()));
}

void Connection::closeAllConnections() {
    Trackable<std::string, Connection *>::RemoveAllTrackable();
}
