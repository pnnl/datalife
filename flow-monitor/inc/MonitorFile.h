#ifndef MonitorFile_H_
#define MonitorFile_H_

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <list>
#include <math.h>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Connection.h"
#include "Loggable.h"
#include "Trackable.h"

// The positions of a file's descriptors, one per MonitorFile::newFilePosIndex().
// Another thread can open the file, adding one, while I/O through the others
// updates theirs, so they live in chunks that never move: chunk k holds 64 << k.
class FilePositions {
  public:
    FilePositions();
    ~FilePositions();
    FilePositions(const FilePositions &) = delete;
    FilePositions &operator=(const FilePositions &) = delete;

    std::atomic<uint64_t> &operator[](uint32_t index);
    // Adds a position, starting at 0, and returns its index. Calls must not overlap.
    uint32_t add();

  private:
    static uint32_t chunkOf(uint32_t index);
    static uint64_t chunkStart(uint32_t chunk);
    static uint64_t chunkSize(uint32_t chunk);

    std::atomic<std::atomic<uint64_t> *> _chunks[32];
    uint32_t _size;
};

class MonitorFile : public Loggable, public Trackable<std::string, MonitorFile *> {
  public:
    enum Type { //Input = 0,
                //Output = 1,
                //Local = 2,
		TrackLocal = 3
    };

    MonitorFile(MonitorFile::Type type, std::string name, std::string metaName, int fd);
    virtual ~MonitorFile();

    virtual void open() = 0;
    // In close, read, write and seek, fd is the caller's descriptor. A file can be
    // open through several at once, each with its own offset and flags, and the
    // first may already be closed and its number reused for another file.
    virtual void close(int fd) = 0;
    virtual uint64_t fileSize() = 0;
  //  virtual uint64_t numBlks() = 0;
  
    // virtual ssize_t read(void *buf, size_t count, uint32_t filePosIndex = 0) = 0;
    // virtual ssize_t write(const void *buf, size_t count, uint32_t filePosIndex = 0) = 0;
    virtual ssize_t read(int fd, void *buf, size_t count, uint32_t filePosIndex = 0, off_t offset = -1) = 0;
    virtual ssize_t write(int fd, const void *buf, size_t count, uint32_t filePosIndex, off_t offset) = 0;

    // A position for a descriptor of the file that was just opened: fd, which
    // also tells whether the descriptor appends.
    virtual uint32_t newFilePosIndex(int fd = -1);
    // A position for a copy of the descriptor whose position is at `index`
    // (dup() and the like). The two are one open file: they have one offset and
    // one set of flags, so filePos() and appends() of the copy are those of the
    // original, whichever of the two a call goes through. What a stream has
    // recorded is the copy's own: each descriptor can have a stream on it (a
    // log, and stdout or stderr put on it with dup2()), each with its buffer.
    uint32_t newFilePosIndexSharing(uint32_t index);
    // The descriptor whose position was at `index` is gone. The position of a
    // copy serves the next copy, so that a program that copies a descriptor
    // over and over (a shell around every redirection) does not add one each
    // time; another stays, as copies may share it.
    void releaseFilePosIndex(uint32_t index);
    virtual uint64_t filePos(uint32_t index);
    virtual void setFilePos(uint32_t index, uint64_t pos);
    // For a stream on this descriptor: -1 until a call that reads or writes is
    // seen on it, then how many of the bytes the program has taken from the
    // stream's buffer, or put in it, are recorded (see trackUnseenStreamIO() in
    // Lib.cpp).
    long streamRecorded(uint32_t index);
    void setStreamRecorded(uint32_t index, long bytes);
    // Whether what is written through the descriptor goes to the end of the
    // file, wherever its offset is: it was opened with O_APPEND, or given the
    // flag since.
    bool appends(uint32_t index);
    void setAppends(uint32_t index, bool appends);
    virtual off_t seek(int fd, off_t offset, int whence, uint32_t index = 0) = 0;

    static MonitorFile *addNewMonitorFile(MonitorFile::Type type, std::string fileName, std::string metaName, int fd, bool open = true);
    static bool removeMonitorFile(std::string fileName);
    // One user of the file fewer: a descriptor that is closed, or a call that is
    // done with the file. The last one deletes it.
    static bool removeMonitorFile(MonitorFile *file);
    static MonitorFile *lookUpMonitorFile(std::string fileName);
    // After a rename of `from` to `to`: a file open under `from` goes by `to` from
    // now on, unless one is open under `to` as well. Returns whether it does;
    // open says whether a file was open under `from`.
    static bool renameMonitorFile(const std::string &from, const std::string &to, bool &open);
    // Whether the base name of path matches one of DATALIFE_FILE_PATTERNS.
    static bool matchesPatterns(const std::string &path);

    MonitorFile::Type type();
    std::string name();
    std::string metaName();
    uint64_t blkSize();
    bool compress();
    bool prefetch();
    virtual bool active();
    bool eof(uint32_t index);
    // Whether the file was a regular file when it was first opened. I/O on other
    // kinds, such as FIFOs, can block until another thread acts.
    bool regularFile();
    // Whether close() reports this file: its name matches a pattern, or did
    // before the file was renamed.
    bool traced();
  protected:
    // Gives the file its new name, and what was recorded under the old one.
    // Called with the registry of files locked.
    virtual void renameTo(const std::string &name) = 0;

    // The names change when the file is renamed, with the registry of files
    // and trackFileStatLock() (TrackFile.h) both locked. Read them with either
    // locked, or through name() and metaName(), which lock the second.
    std::string findMetaParam(std::string param, std::string server, bool required);
    bool readMetaInfo();
    

    MonitorFile::Type _type;
    std::string _name;
    std::string _metaName;
    bool _traced; // matched a pattern under a name it no longer has

    // The offset of the descriptor at index: its own, or for a copy that of the
    // descriptor it was copied from.
    std::atomic<uint64_t> &offsetAt(uint32_t index) { return _filePos[(uint32_t)_shares[index].load()]; }

    std::mutex _fpMutex;
    FilePositions _filePos;        // read and written through offsetAt()
    FilePositions _shares;         // for each position, the index of the one that holds its offset and flags
    std::vector<uint32_t> _freeCopies; // positions of copies whose descriptors are gone, under _fpMutex
    FilePositions _streamRecorded; // for each position, streamRecorded() + 1
    FilePositions _appends;        // for each position, 1 if its descriptor appends
    std::vector<bool> _eof;

    //Properties from meta data file
    bool _compress;
    uint32_t _prefetch; //Adding the option to prefetch or not a particular file
    uint64_t _blkSize;

    uint64_t _initMetaTime;

    std::atomic_bool _active; //if the connections are up
    std::vector<Connection *> _connections;

  private:
    int _fd;
    bool _regularFile;
};

#endif /* MonitorFile_H_ */