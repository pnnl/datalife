#ifndef TRACKABLE_H
#define TRACKABLE_H

#include "MonitorPathPolicy.h"
#include "ReaderWriterLock.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <typeinfo>
#include <unordered_map>

template <class Key, class Value>
class Trackable {
  public:
    int id() {
        return _id;
    }

  static auto begin() {
    auto _iter =  _active.begin();
    return _iter;
  }
  static auto end() {return _active.end();}
   static auto next(auto _iter){ 
   std::advance(_iter, 1);
   return _iter;
  }
  // For the fork handlers in Lib.cpp, which hold it across fork().
  static ReaderWriterLock &registryLock() { return _activeMutex; }

    // One more user of an entry. Outside this class, only of one that cannot
    // go away meanwhile: the caller is a user of it, or has locked a registry
    // whose entries are.
    void incUsers() {
        _users.fetch_add(1);
    }

  protected:
    Trackable() : _id(_total.fetch_add(1)),
                  _users(0) {
    }

    ~Trackable() {}

    bool decUsers(unsigned int dec) {
        return (1 == _users.fetch_sub(dec));
    }

    // One user fewer, without locking the registry, unless this is the last:
    // that one leaves through RemoveTrackable(), which removes the entry too.
    bool dropUser() {
        unsigned int users = _users.load();
        while (users > 1) {
            if (_users.compare_exchange_weak(users, users - 1)) {
                return true;
            }
        }
        return false;
    }

    // createNew, reuseOld and the destructors in the Remove functions run under the
    // writer lock. Their I/O (debug prints, logging) is marked as the library's own
    // so it skips the wrappers, which look in the registries.
    static Value AddTrackable(Key k, std::function<Value(void)> createNew, std::function<void(Value)> reuseOld, bool &created) {
        Value ret = NULL;
        ScopedMonitorInternalIO internal_io;
        _activeMutex.writerLock();
        if (!_active.count(k)) {
            ret = createNew();
            if (ret) {
                _active[k] = ret;
                created = true;
            }
        }
        else {
            ret = _active[k];
            if (reuseOld)
                reuseOld(ret);
            // if (ret){
            //     DPRINTF("reusing %s\n",typeid(ret).name());
            // }
        }

        if (ret)
            ret->incUsers();

        _activeMutex.writerUnlock();
        return ret;
    }

    static Value AddTrackable(Key k, std::function<Value(void)> createNew, bool &created) {
        return AddTrackable(k, createNew, NULL, created);
    }

    static Value AddTrackable(Key k, std::function<Value(void)> createNew) {
        bool dontCare;
        return AddTrackable(k, createNew, NULL, dontCare);
    }

    static bool RemoveTrackable(Key k, unsigned int dec = 1) {
        return RemoveTrackableBy([&]() { return k; }, dec);
    }

    // The same for the key keyOf() returns once the registry is locked. For a key
    // that RekeyTrackable() can change: read before the lock is taken, it may by
    // then be the key of another entry.
    static bool RemoveTrackableBy(std::function<Key()> keyOf, unsigned int dec = 1) {
        // DPRINTF("in remove trackable %d\n",_active.count(k));
        bool ret = false;
        ScopedMonitorInternalIO internal_io;
        _activeMutex.writerLock();
        Key k = keyOf();
        if (_active.count(k)) {
            Value toDelete = _active[k];
            if (toDelete->decUsers(dec)) {
                // fDPRINTF(stderr,"trackable delete %s\n",typeid(toDelete).name());
                //std::cout<<"[MONITOR] " << "Deleting Trackable!!! " <<typeid(toDelete).name()<< std::endl;
                _active.erase(k);
                delete toDelete;
                ret = true;
            }
        }
        _activeMutex.writerUnlock();
        return ret;
    }

    // Puts replacement under k, or nothing if it is NULL, and returns the entry
    // that was there, out of the registry and whatever its users, for the caller
    // to delete; NULL if there was none. One step for the lookups: they find the
    // old entry or the new. changed is called with the registry still locked.
    template <typename Changed>
    static Value ReplaceTrackable(Key k, Value replacement, Changed changed) {
        Value old = NULL;
        _activeMutex.writerLock();
        auto it = _active.find(k);
        if (it != _active.end()) {
            old = it->second;
            _active.erase(it);
        }
        if (replacement) {
            _active[k] = replacement;
            replacement->incUsers();
        }
        changed();
        _activeMutex.writerUnlock();
        return old;
    }

    // Moves the entry under `from` to `to` and calls update on it, all under the
    // writer lock. Changes nothing and returns false if there is no entry under
    // `from` (found is false) or there already is one under `to` (found is true).
    static bool RekeyTrackable(Key from, Key to, std::function<void(Value)> update, bool &found) {
        bool moved = false;
        ScopedMonitorInternalIO internal_io;
        _activeMutex.writerLock();
        auto it = _active.find(from);
        found = (it != _active.end());
        if (found && !_active.count(to)) {
            Value value = it->second;
            _active.erase(it);
            _active[to] = value;
            if (update)
                update(value);
            moved = true;
        }
        _activeMutex.writerUnlock();
        return moved;
    }

    // Calls found on the entry under k, with the registry locked so that the
    // entry stays, and returns whether there is one.
    template <typename Found>
    static bool WithTrackable(Key k, Found found) {
        _activeMutex.readerLock();
        auto it = _active.find(k);
        const bool exists = (it != _active.end());
        if (exists) {
            found(it->second);
        }
        _activeMutex.readerUnlock();
        return exists;
    }

    static Value LookupTrackable(Key k) {
        Value ret = NULL;
        _activeMutex.readerLock();
        if (_active.count(k))
            ret = _active[k];
        _activeMutex.readerUnlock();
        return ret;
    }

    static void RemoveAllTrackable() {
        ScopedMonitorInternalIO internal_io;
        _activeMutex.writerLock();
        std::for_each(_active.begin(), _active.end(),
                      [](std::pair<Key, Value> element) {
                          delete element.second;
                      });
        _activeMutex.writerUnlock();
    }


private:
  int _id;
  std::atomic_uint _users;

  static std::atomic_int _total;
  static ReaderWriterLock _activeMutex;
  static std::unordered_map<Key, Value> _active;
};

template <class Key, class Value>
std::atomic_int Trackable<Key, Value>::_total(0);

template <class Key, class Value>
ReaderWriterLock Trackable<Key, Value>::_activeMutex;

template <class Key, class Value>
std::unordered_map<Key, Value> Trackable<Key, Value>::_active;

#endif /* TRACKABLE_H */