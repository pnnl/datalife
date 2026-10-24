#include "Timer.h"
#include "MonitorPathPolicy.h"
#include "Config.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <new>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#ifdef TIMER_JSON
#include <json.hpp> // for logging json file
#endif

extern char *__progname;

thread_local uint64_t _depth = 0;
thread_local uint64_t _current = 0;

char const *metricTypeName[] = {
    "monitor",
    "local",
    "system"};

char const *metricName[] = {
    "in_open",
    "out_open",
    "close",
    "read",
    "write",
    "seek",
    "stat",
    "fsync",
    "readv",
    "writev",
    "in_fopen",
    "out_fopen",
    "fclose",
    "fread",
    "fwrite",
    "ftell",
    "fseek",
    "fgetc",
    "fgets",
    "fputc",
    "fputs",
    "feof",
    "rewind",
    "constructor",
    "destructor",
    "dummy"};

// Created once, thread-safely, and never freed: the Timer is destroyed after
// ordinary statics are gone. A forked child reinitializes them (resetAfterFork()).
static std::map<std::string, Timer::FileSizeRecord> &fileSizeRegistry() {
    static auto *registry = new std::map<std::string, Timer::FileSizeRecord>();
    return *registry;
}

static std::mutex &fileSizeLock() {
    static auto *lock = new std::mutex();
    return *lock;
}

Timer::Timer() {
    // Created here, before other threads exist, so no fork() can happen while
    // another thread is creating them.
    fileSizeRegistry();
    fileSizeLock();

    for (int i = 0; i < lastMetric; i++) {
        for (int j = 0; j < last; j++) {
            _time[i][j] = 0;
            _cnt[i][j] = 0;
            _amt[i][j] = 0;
        }
    }

    stdoutcp = dup(1);
    myprogname = __progname;
    _thread_timers = new std::unordered_map<std::thread::id, Timer::ThreadMetric*>;
    _lock = new ReaderWriterLock();

    _io_busy_lock = new SpinLock();
    _io_inflight = 0;
    _io_busy_start_ns = 0;
    _io_busy_ns = 0;

    // Task caliper start: the Timer is created in the library constructor.
    _task_start_epoch_ns = epochNs();
    _task_start_steady_ns = steadyNs();
    _task_start_pid = getpid();
}

Timer::~Timer() {
    // Task caliper end: the Timer is deleted in the library destructor.
    const uint64_t task_end_epoch_ns = epochNs();
    const double task_wall_s = (steadyNs() - _task_start_steady_ns) / billion;
    uint64_t io_busy_ns = 0;
    {
        std::lock_guard<SpinLock> busy_guard(*_io_busy_lock);
        io_busy_ns = _io_busy_ns;
    }
    const double io_busy_s = io_busy_ns / billion;
    const double compute_time_s = task_wall_s - io_busy_s;
    // Per-call I/O time summed over all threads (excludes constructor/destructor/dummy);
    // exceeds wall time under concurrent I/O, hence compute time uses io_busy_s instead.
    double io_time_s = 0.0;
    uint64_t io_calls = 0;
    for (int i = 0; i < lastMetric; i++) {
        for (int j = 0; j < last; j++) {
            if (j == constructor || j == destructor || j == dummy)
                continue;
            io_time_s += _time[i][j].load(std::memory_order_relaxed) / billion;
            io_calls += _cnt[i][j].load(std::memory_order_relaxed);
        }
    }
    char hostname[256];
    std::string host_name = (gethostname(hostname, sizeof(hostname)) == 0) ? hostname : "unknown_host";

#ifdef TIMER_JSON
    const bool task_name_set = !Config::task_name_env.empty() && Config::task_name_env != "task_name";
    std::unordered_map<std::thread::id, Timer::ThreadMetric*>::iterator itor;
    if (Config::printStats) {
        nlohmann::json jsonOutput;
        jsonOutput[myprogname] = nlohmann::json::object();

        // Add main metrics to JSON
        for (int i = 0; i < lastMetric; i++) {
            for (int j = 0; j < last; j++) {
                jsonOutput[myprogname][metricTypeName[i]][metricName[j]] = 
                    { _time[i][j].load(std::memory_order_relaxed) / billion,
                      _cnt[i][j].load(std::memory_order_relaxed),
                      _amt[i][j].load(std::memory_order_relaxed) };
            }
        }

        // Add thread metrics to JSON
        for (itor = _thread_timers->begin(); itor != _thread_timers->end(); itor++) {
            // std::string threadId = std::to_string(itor->first);
            std::stringstream ss;
            ss << itor->first;
            std::string threadId = ss.str();
            jsonOutput[myprogname]["threads"][threadId] = nlohmann::json::object();

            for (int i = 0; i < lastMetric; i++) {
                for (int j = 0; j < last; j++) {
                    jsonOutput[myprogname]["threads"][threadId][metricTypeName[i]][metricName[j]] = 
                        { itor->second->time[i][j]->load(std::memory_order_relaxed) / billion,
                          itor->second->cnt[i][j]->load(std::memory_order_relaxed),
                          itor->second->amt[i][j]->load(std::memory_order_relaxed) };
                }
            }
        }
        // Task caliper block. start_pid is the parent's pid in a forked child.
        jsonOutput[myprogname]["task"] = {
            {"pid", (int64_t)getpid()},
            {"start_pid", (int64_t)_task_start_pid},
            {"hostname", host_name},
            {"program", myprogname},
            {"task_name", task_name_set ? Config::task_name_env : ""},
            {"start_epoch_ns", _task_start_epoch_ns},
            {"end_epoch_ns", task_end_epoch_ns},
            {"wall_time_s", task_wall_s},
            {"io_time_s", io_time_s},
            {"io_busy_time_s", io_busy_s},
            {"io_calls", io_calls},
            {"compute_time_s", compute_time_s},
        };

        // Final sizes of the traced files this process closed (both trace modes).
        jsonOutput[myprogname]["files"] = nlohmann::json::object();
        for (const auto &entry : fileSizes()) {
            const auto &rec = entry.second;
            nlohmann::json item = {
                {"file_size_source", rec.source},
                {"closed_epoch_ns", rec.closed_epoch_ns},
                {"closes", rec.closes},
            };
            if (rec.size_bytes >= 0) {
                item["file_size_bytes"] = rec.size_bytes;
                item["file_alloc_bytes"] = rec.alloc_bytes;
            } else {
                item["file_size_bytes"] = nullptr;
                item["file_alloc_bytes"] = nullptr;
            }
            jsonOutput[myprogname]["files"][entry.first] = item;
        }

        // Ensure dataLifeOutputPath is not empty
        if (Config::dataLifeOutputPath.empty()) {
            std::cerr << "Error: DATALIFE_OUTPUT_PATH is not set!" << std::endl;
            return;
        }        

        // Add task PID to file name 
        std::string jsonOutputFileName = "monitor_timer." + std::to_string(getpid()) + "-" + host_name + ".datalife.json";
        std::string fullPath = Config::dataLifeOutputPath + "/" + jsonOutputFileName;
        std::cerr << "write_trace_data(): writing to " << fullPath << std::endl;
        
        // // Ensure the output directory exists
        // std::filesystem::create_directories(Config::dataLifeOutputPath);
        
        // Open the file at the correct location (append mode)
        ScopedMonitorInternalIO internal_io;
        std::ofstream log_file(fullPath, std::ios::out | std::ios::app); 
        if (!log_file) {
            std::cerr << "Failed to open " << fullPath << std::endl;
        } else {
            log_file << jsonOutput.dump(4); // Pretty print with an indent of 4 spaces
            log_file.close();
        }

    //     std::ofstream log_file(jsonOutputFileName, std::ios::out | std::ios::app); // append
    //     // std::ofstream log_file("monitor_timer.datalife.json", std::ios::out | std::ios::trunc); // overwrite
    //     if (!log_file) {
    //         std::cerr << "Failed to open " << jsonOutputFileName << std::endl;
    //     } else {
    //         log_file << jsonOutput.dump(4); // Pretty print with an indent of 4 spaces
    //         // Add a "," to the end of the monitor_timer.datalife.json file
    //         // log_file << ","; // not needed when files are seperated into task PID
    //         log_file.close();
    //     }
    }

    for (itor = _thread_timers->begin(); itor != _thread_timers->end(); itor++) {
        delete itor->second;
    }
    delete _thread_timers;
    delete _lock;
    delete _io_busy_lock;

#else

    std::unordered_map<std::thread::id, Timer::ThreadMetric*>::iterator itor;
    if (Config::printStats) {
        std::stringstream ss;
        ss << std::fixed;
        for (int i = 0; i < lastMetric; i++) {
            for (int j = 0; j < last; j++) {
                ss << "[MONITOR] " << metricTypeName[i] << " " << metricName[j] << " " << _time[i][j].load(std::memory_order_relaxed) / billion << " "
                   << _cnt[i][j].load(std::memory_order_relaxed) << " " << _amt[i][j].load(std::memory_order_relaxed) << std::endl;
            }
        }
        //uint64_t thread_count = 0;
        for(itor = _thread_timers->begin(); itor != _thread_timers->end(); itor++) {
            //thread_count++;
            ss << std::endl << "[MONITOR] " << myprogname << " thread " << (*itor).first << std::endl;
            for (int i = 0; i < lastMetric; i++) {
                for (int j = 0; j < last; j++) {
                    ss << "[MONITOR] " << metricTypeName[i] << " " << metricName[j] << " " << itor->second->time[i][j]->load(std::memory_order_relaxed) / billion << " "
                     << itor->second->cnt[i][j]->load(std::memory_order_relaxed) << " " << itor->second->amt[i][j]->load(std::memory_order_relaxed) << std::endl;
                }
            }
        }
        for (const auto &entry : fileSizes())
            ss << "[MONITOR] file " << entry.first << " file_size_bytes " << entry.second.size_bytes << " file_alloc_bytes " << entry.second.alloc_bytes << " file_size_source " << entry.second.source << " closes " << entry.second.closes << std::endl;
        ss << "[MONITOR] task pid " << getpid() << " start_pid " << _task_start_pid << " host " << host_name << " wall_time_s " << task_wall_s << " io_time_s " << io_time_s << " io_busy_time_s " << io_busy_s << " compute_time_s " << compute_time_s << " start_epoch_ns " << _task_start_epoch_ns << " end_epoch_ns " << task_end_epoch_ns << std::endl;
        dprintf(stdoutcp, "[MONITOR] %s\n%s\n", myprogname.c_str(), ss.str().c_str());
    }
    
    for(itor = _thread_timers->begin(); itor != _thread_timers->end(); itor++) {
        delete itor->second;
    }
    delete _thread_timers;
    delete _lock;
    delete _io_busy_lock;

#endif
}

uint64_t Timer::getCurrentTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    auto value = now_ms.time_since_epoch();
    uint64_t ret = value.count();
    return ret + Config::referenceTime;
}

void Timer::resetAfterFork() {
    // Single-threaded here; locks may be held by threads that no longer exist, so
    // reinitialize them rather than acquire them. The maps start empty; the parent's
    // entries are abandoned, not freed. Nothing here allocates (see monitorForkChild).
    new (&fileSizeLock()) std::mutex();
    new (&fileSizeRegistry()) std::map<std::string, FileSizeRecord>();

    _lock->resetAfterFork();
    new (_thread_timers) std::unordered_map<std::thread::id, Timer::ThreadMetric*>();

    for (int i = 0; i < lastMetric; i++) {
        for (int j = 0; j < last; j++) {
            _time[i][j] = 0;
            _cnt[i][j] = 0;
            _amt[i][j] = 0;
        }
    }

    new (_io_busy_lock) SpinLock();
    _io_inflight = 0;
    _io_busy_start_ns = 0;
    _io_busy_ns = 0;

    // Caliper restarts at the fork; _task_start_pid keeps the parent's pid.
    _task_start_epoch_ns = epochNs();
    _task_start_steady_ns = steadyNs();
}

void Timer::recordFileSize(const std::string &path, int64_t size_bytes, int64_t alloc_bytes, const char *source) {
    std::lock_guard<std::mutex> guard(fileSizeLock());
    auto &rec = fileSizeRegistry()[path];
    rec.size_bytes = size_bytes;
    rec.alloc_bytes = alloc_bytes;
    rec.source = source ? source : "unavailable";
    rec.closed_epoch_ns = epochNs();
    rec.closes += 1;
}

std::map<std::string, Timer::FileSizeRecord> Timer::fileSizes() {
    std::lock_guard<std::mutex> guard(fileSizeLock());
    return fileSizeRegistry();
}

uint64_t Timer::epochNs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t Timer::steadyNs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

char *Timer::printTime() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto buf = ctime(&t);
    buf[strcspn(buf, "\n")] = 0;
    return buf;
}

int64_t Timer::getTimestamp() {
    return (int64_t)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

void Timer::ioBusyBegin() {
    std::lock_guard<SpinLock> busy_guard(*_io_busy_lock);
    if (_io_inflight++ == 0) {
        _io_busy_start_ns = steadyNs();
    }
}

void Timer::ioBusyEnd(Metric metric) {
    std::lock_guard<SpinLock> busy_guard(*_io_busy_lock);
    if (_io_inflight == 0) {
        return; // unbalanced end(); never underflow
    }
    if (--_io_inflight == 0) {
        // Not I/O; excluded as in io_time_s.
        if (metric != constructor && metric != destructor && metric != dummy) {
            const uint64_t now = steadyNs();
            if (now > _io_busy_start_ns)
                _io_busy_ns += now - _io_busy_start_ns;
        }
    }
}

void Timer::start() {
    if (!_depth) {
        _current = getCurrentTime();
        ioBusyBegin();
    }
    _depth++;

    if (Config::ThreadStats){
        auto id = std::this_thread::get_id();
        _lock->readerLock();
        if (_thread_timers->find(id) == _thread_timers->end()){
            _lock->readerUnlock();
            addThread(id);
            _lock->readerLock();
        }
        if (!(*_thread_timers)[id]->depth->load(std::memory_order_relaxed))
            (*_thread_timers)[id]->current->store(getCurrentTime(), std::memory_order_relaxed);
        (*_thread_timers)[id]->depth->fetch_add(1, std::memory_order_relaxed);
        _lock->readerUnlock();
    }
}

void Timer::end(MetricType type, Metric metric) {
    if (_depth == 1) {
        _time[type][metric].fetch_add(getCurrentTime() - _current, std::memory_order_relaxed);
        _cnt[type][metric].fetch_add(1, std::memory_order_relaxed);
        ioBusyEnd(metric);
    }
    _depth--;
    if (Config::ThreadStats){
        auto id = std::this_thread::get_id();
        _lock->readerLock();
        if (_thread_timers->find(id) == _thread_timers->end()){
            _lock->readerUnlock();
            addThread(id);
            _lock->readerLock();
        }
        if ((*_thread_timers)[id]->depth->load(std::memory_order_relaxed) == 1) {
            uint64_t x = getCurrentTime() - (*_thread_timers)[id]->current->load(std::memory_order_relaxed);
            (*_thread_timers)[id]->time[type][metric]->fetch_add(x, std::memory_order_relaxed);
            (*_thread_timers)[id]->cnt[type][metric]->fetch_add(1, std::memory_order_relaxed);
        }
        (*_thread_timers)[id]->depth->fetch_sub(1, std::memory_order_relaxed);
        _lock->readerUnlock();
    }

    
#ifdef TIMER_JSON
    // TODO: Remove the last comma from the monitor_timer.datalife.json file
#endif
}

void Timer::addAmt(MetricType type, Metric metric, uint64_t amt) {
    _amt[type][metric].fetch_add(amt, std::memory_order_relaxed);
    if (Config::ThreadStats){
        auto id = std::this_thread::get_id();
        _lock->readerLock();
        if (_thread_timers->find(id) == _thread_timers->end()){
            _lock->readerUnlock();
            addThread(id);
            _lock->readerLock();
        }
        (*_thread_timers)[id]->amt[type][metric]->fetch_add(amt, std::memory_order_relaxed);
        _lock->readerUnlock();
    }
}

// void Timer::threadStart(std::thread::id id) {
//     if (!(*_thread_timers)[id]->depth->load(std::memory_order_relaxed))
//         (*_thread_timers)[id]->current->store(getCurrentTime(), std::memory_order_relaxed);
//     (*_thread_timers)[id]->depth->fetch_add(1, std::memory_order_relaxed);
// }

// void Timer::threadEnd(std::thread::id id, MetricType type, Metric metric) {
//     if ((*_thread_timers)[id]->depth->load(std::memory_order_relaxed) == 1) {
//         uint64_t x = getCurrentTime() - (*_thread_timers)[id]->current->load(std::memory_order_relaxed);
//         (*_thread_timers)[id]->time[type][metric]->fetch_add(x, std::memory_order_relaxed);
//         (*_thread_timers)[id]->cnt[type][metric]->fetch_add(1, std::memory_order_relaxed);
//     }
//     (*_thread_timers)[id]->depth->fetch_sub(1, std::memory_order_relaxed);
// }

// void Timer::threadAddAmt(std::thread::id id, MetricType type, Metric metric, uint64_t amt) {
//     (*_thread_timers)[id]->amt[type][metric]->fetch_add(amt, std::memory_order_relaxed);
// }

void Timer::addThread(std::thread::id id) {
    //(*_thread_timers)[id] = *(new Timer::ThreadMetric());
    _lock->writerLock();
    (*_thread_timers)[id] = new Timer::ThreadMetric();
    _lock->writerUnlock();
    // std::cout <<"adding timer thread id "<<id<<std::endl;
}

// bool Timer::checkThread(std::thread::id id, bool addIfNotFound) {
//     _lock->readerLock();
//     bool ret = (_thread_timers->find(id) != _thread_timers->end());
//     _lock->readerUnlock();
    
//     if (!ret && addIfNotFound)
//         addThread(id);

//     return ret;
// }

Timer::ThreadMetric::ThreadMetric() {
    current = new std::atomic<uint64_t>(std::uint64_t(0));
    depth = new std::atomic<uint64_t>(std::uint64_t(0));
    for (int i = 0; i < lastMetric; i++) {
        for (int j = 0; j < last; j++) {
            time[i][j] = new std::atomic<uint64_t>(std::uint64_t(0));
            cnt[i][j] = new std::atomic<uint64_t>(std::uint64_t(0));
            amt[i][j] = new std::atomic<uint64_t>(std::uint64_t(0));
        }
    }
}

Timer::ThreadMetric::~ThreadMetric() {
    delete current;
    delete depth;
        for (int i = 0; i < lastMetric; i++) {
            for (int j = 0; j < last; j++) {
                delete time[i][j];
                delete cnt[i][j];
                delete amt[i][j];
            }
    }
}
