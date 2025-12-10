#include "PriorityThreadPool.h"
#include "Cache.h"
#include <iostream>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

template <class T>
PriorityThreadPool<T>::PriorityThreadPool(uint32_t maxThreads) : _maxThreads(maxThreads),
                                                                 _users(0),
                                                                 _index(0),
                                                                 _alive(true),
                                                                 _currentThreads(0), _name("pool") {
}

template <class T>
PriorityThreadPool<T>::PriorityThreadPool(uint32_t maxThreads, std::string name) : _maxThreads(maxThreads),
                                                                                   _users(0),
                                                                                   _index(0),
                                                                                   _alive(true),
                                                                                   _currentThreads(0), _name(name) {
}

template <class T>
PriorityThreadPool<T>::~PriorityThreadPool() {
    std::cerr << "[MONITOR] "
              << "deleting priority pool before: "<<_name<<" " << _users << " " << _q.size() << " " << std::endl;
    terminate(true);
    std::cerr << "[MONITOR] "
              << "deleting priority pool: "<<_name<<" " << _users << " " << _q.size() << " " << std::endl;
}

template <class T>
uint32_t PriorityThreadPool<T>::addThreads(uint32_t numThreads) {
    uint32_t threadsToAdd = numThreads;
    std::unique_lock<std::mutex> lock(_tMutex);
    if (_alive.load()) {
        _users++;

        uint32_t currentThreads = _threads.size();
        if (threadsToAdd + currentThreads > _maxThreads)
            threadsToAdd = _maxThreads - currentThreads;

        _currentThreads.fetch_add(threadsToAdd);
        for (uint32_t i = 0; i < threadsToAdd; i++)
            _threads.push_back(std::thread([this] { std::cout<<"prioritythread pool thread"<<std::endl;workLoop(); }));
    }
    lock.unlock();
    return threadsToAdd;
}

template <class T>
uint32_t PriorityThreadPool<T>::initiate() {
    return addThreads(_maxThreads);
}

template <class T>
bool PriorityThreadPool<T>::terminate(bool force) {
    bool ret = false;
    std::unique_lock<std::mutex> lock(_tMutex);
    if (_users) //So we can join and terminate if there are no users
        _users--;
    if (!_users || force) {
        uint64_t cur_size = _q.size();
        uint64_t timeout_cnt =0;
        while (_q.size() && timeout_cnt < 10){ //let q drain as long as it appears to be making progress otherwise time out
            timeout_cnt++;
            std::this_thread::sleep_for (std::chrono::seconds(1));
            if (cur_size != _q.size()){
                timeout_cnt=0;
                cur_size = _q.size();
            }
        }
        if (_q.size()) {
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
void PriorityThreadPool<T>::addTask(uint32_t priority, T f) {
    std::unique_lock<std::mutex> lock(_qMutex);
    TaskEntry entry(priority, 0, std::move(f));
    entry.timeStamp = _index++;
    _q.push(entry);
    lock.unlock();
    _cv.notify_one();
}

template <class T>
void PriorityThreadPool<T>::workLoop() {
    TaskEntry task;
    while (_alive.load()) {
        std::unique_lock<std::mutex> lock(_qMutex);

        //Don't make this a wait or else we will never be able to join
        if (_q.empty()) {
            _cv.wait(lock);
        }

        //Check task since we are waking everyone up when it is time to join
        bool popped = !_q.empty();
        if (popped) {
            // task = std::move(const_cast<TaskEntry &>(_q.top()));
            task = _q.top();
            _q.pop();
        }

        lock.unlock();

        if (popped) {
            // std::cout << "Excuting task with priority: " << task.priority << std::endl;
            task.func();
            popped = false;
        }
    }
    //This is the end counter we need to decrement
    _currentThreads.fetch_sub(1);
    if (_q.size() > _currentThreads) {
        std::cerr << "[MONITOR DEBUG] " << _name << " not empty while closing!!!! remaining threads: " << _currentThreads << " remaining tasks: " << _q.size() << std::endl;
    }
}

template <class T>
void PriorityThreadPool<T>::wait() {
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
uint32_t PriorityThreadPool<T>::getMaxThreads() {
    return _maxThreads;
}

template <class T>
bool PriorityThreadPool<T>::addThreadWithTask(uint32_t priority, T f) {
    uint32_t ret = addThreads(1);
    addTask(priority, std::move(f));
    return (ret == 1);
}

template <class T>
int PriorityThreadPool<T>::numTasks() {
    return _q.size();
}

template class PriorityThreadPool<std::packaged_task<Request *()>>;
template class PriorityThreadPool<std::packaged_task<std::future<Request *>()>>;
template class PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>>;
template class PriorityThreadPool<std::function<void()>>;
