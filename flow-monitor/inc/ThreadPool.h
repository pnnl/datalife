#ifndef ThreadPool_H
#define ThreadPool_H
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <future>
#include <unordered_map>

template <class T>
class ThreadPool {
  public:
    ThreadPool(unsigned int maxThreads);
    ThreadPool(unsigned int maxThreads,std::string name);
    ~ThreadPool();

    unsigned int initiate();
    bool terminate(bool force = false);
    void wait();

    unsigned int addThreads(unsigned int numThreads);
    void addTask(T f);
    bool addThreadWithTask(T f);

    unsigned int getMaxThreads();
    int numTasks();

  private:
    unsigned int _maxThreads;
    unsigned int _users;

    std::atomic_bool _alive;
    std::atomic_uint _currentThreads;

    std::mutex _tMutex;
    std::vector<std::thread> _threads;
    std::unordered_map<std::thread::id, uint64_t> _idleTime;
    std::unordered_map<std::thread::id, uint64_t> _activeTime;

    std::mutex _qMutex;
    std::deque<T> _q;
    std::condition_variable _cv;

    void workLoop();
    std::string _name;
};

#endif /* ThreadPool_H */
