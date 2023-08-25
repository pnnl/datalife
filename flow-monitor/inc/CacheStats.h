#ifndef CACHESTATS_H
#define CACHESTATS_H

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ReaderWriterLock.h"

class CacheStats {
  public:
    enum MetricType {
        request = 0,
        prefetch,
        lastMetric
    };

    enum Metric {
        hits = 0,
        misses,
        evictions,
        backout,
        prefetches,
        stalls,  //time i stall other caches
        stalled, //time im stalled
        ovh,
        lock,
        write,
        read,
        constructor,
        destructor,
        last
    };

    CacheStats();
    ~CacheStats();

    void print(std::string cacheName);

    void start(bool prefetch, Metric metric, std::thread::id id=std::this_thread::get_id());
    void end(bool prefetch, Metric metric, std::thread::id id=std::this_thread::get_id());
    void addTime(bool prefetch, Metric metric, uint64_t time, std::thread::id id=std::this_thread::get_id(), uint64_t cnt = 0);
    void addAmt(bool prefetch, Metric metric, uint64_t mnt, std::thread::id id=std::this_thread::get_id());
    // void threadStart(std::thread::id id);
    // void threadEnd(std::thread::id id, bool prefetch, Metric metric);
    // void threadAddTime(std::thread::id id, bool prefetch, Metric metric, uint64_t time, uint64_t cnt = 0);
    // void threadAddAmt(std::thread::id id, bool prefetch, Metric metric, uint64_t mnt);
  
    

    static uint64_t getCurrentTime();
    static char *printTime();
    static int64_t getTimestamp();

  private:
    class ThreadMetric {
      public:
        ThreadMetric();
        ~ThreadMetric();
        std::atomic<uint64_t> *depth;
        std::atomic<uint64_t> *current[100];
        std::atomic<uint64_t> *time[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
        std::atomic<uint64_t> *cnt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
        std::atomic<uint64_t> *amt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    };

    void addThread(std::thread::id id);
    // bool checkThread(std::thread::id id, bool addIfNotFound);
    const double billion = 1000000000;
    std::atomic<uint64_t> _time[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    std::atomic<uint64_t> _cnt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    std::atomic<uint64_t> _amt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    std::unordered_map<std::thread::id, CacheStats::ThreadMetric*> *_thread_stats;
    ReaderWriterLock _lock;

    int stdoutcp;
    std::string myprogname;
};

#endif /* CACHESTATS_H */