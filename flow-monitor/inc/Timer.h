#ifndef TIMER_H
#define TIMER_H

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
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
};

#endif /* TIMER_H */