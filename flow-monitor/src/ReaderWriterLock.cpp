#include "ReaderWriterLock.h"
#include "AtomicHelper.h"
#include "Request.h"
#include "Loggable.h"
#include <cstring>
#include <iostream>
#include <sys/types.h>
       #include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)

ReaderWriterLock::ReaderWriterLock() : _readers(0),
                                       _writers(0),
                                       _cnt(0),
                                       _cur(0) {
}

ReaderWriterLock::~ReaderWriterLock() {
    // std::cout << _readers << " " << _writers << " " << std::endl;
    while (_readers) {
        std::this_thread::yield();
    }
}

unsigned int ReaderWriterLock::readerLock() {
    unsigned int ret;
    while (1) {
        while (_writers.load()) {
            std::this_thread::yield();
        }
        ret = _readers.fetch_add(1);
        if (!_writers.load()) {
            break;
        }
        _readers.fetch_sub(1);
    }
    return ret;
}

unsigned int ReaderWriterLock::readerUnlock() {
    return _readers.fetch_sub(1);
}

void ReaderWriterLock::writerLock() {
    unsigned int check = 1;
    while (_writers.exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers.load()) {
        std::this_thread::yield();
    }
}

void ReaderWriterLock::fairWriterLock() {
    unsigned int check = 1;
    auto my_cnt = _cnt.fetch_add(1);
    while (my_cnt == _cur.load() || _writers.exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers.load()) {
        std::this_thread::yield();
    }
}

void ReaderWriterLock::writerUnlock() {
    _writers.store(0);
}

void ReaderWriterLock::fairWriterUnlock() {
    _writers.store(0);
    _cur.fetch_add(1);
}

bool ReaderWriterLock::tryWriterLock() {
    unsigned int check = 1;
    if (_writers.exchange(check) == 0) {
        while (_readers.load()) {
            std::this_thread::yield();
        }
        return true;
    }
    return false;
}

bool ReaderWriterLock::cowardlyTryWriterLock() {
    unsigned int check = 1;
    if (_writers.exchange(check) == 0) {
        if (_readers.load()) {
            _writers.store(0);
            return false;
        }
        return true;
    }
    return false;
}

//JS: Never use this to block.  Will lead to deadlock!
bool ReaderWriterLock::cowardlyUpdgradeWriterLock() {
    unsigned int one = 1;
    unsigned int zero = 0;
    if (_writers.exchange(one) == 0) {
        if (_readers.compare_exchange_weak(one, zero))
            return true;
        _writers.store(0);
    }
    return false;
}

bool ReaderWriterLock::tryReaderLock() {
    if (!_writers.load()) {
        _readers.fetch_add(1);
        if (!_writers.load())
            return true;
        _readers.fetch_sub(1);
        return false;
    }
    return false;
}

void ReaderWriterLock::print() {
    std::cout << "Readers: " << _readers.load() << " Writers: " << _writers.load() << std::endl; 
}

MultiReaderWriterLock::MultiReaderWriterLock(uint32_t numEntries) : _numEntries(numEntries), _dataAddr(NULL) {

    _readers = new std::atomic<uint16_t>[_numEntries];
    // memset(_readers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_readers,_numEntries,(uint16_t)0);
    _writers = new std::atomic<uint16_t>[_numEntries];
    // memset(_writers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_writers,_numEntries,(uint16_t)0);

    _cur = new std::atomic<uint16_t>[_numEntries];
    // memset(_cur, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_cur,_numEntries,(uint16_t)0);
    _cnt = new std::atomic<uint16_t>[_numEntries];
    // memset(_cnt, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_cnt,_numEntries,(uint16_t)0);
}

MultiReaderWriterLock::MultiReaderWriterLock(uint32_t numEntries, uint8_t *dataAddr, bool init) : _numEntries(numEntries), _dataAddr(dataAddr) {
    _readers = (std::atomic<uint16_t> *)_dataAddr;
    _writers = (std::atomic<uint16_t> *)_readers + numEntries;
    _cur = (std::atomic<uint16_t> *)_writers + numEntries;
    _cnt = (std::atomic<uint16_t> *)_cur + numEntries;
    if (init) {
        // memset(_readers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        // memset(_writers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        // memset(_cur, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        // memset(_cnt, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        init_atomic_array(_readers,_numEntries,(uint16_t)0);
        init_atomic_array(_writers,_numEntries,(uint16_t)0);
        init_atomic_array(_cur,_numEntries,(uint16_t)0);
        init_atomic_array(_cnt,_numEntries,(uint16_t)0);
    }
}

MultiReaderWriterLock::~MultiReaderWriterLock() {
    if (_dataAddr == NULL) {
        delete[] _readers;
        delete[] _writers;
        delete[] _cur;
        delete[] _cnt;
    }
}

void MultiReaderWriterLock::readerLock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"rlocking: "<<entry<<std::endl;}
    while (1) {
        while (_writers[entry].load()) {
            std::this_thread::yield();
        }
        _readers[entry].fetch_add(1);
        if (!_writers[entry].load()) {
            break;
        }
        _readers[entry].fetch_sub(1);
    }
    if(req) { req->trace("MultiReaderWriterLock")<<"rlocked: "<<entry<<std::endl;}
}

void MultiReaderWriterLock::readerUnlock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"runlocking: "<<entry<<std::endl; }
    _readers[entry].fetch_sub(1);
    if(req) { req->trace("MultiReaderWriterLock")<<"runlocked: "<<entry<<std::endl;}
}

void MultiReaderWriterLock::writerLock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"wlocking: "<<entry <<" "<<::getpid()<<std::endl; }
    unsigned int check = 1;
    while (_writers[entry].exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers[entry].load()) {
        std::this_thread::yield();
    }
    if(req) { req->trace("MultiReaderWriterLock")<<"wlocked: "<<entry <<" "<<::getpid()<<std::endl; }
}

void MultiReaderWriterLock::fairWriterLock(uint64_t entry) {
    unsigned int check = 1;
    auto my_cnt = _cnt[entry].fetch_add(1);
    while (my_cnt == _cur[entry].load() && _writers[entry].exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers[entry].load()) {
        std::this_thread::yield();
    }
}

void MultiReaderWriterLock::writerUnlock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"wunlocking: "<<entry <<" "<<::getpid()<<std::endl; }
    _writers[entry].store(0);
    if(req) { req->trace("MultiReaderWriterLock")<<"wunlocked: "<<entry <<" "<<::getpid()<<std::endl; }

}

void MultiReaderWriterLock::fairWriterUnlock(uint64_t entry) {
    _writers[entry].store(0);
    _cur[entry].fetch_add(1);
}
