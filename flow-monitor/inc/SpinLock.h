#ifndef DATALIFE_SPINLOCK_H
#define DATALIFE_SPINLOCK_H

#include <atomic>
#include <thread>

// Test-and-test-and-set spinlock for the short critical sections on the I/O hot
// path; a contended std::mutex costs more than the work it guards. lock_guard-compatible.
class SpinLock {
  public:
    SpinLock() : _locked(false) {}
    SpinLock(const SpinLock &) = delete;
    SpinLock &operator=(const SpinLock &) = delete;

    void lock() {
        while (_locked.exchange(true, std::memory_order_acquire)) {
            while (_locked.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }
        }
    }

    void unlock() {
        _locked.store(false, std::memory_order_release);
    }

  private:
    std::atomic<bool> _locked;
};

#endif /* DATALIFE_SPINLOCK_H */