#ifndef TRACKABLE_H
#define TRACKABLE_H

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

  protected:
    Trackable() : _id(_total.fetch_add(1)),
                  _users(0) {
    }

    ~Trackable() {}

    void incUsers() {
        _users.fetch_add(1);
    }

    bool decUsers(unsigned int dec) {
        return (1 == _users.fetch_sub(dec));
    }

    static Value AddTrackable(Key k, std::function<Value(void)> createNew, std::function<void(Value)> reuseOld, bool &created) {
        Value ret = NULL;
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
        // DPRINTF("in remove trackable %d\n",_active.count(k));
        bool ret = false;
        _activeMutex.writerLock();
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

    static Value LookupTrackable(Key k) {
        Value ret = NULL;
        _activeMutex.readerLock();
        if (_active.count(k))
            ret = _active[k];
        _activeMutex.readerUnlock();
        return ret;
    }

    static void RemoveAllTrackable() {
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
