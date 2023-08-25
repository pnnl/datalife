#ifndef READERWRITERLOCK_H
#define READERWRITERLOCK_H

// #include "Request.h"
class Request;
#include <atomic>
#include <thread>

class ReaderWriterLock {
  public:
    unsigned int readerLock();
    unsigned int readerUnlock();

    void writerLock();
    void fairWriterLock();
    void writerUnlock();
    void fairWriterUnlock();

    bool tryWriterLock();
    bool cowardlyTryWriterLock();
    bool tryReaderLock();
    bool cowardlyUpdgradeWriterLock();

    void print();

    ReaderWriterLock();
    ~ReaderWriterLock();

  private:
    std::atomic_uint _readers;
    std::atomic_uint _writers;
    std::atomic<uint64_t> _cnt;
    std::atomic<uint64_t> _cur;
};

class MultiReaderWriterLock {
  public:
    void readerLock(uint64_t entry, Request* req = NULL);
    void readerUnlock(uint64_t entry, Request* req = NULL);

    void writerLock(uint64_t entry, Request* req = NULL);
    void fairWriterLock(uint64_t entry);
    void writerUnlock(uint64_t entry, Request* req = NULL);
    void fairWriterUnlock(uint64_t entry);
    int lockAvail(uint64_t entry, Request* req = NULL);

    MultiReaderWriterLock(uint32_t numEntries);
    MultiReaderWriterLock(uint32_t numEntries, uint8_t *dataAddr, bool init = false);
    ~MultiReaderWriterLock();
    static uint64_t getDataSize(uint64_t numEntries) {
        return (numEntries * sizeof(std::atomic<uint16_t>)) * 4;
    }

  private:
    uint32_t _numEntries;
    std::atomic<uint16_t> *_readers;
    std::atomic<uint16_t> *_writers;
    std::atomic<uint16_t> *_cnt;
    std::atomic<uint16_t> *_cur;
    uint8_t *_dataAddr;
};

#endif /* READERWRITERLOCK_H */
