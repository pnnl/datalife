#ifndef TRACKFILE_H
#define TRACKFILE_H
#include "FileCacheRegister.h"
#include "MonitorFile.h"
#include "SpinLock.h"
#include <atomic>
#include <mutex>
#include <string>
#include <map>

extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_r_stat;
extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_r_stat_size;
extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat;
extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat_size;
//extern std::map<std::string, std::map<int, std::tuple<std::atomic<int64_t>, std::atomic<int64_t> > > > track_file_blk_w_stat;

// For tracing
extern std::vector<std::string> patterns;
extern std::map<std::string, std::vector<int> > trace_read_blk_seq;
extern std::map<std::string, std::vector<int> > trace_write_blk_seq;

// For JOSN tracing
using TraceData = std::vector<int>;
extern std::unordered_map<std::string, TraceData> trace_read_blk_order;
extern std::unordered_map<std::string, TraceData> trace_write_blk_order;

// Guards the maps above and the per-file trace state, which several threads update
// concurrently. Held for bookkeeping only, never across a syscall or output write.
SpinLock &trackFileStatLock();
// Creates the lock above and the legacy output state; called by the library
// constructor, before other threads exist.
void trackFileInit();
// pthread_atfork child handler: reinitializes the stat lock and drops the parent's block
// statistics and traces, without allocating.
void trackFileForkChildReset();

class TrackFile : public MonitorFile {
public:
  TrackFile(std::string name, int fd, bool openFile = true);
  ~TrackFile();

  void open();
  void close(int fd);
  uint64_t fileSize();

  
  ssize_t read(int fd, void *buf, size_t count, uint32_t index, off_t offset);
  ssize_t write(int fd, const void *buf, size_t count, uint32_t filePosIndex, off_t offset) override;
  off_t seek(int fd, off_t offset, int whence, uint32_t index = 0);
  int vfprintf(unsigned int pos, int count);

  // Tracking-only methods (no I/O) for FILE* operations to avoid buffering corruption
  void trackRead(size_t count, uint32_t index, off_t filePos);
  void trackWrite(size_t count, uint32_t index, off_t filePos);
  
private:
// bool trackRead(size_t count, uint32_t index, uint32_t startBlock, uint32_t endBlock);
//    uint64_t copyBlock(char *buf, char *blkBuf, uint32_t blk, uint32_t startBlock, uint32_t endBlock, uint32_t fpIndex, uint64_t count);

  // Records one access to the block histogram and the JSON access pattern.
  void recordAccess(bool isWrite, off_t pos, ssize_t bytes);

  std::mutex _openCloseLock;
  // std::atomic<uint64_t> 
  uint64_t _fileSize;
  uint32_t _numBlks;
  uint32_t _regFileIndex;
  std::string _filename;
  std::hash<int64_t> hashed;
  std::chrono::high_resolution_clock::time_point open_file_start_time;
  std::chrono::high_resolution_clock::time_point close_file_end_time;
  std::chrono::seconds total_time_spent_read;
  std::chrono::seconds total_time_spent_write;

  // For JSON tracing: this file's access pattern so far (see recordAccess())
  int first_access_block = -1;
  int largest_access_block = -1;
  off_t prev_end_offset = -1; // the byte after the previous access
  bool has_been_random = false;
  friend void trackFileForkChildReset(); // resets the four above in a forked child
};


#endif /* LOCALFILE_H */