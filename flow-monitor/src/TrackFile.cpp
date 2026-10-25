#include "TrackFile.h"

#include "UnixIO.h" // to invoke posix read
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "FileCacheRegister.h"
#include "Message.h"
#include "MonitorPathPolicy.h"
#include "Request.h"
#include "Timer.h"
#include "UnixIO.h"
#include "lz4.h"
#include "xxhash.h"

#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <signal.h>
#include <sstream>
#include <string.h>
#include <string>
#include <tuple>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <algorithm>
#include <cassert>
#include <functional>
#include <chrono>
#include <json.hpp> // for logging json file
#include <limits.h> // for HOST_NAME_MAX

using namespace std::chrono;
#ifdef LIBDEBUG
// Marked as the library's own I/O: callers may hold the trace-stat lock, and GCC
// can turn the fprintf into fwrite, whose wrapper takes a registry lock that the
// fork prepare handler in Lib.cpp takes before the trace-stat lock.
#define DPRINTF(...) do { ScopedMonitorInternalIO dprintf_io; fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define DPRINTF(...)
#endif

// BLK_SIZE is defined in json_version but not in main
// If undefined, json mode should still work correctly
#ifndef BLK_SIZE
#define BLK_SIZE 4096 // 4KB default for JSON mode
#endif

#define GATHERSTAT 1
#define USE_HASH 1
// #define ENABLE_TRACE 1
// #define WRITE_STAT_EACH 0

// Created once, thread-safely, and never freed; trackFileInit() creates it before
// other threads exist. A forked child reinitializes it in place.
SpinLock &trackFileStatLock() {
    static auto *lock = new SpinLock();
    return *lock;
}

static std::mutex &legacyStatLock();
static std::map<std::string, off_t> &legacyTableStart();

void trackFileInit() {
    trackFileStatLock();
    legacyStatLock();
    legacyTableStart();
}

// Empties a trace map in a forked child without freeing the parent's entries: freeing
// writes to every page they occupy, which the child would then have to copy, and the
// child may not use the heap at all (see monitorForkChild in Lib.cpp). A new empty
// map is constructed over the old one, so the entries are abandoned.
template <class Map> static void dropInheritedTraces(Map &traces) {
    new (&traces) Map();
}

void trackFileForkChildReset() {
    // May be held by a thread that does not exist in the child; reinitialize, don't acquire.
    new (&trackFileStatLock()) SpinLock();
    new (&legacyStatLock()) std::mutex();
    // The child's pid names new output files; the parent's table positions are abandoned.
    new (&legacyTableStart()) std::map<std::string, off_t>();

    // Start the child's block statistics and traces empty, so its trace files cover
    // only its own accesses, as its timer output does.
    dropInheritedTraces(track_file_blk_r_stat);
    dropInheritedTraces(track_file_blk_r_stat_size);
    dropInheritedTraces(track_file_blk_w_stat);
    dropInheritedTraces(track_file_blk_w_stat_size);
    dropInheritedTraces(trace_read_blk_seq);
    dropInheritedTraces(trace_write_blk_seq);
    dropInheritedTraces(trace_read_blk_order);
    dropInheritedTraces(trace_write_blk_order);
    // Files open across fork() restart their access pattern.
    for (auto it = Trackable<std::string, MonitorFile *>::begin();
         it != Trackable<std::string, MonitorFile *>::end(); ++it) {
        if (it->second->type() == MonitorFile::TrackLocal) {
            TrackFile *file = static_cast<TrackFile *>(it->second);
            file->first_access_block = -1;
            file->largest_access_block = -1;
            file->prev_end_offset = -1;
            file->has_been_random = false;
        }
    }
}

// Locked map lookup; the returned reference stays valid (unordered_map never moves elements).
static TraceData &traceEntry(std::unordered_map<std::string, TraceData> &traces, const std::string &key) {
    std::lock_guard<SpinLock> stat_guard(trackFileStatLock());
    return traces[key];
}

// Helper function to extract basename from a path
inline std::string get_basename_str(const std::string& path) {
    size_t pos = path.rfind('/');
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

TrackFile::TrackFile(std::string name, int fd, bool openFile) : 
  MonitorFile(MonitorFile::Type::TrackLocal, name, name, fd),
  _fileSize(0),
  _numBlks(0),
  _filename(name),
  total_time_spent_read(0),
  total_time_spent_write(0)

{ 
  // DPRINTF("In Trackfile constructor openfile bool: %d\n", openFile);
  // std::cout << "TrackFile.cpp: constructor openfile bool: " << openFile << std::endl;
  _blkSize = Config::blockSizeForStat;
  open();
  _active.store(true);
}

TrackFile::~TrackFile() {
    *this << "Destroying file " << _metaName << std::endl;
    // close();
}

void TrackFile::open() {
  DPRINTF("[MONITOR] TrackFile open: %s\n", _name.c_str()) ;
  // Inserts into the shared maps while other threads may be looking them up.
  std::lock_guard<SpinLock> stat_guard(trackFileStatLock());
  // #if 0
  // _closed = false;

#ifdef BLK_IDX
  // trace_read_blk_order.emplace(_name, std::vector<TraceData>());
  // trace_write_blk_order.emplace(_name, std::vector<TraceData>());
  trace_read_blk_order.emplace(_name, TraceData());
  trace_write_blk_order.emplace(_name, TraceData());
#else
  // Initialize trace maps for JSON output mode when BLK_IDX is not defined
  if (Config::enableJsonOutput) {
    trace_read_blk_order.emplace(_name, TraceData());
    trace_write_blk_order.emplace(_name, TraceData());
  }
#endif

#ifdef GATHERSTAT
  if (track_file_blk_r_stat.find(_name) == track_file_blk_r_stat.end()) {
    track_file_blk_r_stat.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }
  if (track_file_blk_r_stat_size.find(_name) == track_file_blk_r_stat_size.end()) {
    track_file_blk_r_stat_size.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }
  if (track_file_blk_w_stat.find(_name) == track_file_blk_w_stat.end()) {
    track_file_blk_w_stat.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }

  if (track_file_blk_w_stat_size.find(_name) == track_file_blk_w_stat_size.end()) {
    track_file_blk_w_stat_size.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }

  if (trace_read_blk_seq.find(_name) == trace_read_blk_seq.end()) {
    trace_read_blk_seq.insert(std::make_pair(_name, std::vector<int>()));
  }

  if (trace_write_blk_seq.find(_name) == trace_write_blk_seq.end()) {
    trace_write_blk_seq.insert(std::make_pair(_name, std::vector<int>()));
  }
#endif

  open_file_start_time = high_resolution_clock::now();

  // #endif
  DPRINTF("Returning from trackfile open\n");

}

// Bookkeeping for one access of `bytes` bytes at `pos`, after the real call: this
// file's access pattern (JSON mode) and block histogram. The access touches blocks
// pos / size through (pos + bytes - 1) / size: one that ends on a block boundary
// does not touch the next block.
void TrackFile::recordAccess(bool isWrite, off_t pos, ssize_t bytes) {
    if (bytes <= 0) {
        return;
    }
    const uint64_t first_byte = pos;
    const uint64_t last_byte = pos + bytes - 1;

    std::lock_guard<SpinLock> stat_guard(trackFileStatLock());

    // JSON mode: track block access patterns
    if (Config::enableJsonOutput) {
        const int start_block = first_byte / BLK_SIZE;
        const int end_block = last_byte / BLK_SIZE;
        auto &trace_vector = (isWrite ? trace_write_blk_order : trace_read_blk_order)[_name];
#ifdef BLK_IDX
        trace_vector.push_back(start_block);
        trace_vector.push_back(end_block);
#else
        if (first_access_block == -1) {
            first_access_block = start_block;
        }
        if (largest_access_block < end_block) {
            largest_access_block = end_block;
        }
        // [first block, last block, largest block, -1 while sequential, -2 once random]
        if (trace_vector.size() < 4) {
            trace_vector.resize(4, -2);
        }
        trace_vector[0] = first_access_block;
        trace_vector[1] = end_block;
        trace_vector[2] = largest_access_block;
        // Sequential: each access starts at or after the end of the previous one.
        if (prev_end_offset == -1) {
            trace_vector[3] = -1;
        } else if (has_been_random) {
            trace_vector[3] = -2;
        } else if (pos >= prev_end_offset) {
            trace_vector[3] = -1;
        } else {
            trace_vector[3] = -2;
            has_been_random = true;
        }
        if (!has_been_random) {
            prev_end_offset = pos + bytes;
        }
        DPRINTF("TrackFile::recordAccess() %s blocks [%d, %d], largest %d, status %d\n", isWrite ? "write" : "read",
                start_block, end_block, largest_access_block, trace_vector[3]);
#endif
    }

#ifdef GATHERSTAT
    const uint32_t start_block = first_byte / _blkSize;
    const uint32_t end_block = last_byte / _blkSize;
    auto &block_stat = (isWrite ? track_file_blk_w_stat : track_file_blk_r_stat)[_name];
    auto &block_stat_size = (isWrite ? track_file_blk_w_stat_size : track_file_blk_r_stat_size)[_name];
    auto &block_seq = (isWrite ? trace_write_blk_seq : trace_read_blk_seq)[_name];
    for (uint32_t block = start_block; block <= end_block; ++block) {
#ifdef USE_HASH
        auto sample = hashed(block) % Config::hashtableSizeForStat;
        if (sample >= Config::hashtableSizeForStat / 2) continue;
#endif
        if (block_stat.find(block) == block_stat.end()) {
            block_stat[block] = 1;
            // The bytes of this access that fall in this block.
            const int64_t block_start = (int64_t)block * _blkSize;
            block_stat_size[block] = std::min<int64_t>(pos + bytes, block_start + _blkSize) - std::max<int64_t>(pos, block_start);
        } else {
            block_stat[block]++;
        }
        block_seq.push_back(block);
    }
#endif
}

ssize_t TrackFile::read(int fd, void *buf, size_t count, uint32_t index, off_t offset) {
    DPRINTF("In trackfile read count %u\n", count);
    // std::cerr << "DEBUG: TrackFile::read() called for " << _name << " count=" << count << std::endl;

    auto start_time = high_resolution_clock::now();
    unixread_t unixRead = (unixread_t)dlsym(RTLD_NEXT, offset == -1 ? "read" : "pread");

    // Handle offset for `pread`
    ssize_t bytes_read = (offset == -1)
        ? (*unixRead)(fd, buf, count)
        : ((unixpread_t)unixRead)(fd, buf, count, offset);

    total_time_spent_read += duration_cast<seconds>(high_resolution_clock::now() - start_time);

    DPRINTF("bytes_read: %ld fd: %d _name: %s\n", bytes_read, fd, _name.c_str());

    // read() reads at this descriptor's position and advances it; pread() reads at offset.
    off_t pos = (offset == -1) ? (off_t)_filePos[index].load() : offset;
    recordAccess(false, pos, bytes_read);
    if (bytes_read > 0 && offset == -1) {
        _filePos[index] += bytes_read;
    }

#ifdef WRITE_STAT_EACH
    close(fd);
#endif

    return bytes_read;
}

ssize_t TrackFile::write(int fd, const void *buf, size_t count, uint32_t index, off_t offset) {
    DPRINTF("In TrackFile::write with count: %u\n", count);

    auto start_time = high_resolution_clock::now();

    // Use the appropriate system call based on the offset
    ssize_t bytes_written;
    if (offset >= 0) {
        // Use pwrite for positional writes
        unixpwrite_t unixPwrite = (unixpwrite_t)dlsym(RTLD_NEXT, "pwrite");
        bytes_written = (*unixPwrite)(fd, buf, count, offset);
        DPRINTF("Performing pwrite: offset=%ld\n", offset);
    } else {
        // Use write for sequential writes
        unixwrite_t unixWrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");
        bytes_written = (*unixWrite)(fd, buf, count);
    }

    if (bytes_written < 0) {
        perror("Write failed");
        return bytes_written;
    }

    auto end_time = high_resolution_clock::now();
    total_time_spent_write += duration_cast<seconds>(end_time - start_time);

    DPRINTF("Bytes written: %ld, FD: %d, File: %s\n", bytes_written, fd, _name.c_str());

    // write() writes at this descriptor's position and advances it; pwrite() writes at offset.
    off_t pos = (offset >= 0) ? offset : (off_t)_filePos[index].load();
    recordAccess(true, pos, bytes_written);
    if (bytes_written > 0 && offset < 0) {
        _filePos[index] += bytes_written;
    }

    return bytes_written;
}

/*POSIX ends */

int TrackFile::vfprintf(unsigned int pos, int count) {
  DPRINTF("In trackfile vfprintf\n");
  if (count > 0) {
    recordAccess(true, (off_t)_filePos[pos].load(), count);
    _filePos[pos] += count;
  }
  return count;
}

uint64_t TrackFile::fileSize() {
        return _fileSize;
}

off_t TrackFile::seek(int fd, off_t offset, int whence, uint32_t index) {
//   struct stat sb;
//   fstat(_fd_orig, &sb);
//   auto _fileSize = sb.st_size;

//   switch (whence) {
//   case SEEK_SET:
//     _filePos[index] = offset;
//     break;
//   case SEEK_CUR:
//     _filePos[index] += offset;
//     if (_filePos[index] > _fileSize) {
//       _filePos[index] = _fileSize;
//     }
//     break;
//   case SEEK_END:
//     _filePos[index] = _fileSize + offset;
//     break;
//   }
//   // _eof[index] = false;


  DPRINTF("Calling Seek in Trackfile\n");
  unixlseek_t unixLseek = (unixlseek_t)dlsym(RTLD_NEXT, "lseek");

//   auto offset_loc = (*unixLseek)(_fd_orig, offset, whence);
//   return  offset_loc;

  auto actual_pos = (*unixLseek)(fd, offset, whence);

  if (actual_pos != (off_t)-1) {
    _filePos[index] = actual_pos;
  }

  return actual_pos;
}

// Helper function for JSON trace output
void write_trace_data(const std::string& filename, const std::string &data_name, TraceData& blk_trace_info, const std::string &pid, const std::string &_name, bool is_read,
                      int64_t file_size_bytes, int64_t file_alloc_bytes, const char *file_size_source) {
  if (isMonitorInternalIO()) {
      return;
  }
  ScopedMonitorInternalIO internal_io;

  // Ensure dataLifeOutputPath is not empty
  if (Config::dataLifeOutputPath.empty()) {
      std::cerr << "Error: DATALIFE_OUTPUT_PATH is not set!" << std::endl;
      return;
  }

  // Manually extract the filename from the full path using string splitting
  std::string actualFilename = filename;
  size_t lastSlash = filename.find_last_of("/\\"); // Works for both Linux (/) and Windows (\)
  if (lastSlash != std::string::npos) {
      actualFilename = filename.substr(lastSlash + 1); // Get only the filename
  }

  std::string actualDataName = data_name;
  lastSlash = data_name.find_last_of("/\\"); // Works for both Linux (/) and Windows (\)
  if (lastSlash != std::string::npos) {
      actualDataName = data_name.substr(lastSlash + 1); // Get only the filename
  }

  // Construct new full path in the DATALIFE_OUTPUT_PATH directory
  std::string fullPath = Config::dataLifeOutputPath + "/" + actualFilename;

  DPRINTF("write_trace_data(): writing to %s", fullPath.c_str());

  nlohmann::json jsonOutput;
  { // read and clear shared trace state under the lock; write the file without it
  std::lock_guard<SpinLock> stat_guard(trackFileStatLock());

  if (blk_trace_info.empty()) {
      DPRINTF("write_trace_data(): blk_trace_info is empty");
      return;  // Do nothing if blk_trace_info is empty
  }

  // Create JSON object
  jsonOutput["file_name"] = actualDataName;
  jsonOutput["task_name"] = getenv("DATALIFE_TASK_NAME") ? std::string(getenv("DATALIFE_TASK_NAME")) : "task_name";
  jsonOutput["pid"] = pid;

  /// Calculate statistical values
  uint64_t accumu_access_frequency = 0;
  uint64_t accumu_data_volume = 0;  /// data_volume = access_frequency * access_size
  if (is_read) {
    for (auto& blk_info  : track_file_blk_r_stat[_name]) {
      accumu_access_frequency += blk_info.second;
      accumu_data_volume += blk_info.second * track_file_blk_r_stat_size[_name][blk_info.first];
    }
  } else { /// is write
    for (auto& blk_info : track_file_blk_w_stat[_name]) {
        accumu_access_frequency += blk_info.second;
        accumu_data_volume += blk_info.second * track_file_blk_w_stat_size[_name][blk_info.first];
    }
  }
  jsonOutput["access_frequency"] = accumu_access_frequency;
  jsonOutput["data_volume"] = accumu_data_volume;
  // Authoritative file size from fstat() at close; separate from data_volume and
  // io_blk_range, which describe this process's I/O activity, not the file.
  if (file_size_bytes >= 0) {
    jsonOutput["file_size_bytes"] = file_size_bytes;
    jsonOutput["file_alloc_bytes"] = file_alloc_bytes;
  } else {
    jsonOutput["file_size_bytes"] = nullptr;
    jsonOutput["file_alloc_bytes"] = nullptr;
  }
  jsonOutput["file_size_source"] = file_size_source;

  jsonOutput["io_blk_range"] = blk_trace_info;

  // Clear the vector after creating JSON object
  blk_trace_info.clear();
  } // end stat_guard scope

  // Write the JSON object to the new file location
  std::ofstream file(fullPath, std::ios::out | std::ios::trunc); // Overwrite file
  if (!file) {
      std::cerr << "Error: Could not create file at " << fullPath << std::endl;
      return;
  }

  file << jsonOutput.dump(4); // Pretty print with an indent of 4 spaces
  DPRINTF("write_trace_data(): blk_trace_info written to file %s", fullPath.c_str());
  file.close();
}

// Legacy histogram files (<file>_<pid>_r_stat, _w_stat) hold one table per
// process image: this process's totals so far, rewritten at every close. The
// pid in the name survives exec(), so a later image's table follows the earlier
// one's. Held by close() from its snapshot of the totals through writing them.
static std::mutex &legacyStatLock() {
    static auto *lock = new std::mutex();
    return *lock;
}

// Where this image's table starts in each histogram file.
static std::map<std::string, off_t> &legacyTableStart() {
    static auto *starts = new std::map<std::string, off_t>();
    return *starts;
}

static bool writeFully(int fd, const std::string &text, off_t offset) {
    size_t done = 0;
    while (done < text.size()) {
        ssize_t n = ::pwrite(fd, text.data() + done, text.size() - done, offset + done);
        if (n <= 0) {
            return false;
        }
        done += n;
    }
    return true;
}

static void writeLegacyHistogram(const std::string &path, const std::string &name,
                                 const std::map<int, int64_t> &stat, std::map<int, int64_t> &sizes) {
    std::ostringstream rows;
    for (auto const &blk_info : stat) {
        rows << blk_info.first << " " << blk_info.second << " " << sizes[blk_info.first] << '\n';
    }

    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        DPRINTF("File for stat collection not created!");
        return;
    }
    auto &starts = legacyTableStart();
    auto it = starts.find(path);
    off_t start;
    if (it != starts.end()) {
        start = it->second;
    } else {
        start = ::lseek(fd, 0, SEEK_END);
        if (start == 0) { // a new file: header first
            const std::string header = name + " Block no. Frequency Access size in byte\n";
            if (writeFully(fd, header, 0)) {
                start = header.size();
            }
        }
        starts[path] = start;
    }
    const std::string text = rows.str();
    if (!writeFully(fd, text, start) || ::ftruncate(fd, start + text.size()) != 0) {
        DPRINTF("Stat table for %s not written!", path.c_str());
    }
    ::close(fd);
}

// Legacy sequence files (<file>_<pid>_r_trace_stat, _w_trace_stat) list every
// access in order; each close appends the accesses since the previous one.
static void appendLegacySequence(const std::string &path, const std::vector<int> &blocks) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        DPRINTF("File for trace stat collection not created!");
        return;
    }
    std::ostringstream text;
    for (int block : blocks) {
        text << block << '\n';
    }
    const std::string s = text.str();
    size_t done = 0;
    while (done < s.size()) {
        ssize_t n = ::write(fd, s.data() + done, s.size() - done);
        if (n <= 0) {
            break;
        }
        done += n;
    }
    ::close(fd);
}

void TrackFile::close(int fd) {
    if (isMonitorInternalIO()) {
        return;
    }
    ScopedMonitorInternalIO internal_io;

    DPRINTF("Calling TrackFile close: %s\n", _name.c_str());

    auto pid = std::to_string(getpid());
    close_file_end_time = high_resolution_clock::now();

    // Capture the file's size while the descriptor is still open: monitorClose() /
    // monitorFclose() call this before the real close(). st_size is the logical
    // size, st_blocks*512 the allocated bytes (differs for sparse files).
    int64_t file_size_bytes = -1;
    int64_t file_alloc_bytes = -1;
    const char *file_size_source = "unavailable";
    struct stat st;
    if (fd >= 0 && ::fstat(fd, &st) == 0) {
        file_size_bytes = (int64_t)st.st_size;
        file_alloc_bytes = (int64_t)st.st_blocks * 512;
        file_size_source = "posix_fstat_at_close";
    }
    Timer::recordFileSize(_name, file_size_bytes, file_alloc_bytes, file_size_source);

    // JSON mode: write JSON trace files
    if (Config::enableJsonOutput) {

        // std::cerr << "DEBUG close(): _name=" << _name << " _filename=" << _filename << std::endl;
        // std::cerr << "DEBUG: trace_read_blk_order[_name].size()=" << trace_read_blk_order[_name].size() << std::endl;
        // std::cerr << "DEBUG: trace_read_blk_order[_filename].size()=" << trace_read_blk_order[_filename].size() << std::endl;

        char hostname[256]; // Buffer to store the host name
        std::string host_name = (gethostname(hostname, sizeof(hostname)) == 0) ? hostname : "unknown_host";

        DPRINTF("Writing r blk access order stat with prefix %s\n", _filename.c_str());
        std::string file_name_trace_r = _filename + "." + pid + "-" + host_name + ".r_blk_trace.json";

        auto& blk_trace_info_r = traceEntry(trace_read_blk_order, _filename);
        auto future_r = std::async(std::launch::async,
                                   write_trace_data,
                                   file_name_trace_r,
                                   _filename,
                                   std::ref(blk_trace_info_r),
                                   pid,
                                   _name,
                                   /*is_read=*/true,
                                   file_size_bytes, file_alloc_bytes, file_size_source);

        DPRINTF("Writing w blk access order stat with prefix %s\n", _filename.c_str());
        std::string file_name_trace_w = _filename + "." + pid + "-" + host_name + ".w_blk_trace.json";

        auto& blk_trace_info_w = traceEntry(trace_write_blk_order, _filename);
        auto future_w = std::async(std::launch::async,
                                   write_trace_data,
                                   file_name_trace_w,
                                   _filename,
                                   std::ref(blk_trace_info_w),
                                   pid,
                                   _name,
                                   /*is_read=*/false,
                                   file_size_bytes, file_alloc_bytes, file_size_source);
                                   
        // Wait for both async tasks to complete
        future_r.get();
        future_w.get();
    }

#ifdef GATHERSTAT
    // Original stat output mode (non-JSON)
    if (!Config::enableJsonOutput) {
    // Snapshot the histograms and take this close's part of the access sequences
    // under the stat lock; write with it released: every monitored read and write
    // waits for the stat lock, so it is never held across I/O. The legacy lock is
    // held from snapshot to write, so the tables are written in snapshot order.
    std::lock_guard<std::mutex> legacy_guard(legacyStatLock());
    std::map<int, int64_t> r_stat, r_stat_size, w_stat, w_stat_size;
    std::vector<int> blk_trace_info_r, blk_trace_info_w;
    {
        std::lock_guard<SpinLock> stat_guard(trackFileStatLock());
        for (auto const& blk_info : track_file_blk_r_stat[_name])
            r_stat[blk_info.first] = blk_info.second.load();
        for (auto const& blk_info : track_file_blk_r_stat_size[_name])
            r_stat_size[blk_info.first] = blk_info.second.load();
        for (auto const& blk_info : track_file_blk_w_stat[_name])
            w_stat[blk_info.first] = blk_info.second.load();
        for (auto const& blk_info : track_file_blk_w_stat_size[_name])
            w_stat_size[blk_info.first] = blk_info.second.load();
        // The accesses since the previous close: written out below and dropped here.
        blk_trace_info_r.swap(trace_read_blk_seq[_name]);
        blk_trace_info_w.swap(trace_write_blk_seq[_name]);
    }
    const std::string prefix = Config::dataLifeOutputPath + "/" + get_basename_str(_filename) + "_" + pid;
    DPRINTF("Writing r/w blk access stats with prefix %s\n", prefix.c_str());
    writeLegacyHistogram(prefix + "_r_stat", _filename, r_stat, r_stat_size);
    writeLegacyHistogram(prefix + "_w_stat", _filename, w_stat, w_stat_size);
    appendLegacySequence(prefix + "_r_trace_stat", blk_trace_info_r);
    appendLegacySequence(prefix + "_w_trace_stat", blk_trace_info_w);
    } // end if (!Config::enableJsonOutput)
#endif
}


// Tracking-only method for FILE* operations (no actual I/O to avoid buffering corruption):
// count bytes were just read, leaving the stream at filePos.
void TrackFile::trackRead(size_t count, uint32_t index, off_t filePos) {
    if (count == 0) return;

    // Update file position
    _filePos[index] = filePos;
    recordAccess(false, filePos - (off_t)count, (ssize_t)count);
}

// Tracking-only method for FILE* write operations
void TrackFile::trackWrite(size_t count, uint32_t index, off_t filePos) {
    if (count == 0) return;

    // Update file position
    _filePos[index] = filePos;
    recordAccess(true, filePos - (off_t)count, (ssize_t)count);
}
