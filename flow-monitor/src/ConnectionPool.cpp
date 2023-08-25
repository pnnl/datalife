#include "ConnectionPool.h"
#include "Connection.h"
#include "Message.h"
#include "Timer.h"
#include <cstdint>
#include <iomanip>
#include <math.h>
extern thread_local std::uint64_t _tlSocketTime;
extern thread_local std::uint64_t _tlSocketBytes;
std::atomic<uint64_t> ConnectionPool::_curCnt(0);
std::unordered_map<std::string, uint64_t> *ConnectionPool::useCnt;
std::unordered_map<std::string, uint64_t> *ConnectionPool::consecCnt;
std::unordered_map<std::string, std::pair<double, double>> *ConnectionPool::stats;

ConnectionPool::ConnectionPool(std::string name, bool compress, std::vector<Connection *> &connections) : _name(name),
                                                                                                          _fileSize(0),
                                                                                                          _compress(compress),
                                                                                                          _connections(connections.begin(), connections.end()) {
    _servers_requested = 0;
    _valid_server = false;
    _startTime = Timer::getCurrentTime();
}

ConnectionPool::~ConnectionPool() {
    // std::cout << "deleting connection pool" << std::endl;
    closeFileOnAllServers();
}
void ConnectionPool::pushConnection(Connection *connection, bool useTL) {
    //    *this << "Push Connection" << std::endl;
    std::uint64_t bytes = 100;
    std::uint64_t time = 1;
    _qMutex.lock();
    if (stats->count(connection->addrport()) == 0) {
        (*stats)[connection->addrport()] = std::make_pair(1.0, .00000001);
        _prevStart[connection] = Timer::getCurrentTime();
        _tmp[connection] = 1;
    }
    if (useTL) {
        bytes = _tlSocketBytes;
        time = _tlSocketTime;
        (*stats)[connection->addrport()].first += bytes;
        (*stats)[connection->addrport()].second += (double)time / 1000000000.0;
    }
    ConnectionCompare entry(bytes, time, connection);
    //std::cout<<"[MONITOR] " << connection->addrport() << " " << bytes << " " << (double)time / 1000000000.0 << std::endl;

    eta[connection] = std::make_pair(0, (bytes * 1000000000) / (double)time);

    _mq.push_back(entry);

    _qMutex.unlock();
}

Connection *ConnectionPool::popConnection() {
    //    *this << "Pop Connection" << std::endl;
    ConnectionCompare entry;
    if (!_mq.empty()) {
        _qMutex.lock();
        bool popped = !_mq.empty();
        if (popped) {
            double base = 1.00001;
            entry = _mq.front();
            auto bit = _mq.begin();
            for (auto it = _mq.begin(); it != _mq.end(); ++it) {
                ConnectionCompare temp = *it;
                if (temp.rate * pow(base, _curCnt - ConnectionPool::consecCnt->at(temp.connection->addrport())) > entry.rate * pow(base, _curCnt - ConnectionPool::consecCnt->at(entry.connection->addrport()))) {
                    entry = temp;
                    bit = it;
                }
            }
            _mq.erase(bit);
            ConnectionPool::consecCnt->at(entry.connection->addrport()) = ++_curCnt;
            ConnectionPool::useCnt->at(entry.connection->addrport())++;
        }

        _qMutex.unlock();
        if (popped)
            return entry.connection;
    }
    return NULL;
}

Connection *ConnectionPool::popConnection(uint64_t size) {
    //    *this << "Pop Connection" << std::endl;
    bool popped = false;
    ConnectionCompare entry;
    if (!_mq.empty()) {
        _qMutex.lock();
        Connection *bcon = NULL;
        double curTime = (double)Timer::getCurrentTime() - _startTime;
        double b_eta = 1000000000000000;
        double bestRate = 0.0001;
        std::vector<double> etas(eta.size());
        std::vector<Connection *> cons(eta.size());
        int i = 0;
        for (auto it = eta.begin(); it != eta.end(); ++it) {
            Connection *c = (*it).first;
            double avgRate = ((*stats)[c->addrport()].first / (*stats)[c->addrport()].second); // - (*it).second.second * (2 / (ConnectionPool::useCnt->at(c->addrport()) + 1)) + (*it).second.second;
            double t_eta = curTime + (double)size / avgRate;
            if ((*it).second.first > 0.0001) { // && (*it).second.first > curTime) {
                _tmp[c] *= 1.02;
                // t_eta = curTime + ((double)size / avgRate) * _tmp[connection];
                t_eta = (*it).second.first + ((double)size / avgRate) * _tmp[c];
                //std::cout<<"[MONITOR] " << (((double)size / avgRate)) << " " << (((double)size / avgRate) * 3) << std::endl;
            }
            else {
                _tmp[c] = 1;
                t_eta = curTime + ((double)size / avgRate) * _tmp[c];
            }

            etas[i] = t_eta;
            cons[i] = c;
            //std::cout<<"[MONITOR] " << c->addrport() << " " << curTime << " " << t_eta << " " << t_eta - curTime << " " << (*it).second.first << " " << (double)size / (*it).second.second << " " << (*it).second.second << " " << avgRate << std::endl;
            if (t_eta < b_eta) {
                bcon = c;
                b_eta = t_eta;
                bestRate = avgRate;
            }

            i++;
        }

        auto bit = _mq.begin();
        for (auto it = _mq.begin(); it != _mq.end(); ++it) {
            ConnectionCompare temp = *it;
            if (temp.connection == bcon) {
                entry = temp;
                bit = it;
                popped = true;
                break;
            }
        }
        if (popped) {
            _mq.erase(bit);
            ConnectionPool::consecCnt->at(entry.connection->addrport()) = ++_curCnt;
            ConnectionPool::useCnt->at(entry.connection->addrport())++;
            // std::cout<<"[MONITOR] " << (double)(Timer::getCurrentTime() - _startTime) << " " << (double)size / eta[entry.connection].second << " " << (double)(Timer::getCurrentTime() - _startTime) + (double)size / eta[entry.connection].second << std::endl;
            eta[entry.connection].first = (double)(Timer::getCurrentTime() - _startTime) + (double)size / bestRate;
            _prevStart[entry.connection] = Timer::getCurrentTime();
        }
        _qMutex.unlock();
        if (popped)
            return entry.connection;
    }
    return NULL;
}

int ConnectionPool::numConnections() {
    return _mq.size();
}

ConnectionPool *ConnectionPool::addNewConnectionPool(std::string filename, bool compress, std::vector<Connection *> &connections, bool &created) {
    return Trackable<std::string, ConnectionPool *>::AddTrackable(filename, [&] {
        ConnectionPool *ret = new ConnectionPool(filename, compress, connections);
        return ret;
    });
}
bool ConnectionPool::removeConnectionPool(std::string filename, unsigned int dec) {
    return Trackable<std::string, ConnectionPool *>::RemoveTrackable(filename, dec);
}

bool ConnectionPool::openFileOnServer(Connection *server) {
    server->lock();
    uint64_t fileSize = 0;
    // std::cout << std::this_thread::get_id() << " Opening file on server: " << server->addrport() << " name: " << _name << " blkSize: " << Config::networkBlockSize << " compress: " << _compress << std::endl;
    for (uint32_t i = 0; i < Config::socketRetry; i++) {
        if (sendOpenFileMsg(server, _name, Config::networkBlockSize, _compress, false)) {
            if (recFileSizeMsg(server, fileSize))
                break;
        }
    }
    server->unlock();
    // std::cout << std::this_thread::get_id() << " done Opening file on server: " << server->addrport() << " name: " << _name << " blkSize: " << Config::networkBlockSize << " compress: " << _compress << std::endl;

    if (fileSize == 0) {
        std::cerr << "ERROR: files size " << fileSize << " " << server->addrport() << " " << _name << std::endl;
        return false;
    }
    else {
        if (_fileSize == 0) {
            _fileSize = fileSize;
        }
        else if (_fileSize != fileSize) {
            std::cerr << "ERROR: file sizes do not match across servers " << fileSize << " " << _fileSize << std::endl;
            return false;
        }
    }
    return true;
}

void ConnectionPool::addOpenFileTask(Connection *server) {
    //if (_prefetchLock.tryReaderLock()) { //This makes sure the file isn't deleted
    // std::cout << "adding open file task " << server->addrport() << std::endl;
    std::thread t = std::thread([this, server] {
        std::cout << "adding open file task " << server->addrport() << std::endl;
        //if (_active) {
        if (openFileOnServer(server)) {
            for (uint32_t j = 0; j < server->numSockets(); j++) {
                pushConnection(server);
            }
            _valid_server = true;
        }
        //}
        _servers_requested--;
        //_prefetchLock.readerUnlock();
    });
    t.detach();
    //}
}

uint64_t ConnectionPool::openFileOnAllServers() {
    if (!_valid_server) {
        // std::cout << "valid server!!! " << _connections.size() << std::endl;
        for (uint32_t i = 0; i < _connections.size(); i++) {
            _servers_requested++;
            addOpenFileTask(_connections[i]);
        }

        while (_servers_requested > 0 && !_valid_server) {
            sched_yield();
        }
    }
    // std::cout << "Well: " << _fileSize << std::endl;
    return _fileSize;
}

bool ConnectionPool::closeFileOnAllServers() {
    bool ret = true;
    std::atomic<int> closeCnt;
    while (_servers_requested) {
        sched_yield();
    }
    closeCnt.store(_connections.size());
    for (uint32_t i = 0; i < _connections.size(); i++) {
        Connection *server = _connections[i];
        std::thread t = std::thread([this, server, &closeCnt] {
            server->lock();
            std::cout << "sending close file" << std::endl;
            sendCloseFileMsg(server, _name);
            recAckMsg(server, CLOSE_FILE_MSG);
            // std::cout << "closed" << std::endl;
            server->unlock();
            popConnection();
            closeCnt--;
        });
        t.detach();
    }
    while (closeCnt) {
        sched_yield();
    }
    // std::cout << "cc: " << closeCnt << " numConnections" << _connections.size() << std::endl;
    return ret;
}

void ConnectionPool::removeAllConnectionPools() {
    Trackable<std::string, ConnectionPool *>::RemoveAllTrackable();
}