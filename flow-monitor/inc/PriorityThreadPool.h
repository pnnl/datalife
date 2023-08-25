#ifndef PriorityThreadPool_H
#define PriorityThreadPool_H
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

template <class T>
class PriorityThreadPool {
  public:
    PriorityThreadPool(uint32_t maxThreads);
    PriorityThreadPool(uint32_t maxThreads,std::string name);
    ~PriorityThreadPool();

    uint32_t initiate();
    bool terminate(bool force = false);
    void wait();

    uint32_t addThreads(uint32_t numThreads);
    void addTask(uint32_t priority, T f);
    bool addThreadWithTask(uint32_t prior, T f);

    uint32_t getMaxThreads();
    int numTasks();

  private:
    struct TaskEntry {
        uint32_t priority;
        uint32_t timeStamp;
        T func;

        TaskEntry() {}

        TaskEntry(const TaskEntry &entry) {
            priority = entry.priority;
            timeStamp = entry.timeStamp;
            func = std::move(const_cast<T &>(entry.func)); // this pattern is an intricacie of move only types I dont fully understand
                                                           // found by searching for: "Broken interaction between std::priority_queue and move-only types"
        }

        TaskEntry(uint32_t prior, uint32_t time, T fun) : priority(prior),
                                                          timeStamp(time) {
            func = std::move(fun);
        }

        ~TaskEntry() {}

        TaskEntry &operator=(const TaskEntry &other) {
            if (this != &other) {
                priority = other.priority;
                timeStamp = other.timeStamp;
                func = std::move(const_cast<T &>(other.func)); // this pattern is an intricacie of move only types I dont fully understand
                                                               // found by searching for: "Broken interaction between std::priority_queue and move-only types"
            }
            return *this;
        }

        bool operator()(const TaskEntry &lhs, const TaskEntry &rhs) const {
            if (lhs.priority == rhs.priority)
                return lhs.timeStamp > rhs.timeStamp;
            return lhs.priority > rhs.priority;
        }
    };

    uint32_t _maxThreads;
    uint32_t _users;
    uint32_t _index;

    std::atomic_bool _alive;
    std::atomic_uint _currentThreads;

    std::mutex _tMutex;
    std::vector<std::thread> _threads;

    std::mutex _qMutex;
    std::priority_queue<TaskEntry, std::vector<TaskEntry>, TaskEntry> _q;
    std::condition_variable _cv;

    void workLoop();

    std::string _name;
};

#endif /* PriorityThreadPool_H */
