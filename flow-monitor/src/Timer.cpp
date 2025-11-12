#include "Timer.h"
#include "Config.h"
#include <atomic>
#include <fstream>
#include <iostream>
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

Timer::Timer() {
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
}

Timer::~Timer() {

#ifdef TIMER_JSON
    std::unordered_map<std::thread::id, Timer::ThreadMetric*>::iterator itor;
    if (Config::printStats) {
        nlohmann::json jsonOutput;
        jsonOutput[myprogname] = nlohmann::json::object();

        // Add main metrics to JSON
        for (int i = 0; i < lastMetric; i++) {
            for (int j = 0; j < last; j++) {
                jsonOutput[myprogname][metricTypeName[i]][metricName[j]] = 
                    { _time[i][j] / billion, _cnt[i][j], _amt[i][j] };
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
        // Get the current host name
        char hostname[256]; // Buffer to store the host name
        std::string host_name = (gethostname(hostname, sizeof(hostname)) == 0) ? hostname : "unknown_host";
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

#else

    std::unordered_map<std::thread::id, Timer::ThreadMetric*>::iterator itor;
    if (Config::printStats) {
        std::stringstream ss;
        ss << std::fixed;
        for (int i = 0; i < lastMetric; i++) {
            for (int j = 0; j < last; j++) {
                ss << "[MONITOR] " << metricTypeName[i] << " " << metricName[j] << " " << _time[i][j] / billion << " " << _cnt[i][j] << " " << _amt[i][j] << std::endl;
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
        dprintf(stdoutcp, "[MONITOR] %s\n%s\n", myprogname.c_str(), ss.str().c_str());
    }
    
    for(itor = _thread_timers->begin(); itor != _thread_timers->end(); itor++) {
        delete itor->second;
    }
    delete _thread_timers;

#endif
}

uint64_t Timer::getCurrentTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    auto value = now_ms.time_since_epoch();
    uint64_t ret = value.count();
    return ret + Config::referenceTime;
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

void Timer::start() {
    if (!_depth)
        _current = getCurrentTime();
    _depth++;

    if (Config::ThreadStats){
        auto id = std::this_thread::get_id();
        _lock.readerLock();
        if (_thread_timers->find(id) == _thread_timers->end()){
            _lock.readerUnlock();
            addThread(id);
            _lock.readerLock();
        }
        if (!(*_thread_timers)[id]->depth->load(std::memory_order_relaxed))
            (*_thread_timers)[id]->current->store(getCurrentTime(), std::memory_order_relaxed);
        (*_thread_timers)[id]->depth->fetch_add(1, std::memory_order_relaxed);
        _lock.readerUnlock();
    }
}

void Timer::end(MetricType type, Metric metric) {
    if (_depth == 1) {
        _time[type][metric] += getCurrentTime() - _current;
        _cnt[type][metric]++;
    }
    _depth--;
    if (Config::ThreadStats){
        auto id = std::this_thread::get_id();
        _lock.readerLock();
        if (_thread_timers->find(id) == _thread_timers->end()){
            _lock.readerUnlock();
            addThread(id);
            _lock.readerLock();
        }
        if ((*_thread_timers)[id]->depth->load(std::memory_order_relaxed) == 1) {
            uint64_t x = getCurrentTime() - (*_thread_timers)[id]->current->load(std::memory_order_relaxed);
            (*_thread_timers)[id]->time[type][metric]->fetch_add(x, std::memory_order_relaxed);
            (*_thread_timers)[id]->cnt[type][metric]->fetch_add(1, std::memory_order_relaxed);
        }
        (*_thread_timers)[id]->depth->fetch_sub(1, std::memory_order_relaxed);
        _lock.readerUnlock();
    }

    
#ifdef TIMER_JSON
    // TODO: Remove the last comma from the monitor_timer.datalife.json file
#endif
}

void Timer::addAmt(MetricType type, Metric metric, uint64_t amt) {
    _amt[type][metric] += amt;
    if (Config::ThreadStats){
        auto id = std::this_thread::get_id();
        _lock.readerLock();
        if (_thread_timers->find(id) == _thread_timers->end()){
            _lock.readerUnlock();
            addThread(id);
            _lock.readerLock();
        }
        (*_thread_timers)[id]->amt[type][metric]->fetch_add(amt, std::memory_order_relaxed);
        _lock.readerUnlock();
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
    _lock.writerLock();
    (*_thread_timers)[id] = new Timer::ThreadMetric();
    _lock.writerUnlock();
    // std::cout <<"adding timer thread id "<<id<<std::endl;
}

// bool Timer::checkThread(std::thread::id id, bool addIfNotFound) {
//     _lock.readerLock();
//     bool ret = (_thread_timers->find(id) != _thread_timers->end());
//     _lock.readerUnlock();
    
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