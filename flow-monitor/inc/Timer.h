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
        dummy, //use to match calls to start...
        last
    };

    Timer();
    ~Timer();

    void start();
    void end(MetricType type, Metric metric);
    void addAmt(MetricType type, Metric metric, uint64_t amt);

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
    uint64_t _time[Timer::MetricType::lastMetric][Timer::Metric::last];
    uint64_t _cnt[Timer::MetricType::lastMetric][Timer::Metric::last];
    uint64_t _amt[Timer::MetricType::lastMetric][Timer::Metric::last];
    ReaderWriterLock *_lock; // heap-allocated so resetAfterFork() can replace it

    int stdoutcp;
    std::string myprogname;

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
};

#endif /* TIMER_H */