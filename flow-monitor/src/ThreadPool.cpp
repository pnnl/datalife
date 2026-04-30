#include "ThreadPool.h"
#include "Timer.h"
#include "Cache.h"
#include <iostream>
#include <unordered_map>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

template <class T>
ThreadPool<T>::ThreadPool(unsigned int maxThreads) : _maxThreads(maxThreads),
                                                  _users(0),
                                                  _alive(true),
                                                  _currentThreads(0),_name("pool") {
}

template <class T>
ThreadPool<T>::ThreadPool(unsigned int maxThreads,std::string name) : _maxThreads(maxThreads),
                                                  _users(0),
                                                  _alive(true),
                                                  _currentThreads(0),_name(name) {
}

template <class T>
ThreadPool<T>::~ThreadPool() {
   std::cerr << "[MONITOR] "
              << "deleting thread pool before: "<<_name<<" " << _users << " " << _currentThreads << " " << std::endl;
    terminate(true);
    std::cerr << "[MONITOR] "
              << "deleting thread pool: "<<_name<<" " << _users << " " << _currentThreads << " " << std::endl;

    std::unordered_map<std::thread::id, uint64_t>::iterator itor;

    for(itor = _activeTime.begin(); itor != _activeTime.end(); ++itor){
        std::cerr << "[MONITOR] threadPool "<<_name<<" thread "<<(*itor).first << std::endl;
        std::cerr << "[MONITOR] "<<_name<<" activeTime "<<(double)(*itor).second/1000000000.0<<std::endl;
        std::cerr << "[MONITOR] "<<_name<<" idleTime "<<(double)_idleTime[(*itor).first]/1000000000.0<<std::endl;
    }
    
}

template <class T>
unsigned int ThreadPool<T>::addThreads(unsigned int numThreads) {
    unsigned int threadsToAdd = numThreads;
    std::unique_lock<std::mutex> lock(_tMutex);
    if (_alive.load()) {
        _users++;

        unsigned int currentThreads = _threads.size();
        if (threadsToAdd + currentThreads > _maxThreads)
            threadsToAdd = _maxThreads - currentThreads;

        // std::cout<<"adding new threads "<<threadsToAdd<<std::endl;
        _currentThreads.fetch_add(threadsToAdd);
        for (unsigned int i = 0; i < threadsToAdd; i++)
            _threads.push_back(std::thread([this] {std::cout<<"thread pool thread"<<std::endl; workLoop(); }));
    }
    lock.unlock();
    return threadsToAdd;
}

template <class T>
unsigned int ThreadPool<T>::initiate() {
    return addThreads(_maxThreads);
}

template <class T>
bool ThreadPool<T>::terminate(bool force) {
    bool ret = false;
    std::unique_lock<std::mutex> lock(_tMutex);
    if (_users) //So we can join and terminate if there are no users
        _users--;
    if (!_users || force) {
        uint64_t cur_size = _q.size();
        uint64_t timeout_cnt =0;
        while(_q.size() && timeout_cnt < 10){ //let q drain as long as it appears to be making progress otherwise time out
            timeout_cnt++;
            std::this_thread::sleep_for (std::chrono::seconds(1));
            if (cur_size != _q.size()){
                timeout_cnt=0;
                cur_size = _q.size();
            }
        }
        if (timeout_cnt >= 10) {
            std::cerr<<"[MONITOR ERROR] priority thread pool: "<<_name<<" timed out with a non empty queue: "<<_q.size()<<std::endl;
        }
        //This is to deal with the conditional variable
        _alive.store(false);
        while (_currentThreads.load())
            _cv.notify_all();
        //At this point we know the threads have exited
        while (_threads.size()) {
            _threads.back().join();
            _threads.pop_back();
        }
        //if the force is called then we can't reuse the pool
        _alive.store(!force);
        ret = true;
    }
    lock.unlock();
    return ret;
}

template <class T>
void ThreadPool<T>::addTask(T f) {
    std::unique_lock<std::mutex> lock(_qMutex);
    _q.push_back(std::move(f));
    lock.unlock();
    _cv.notify_one();
}

template <class T>
void ThreadPool<T>::workLoop() {
    // std::cout<<"starting workloop "<<std::this_thread::get_id()<<std::endl;
    
    uint64_t idleSum=0;
    uint64_t activeSum=0;
    T task;
    while (_alive.load()) {
        uint64_t idleStart = Timer::getCurrentTime();
        std::unique_lock<std::mutex> lock(_qMutex);

        //Don't make this a wait or else we will never be able to join
        if (_q.empty()) {
            _cv.wait(lock);
        }

        //Check task since we are waking everyone up when it is time to join
        bool popped = !_q.empty();
        if (popped) {
            task = std::move(_q.front());
            _q.pop_front();
        }

        lock.unlock();
        idleSum +=Timer::getCurrentTime()- idleStart;

        if (popped) {
            uint64_t activeStart = Timer::getCurrentTime();
            task();
            activeSum +=Timer::getCurrentTime()- activeStart;
            popped = false;
        }
    }
    std::thread::id thread_id = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(_qMutex);
    _activeTime[thread_id]=activeSum;
    _idleTime[thread_id]=idleSum;
    lock.unlock();
    //This is the end counter we need to decrement
    _currentThreads.fetch_sub(1);
    if (_q.size() > _currentThreads){
        std::cerr<<"[MONITOR DEBUG] "<<_name<<" not empty while closing!!!! remaining threads: "<<_currentThreads<<" remaining tasks: "<<_q.size()<<std::endl;
    }
}

template <class T>
void ThreadPool<T>::wait() {
    bool full = true;

    while (full) {
        std::unique_lock<std::mutex> lock(_qMutex);
        full = !_q.empty();
        lock.unlock();

        if (full) {
            std::this_thread::yield();
        }
    }
}

template <class T>
unsigned int ThreadPool<T>::getMaxThreads() {
    return _maxThreads;
}

template <class T>
bool ThreadPool<T>::addThreadWithTask(T f) {
    unsigned int ret = addThreads(1);
    addTask(std::move(f));
    return (ret == 1);
}

template <class T>
int ThreadPool<T>::numTasks() {
    return _q.size();
}

template class ThreadPool<std::packaged_task<std::pair<char*,Cache*>()>>;
template class ThreadPool<std::packaged_task<std::future<std::pair<char*,Cache*>>()>>;
template class ThreadPool<std::function<void()>>;
