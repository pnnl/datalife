#ifndef MONITORFILEDESCRIPTOR_H
#define MONITORFILEDESCRIPTOR_H

#include "MonitorFile.h"
#include "Trackable.h"
//#include "OutputFile.h"

// The monitored descriptors. Each is a user of its file from the moment it is
// recorded until closeMonitorFile().
class MonitorFileDescriptor : public Trackable<int, MonitorFileDescriptor *> {
  public:
    // The file of a monitored descriptor and the index of the descriptor's
    // position in it, for a call to use. The caller becomes a user of the file,
    // so the file stays though another thread closes the descriptor meanwhile,
    // and ends that with MonitorFile::removeMonitorFile().
    static bool useMonitorFileDescriptor(int fd, MonitorFile *&file, unsigned int &index);
    // fd is from now on a descriptor of file, which has a user for it, or of no
    // monitored file if file is NULL. Returns the file fd was a descriptor of
    // until now, if it was monitored, for closeMonitorFile().
    static MonitorFile *replaceMonitorFileDescriptor(int fd, MonitorFile *file, unsigned int index);
    // A descriptor of file is closed: writes the file's traces
    // (TrackFile::close()) and ends the descriptor's use of the file. openAs is a
    // descriptor through which the file can still be looked at, or -1. Not to be
    // called with a stream locked: writing the traces opens files, which waits
    // for a thread that is in fflush(NULL), which waits for the stream.
    static void closeMonitorFile(MonitorFile *file, int openAs);
    // fd, new from the kernel, is a descriptor of file. A descriptor still
    // monitored under that number was closed in a way the library did not see
    // (close_range(), a system call made directly).
    static bool addMonitorFileDescriptor(int fd, MonitorFile *file, unsigned int index);
    // fd is closed, or about to be (it is openAs, then). Returns whether it was
    // monitored.
    static bool removeMonitorFileDescriptor(int fd, int openAs);
    // False if fd is not monitored, without a lock: most calls a program makes
    // are on descriptors that are not. True if it may be.
    static bool mayBeMonitored(int fd) {
        return fd >= 0 && (fd >= kExact || (_monitored[fd / 64].load(std::memory_order_relaxed) >> (fd % 64)) & 1);
    }
    MonitorFile* getFile() {return _file;}
    MonitorFileDescriptor(MonitorFile *file, unsigned int index);
    ~MonitorFileDescriptor();

  private:
    MonitorFile *_file;
    int _index;

    // A bit for each monitored descriptor below kExact, changed with the
    // registry locked.
    static const int kExact = 1024;
    static std::atomic<uint64_t> _monitored[kExact / 64];
    static void setMonitored(int fd, bool monitored);
};

#endif /* MONITORFILEDESCRIPTOR_H */