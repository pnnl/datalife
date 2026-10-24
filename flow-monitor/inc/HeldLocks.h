#ifndef DATALIFE_HELD_LOCKS_H
#define DATALIFE_HELD_LOCKS_H

#include <atomic>
#include <mutex>

// Per-thread count of DataLife locks held or being taken. fork() can be called
// from a signal handler that interrupted the thread inside a locked section;
// the fork prepare handler in Lib.cpp checks the count so it does not wait for
// a lock that only the interrupted code can release. The lock types count
// before trying to take a lock and after releasing it.
//
// Updated on every lock and unlock, so it is inline. It is __thread, not
// thread_local: code that sees only an extern thread_local declaration checks
// for a dynamic initializer to call on every access.
extern __thread unsigned int heldLockCount;

// The signal fences keep the compiler from moving the count across the lock
// operation, so a signal handler never sees a held lock with a zero count.
inline void heldLockAcquire() {
    ++heldLockCount;
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

inline void heldLockRelease() {
    std::atomic_signal_fence(std::memory_order_seq_cst);
    --heldLockCount;
}

inline unsigned int heldLocks() {
    return heldLockCount;
}

// std::mutex that keeps the count. lock_guard-compatible.
class CountedMutex {
  public:
    void lock() {
        heldLockAcquire();
        _mutex.lock();
    }

    void unlock() {
        _mutex.unlock();
        heldLockRelease();
    }

  private:
    std::mutex _mutex;
};

#endif /* DATALIFE_HELD_LOCKS_H */
