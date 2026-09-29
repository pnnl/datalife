#ifndef TIMER_H
#define TIMER_H

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "ReaderWriterLock.h"

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
    ReaderWriterLock _lock;

    int stdoutcp;
    std::string myprogname;

    // Task caliper: stamps taken when the library initializes (constructor, before
    // main) and tears down (destructor, at exit), i.e. the traced process lifetime.
    uint64_t _task_start_epoch_ns;
    uint64_t _task_start_steady_ns;
    pid_t _task_start_pid;
};

#endif /* TIMER_H */