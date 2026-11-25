#ifndef TIMER_H
#define TIMER_H

#include <atomic>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "ReaderWriterLock.h"
#include "SpinLock.h"

class Timer {
  public:
    enum MetricType {
        monitor = 0,
        local,
        system,
        lastMetric
    };

    enum Metric {
        in_open = 0,
        out_open,
        close,
        read,
        write,
        seek,
        stat,
        fsync,
        readv,
        writev,
        in_fopen,
        out_fopen,
        fclose,
        fread,
        fwrite,
        ftell,
        fseek,
        fgetc,
        fgets,
        fputc,
        fputs,
        feof,
        rewind,
        constructor,
        destructor,
        trace_output, // the library writing a file's traces: its own work, like the two above
        dummy, //use to match calls to start...
        last
    };

    Timer();
    ~Timer();

    void start();
    void end(MetricType type, Metric metric);
    void addAmt(MetricType type, Metric metric, uint64_t amt);

    // While one of these exists on a thread, the library is doing work of its
    // own there: writing a file's traces, which it does from inside the
    // program's close() of the file. That time is not the call's, and neither
    // the program's I/O nor its computing. It is taken out of the time of the
    // call being timed on the thread, if one is, out of the I/O busy time and
    // out of the compute time, and reported as `monitor trace_output`. Does
    // nothing before the library's timer exists or after it is gone.
    class OwnWork {
      public:
        OwnWork() { begin(); }
        ~OwnWork() { end(); }
        OwnWork(const OwnWork &) = delete;
        OwnWork &operator=(const OwnWork &) = delete;

      private:
        static void begin();
        static void end();
    };

    static uint64_t getCurrentTime();
    static char *printTime();
    static int64_t getTimestamp();
    // Wall clock (system_clock) in ns since the epoch: comparable across processes and nodes.
    static uint64_t epochNs();
    // Monotonic clock in ns: used for durations, immune to clock adjustments.
    static uint64_t steadyNs();

    // Authoritative size of a traced file, captured with fstat() while its descriptor
    // is still open (TrackFile::close() runs before the real close). Kept separate
    // from data_volume / block coverage, which measure I/O activity, not file size.
    struct FileSizeRecord {
        int64_t size_bytes;      // st_size, -1 if fstat failed
        int64_t alloc_bytes;     // st_blocks * 512, -1 if fstat failed
        std::string source;      // "posix_fstat_at_close" or "unavailable"
        uint64_t closed_epoch_ns;
        uint32_t closes;         // times this process closed the file; last close wins
    };
    static void recordFileSize(const std::string &path, int64_t size_bytes, int64_t alloc_bytes, const char *source);
    static std::map<std::string, FileSizeRecord> fileSizes();

    // A rename this process made of a file it monitored, or whose old or new name
    // matches a pattern. followed: what was recorded under `from`, its size
    // included, is now reported under `to` (see trackFileRenamed()). Otherwise the
    // record is all that ties the two names: the file was written by another
    // process, for example, and renamed by this one.
    struct RenameRecord {
        std::string from;
        std::string to;
        uint64_t epoch_ns;
        bool followed;
    };
    static void recordRename(const std::string &from, const std::string &to, bool followed);
    static std::vector<RenameRecord> renames();

    // pthread_atfork child handler: drop inherited counters/registry, restart the caliper.
    void resetAfterFork();

  private:
    void addThread(std::thread::id id);
    class ThreadMetric {
      public:
        ThreadMetric();
        ~ThreadMetric();
        std::atomic<uint64_t> *current;
        std::atomic<uint64_t> *depth;
        std::atomic<uint64_t> *time[Timer::MetricType::lastMetric][Timer::Metric::last];
        std::atomic<uint64_t> *cnt[Timer::MetricType::lastMetric][Timer::Metric::last];
        std::atomic<uint64_t> *amt[Timer::MetricType::lastMetric][Timer::Metric::last];
    };

    const double billion = 1000000000;
    std::unordered_map<std::thread::id, Timer::ThreadMetric*> *_thread_timers;
    // Process-wide totals, updated by every thread that does I/O.
    std::atomic<uint64_t> _time[Timer::MetricType::lastMetric][Timer::Metric::last];
    std::atomic<uint64_t> _cnt[Timer::MetricType::lastMetric][Timer::Metric::last];
    std::atomic<uint64_t> _amt[Timer::MetricType::lastMetric][Timer::Metric::last];
    ReaderWriterLock *_lock; // heap-allocated so resetAfterFork() can replace it

    int stdoutcp;
    std::string myprogname;
    // The timer's own copies of Config's: read when the library is unloaded,
    // which can be after Config's strings are destroyed.
    std::string _output_path;
    std::string _task_name;

    // Task caliper: library init to library teardown (restarted at fork in a child).
    uint64_t _task_start_epoch_ns;
    uint64_t _task_start_steady_ns;
    pid_t _task_start_pid;

    // I/O busy time: wall-clock time with >= 1 thread inside an intercepted call
    // (union of intervals, unlike _time which sums per call across threads).
    SpinLock *_io_busy_lock;
    uint32_t _io_inflight;
    uint64_t _io_busy_start_ns;
    uint64_t _io_busy_ns;
    void ioBusyBegin();
    void ioBusyEnd(Metric metric);
    // Own busy time: wall-clock time with >= 1 thread in an OwnWork and none
    // inside an intercepted call, so that it and the I/O busy time never
    // overlap. Guarded by _io_busy_lock like the I/O busy time.
    uint32_t _own_inflight;
    bool _own_busy_open;
    uint64_t _own_busy_start_ns;
    uint64_t _own_busy_ns;
    void ownBusyBegin();
    void ownBusyEnd();
};

#endif /* TIMER_H */