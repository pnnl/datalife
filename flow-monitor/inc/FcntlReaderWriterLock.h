#ifndef FcntlREADERWRITERLOCK_H
#define FcntlREADERWRITERLOCK_H
#include "ReaderWriterLock.h"
#include "UnixIO.h"
#include "Timer.h"
#include "Request.h"
#include <atomic>
#include <limits.h>
#include <thread>
#include <vector>

class FcntlReaderWriterLock {
  public:
    void readerLock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    void readerUnlock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);

    void writerLock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    int writerLock2(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    int writerLock3(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt, std::string blkPath);
    int lockAvail(int fd, uint64_t blk);
    int lockAvail2(int fd, uint64_t blk, std::string blkPath);

    void writerUnlock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    void writerUnlock2(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    void writerUnlock3(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt, std::string blkPath);

    FcntlReaderWriterLock();
    ~FcntlReaderWriterLock();

  private:
    ReaderWriterLock _lock;
    ReaderWriterLock _fdMutex;
};

class OldLock {
  public:
    void readerLock(uint64_t entry,bool log=true);
    void readerUnlock(uint64_t entry,bool log=true);

    void writerLock(uint64_t entry,bool log=true);
    void writerUnlock(uint64_t entry,bool log=true);
    int lockAvail(uint64_t entry,bool log=true);

    OldLock(uint32_t entrySize, uint32_t numEntries, std::string lockPath, std::string id);
    ~OldLock();

  private:
    ReaderWriterLock *_shmLock;
    ReaderWriterLock *_fdMutex;
    uint32_t _entrySize;
    uint32_t _numEntries;
    std::atomic<uint16_t> *_readers;
    std::atomic<uint16_t> *_writers;
    std::atomic<uint8_t> *_readerMutex;
    std::string _lockPath;
    std::string _id;
    int _fd;
};

class FcntlBoundedReaderWriterLock {
  public:
    void readerLock(uint64_t entry,Request* req,bool log=true);
    void readerUnlock(uint64_t entry,Request* req,bool log=true);

    void writerLock(uint64_t entry,Request* req, bool log=true);
    void writerUnlock(uint64_t entry,Request* req, bool log=true);
    int lockAvail(uint64_t entry,Request* req, bool log=true);

    FcntlBoundedReaderWriterLock(uint32_t entrySize, uint32_t numEntries, std::string lockPath, std::string id);
    ~FcntlBoundedReaderWriterLock();

  private:
    int lockOp(uint64_t entry, int lock_type, int op_type);
    ReaderWriterLock *_shmLock;
    ReaderWriterLock *_fdMutex;
    uint32_t _entrySize;
    uint32_t _numEntries;
    std::atomic<uint16_t> *_readers;
    std::atomic<uint16_t> *_writers;
    std::atomic<uint8_t> *_readerMutex;
    std::string _lockPath;
    std::string _id;
    int _fd;
};

#endif /* FcntlREADERWRITERLOCK_H */
