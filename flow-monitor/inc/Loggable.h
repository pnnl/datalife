#ifndef LOGGABLE_H
#define LOGGABLE_H

#include "Config.h"
#include "ThreadPool.h"
#include "Timer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>

class Loggable {
  public:
    struct log {
        std::unique_lock<std::mutex> lk;
        log(Loggable *me)
            : lk(std::unique_lock<std::mutex>(*mtx_cout)), _parent(me) {
            if (_parent->_log) {
                *_parent->_o << "[MONITOR] " << Timer::getCurrentTime() << " ";
            }
        }
        log(): lk(std::unique_lock<std::mutex>(*mtx_cout)){
            _parent = new Loggable();
            *_parent->_o << "[MONITOR] " ;
        }
        ~log() {
            if(_parent->_freestanding){
                delete _parent;
            }
        }

        template <typename T>
        log &operator<<(const T &_t) {
            if (_parent->_log) {
                *_parent->_o << _t;
            }
            return *this;
        }

        log &operator<<(std::ostream &(*fp)(std::ostream &)) {
            if (_parent->_log) {
                *_parent->_o << fp;
            }
            return *this;
        }

      private:
        Loggable *_parent;
    };

    struct debug {
        std::unique_lock<std::mutex> lk;
        debug(Loggable *me)
            : lk(std::unique_lock<std::mutex>(*mtx_cout)), _parent(me) {
            if (_parent->_log) {
                *_parent->_o << "[MONITOR DEBUG] " << Timer::printTime() << " ";
            }
        }
        debug(): lk(std::unique_lock<std::mutex>(*mtx_cout)){
            _parent = new Loggable();
            *_parent->_o << "[MONITOR DEBUG] " ;
        }
        ~debug() {
            if(_parent->_freestanding){
                delete _parent;
            }
        }

        template <typename T>
        debug &operator<<(const T &_t) {
            if (_parent->_log) {
                *_parent->_o << _t;
            }
            return *this;
        }

        debug &operator<<(std::ostream &(*fp)(std::ostream &)) {
            if (_parent->_log) {
                *_parent->_o << fp;
            }
            return *this;
        }

      private:
        Loggable *_parent;
    };

    struct err {
        std::unique_lock<std::mutex> lk;
        err(Loggable *me)
            : lk(std::unique_lock<std::mutex>(*mtx_cout)), _parent(me) {
            *_parent->_o << "[MONITOR ERROR] " << Timer::printTime() << " ";
        }
        err(): lk(std::unique_lock<std::mutex>(*mtx_cout)){
            _parent = new Loggable();
            *_parent->_o << "[MONITOR ERROR] " ;
        }
        ~err() {
            if(_parent->_freestanding){
                delete _parent;
            }
        }

        template <typename T>
        err &operator<<(const T &_t) {
            *_parent->_o << _t;
            return *this;
        }

        err &operator<<(std::ostream &(*fp)(std::ostream &)) {
            *_parent->_o << fp;
            return *this;
        }

      private:
        Loggable *_parent;
    };
    static std::mutex *mtx_cout;

    Loggable(bool log, std::string fileName) : _log(log), _freestanding(false), _o(NULL) {
        if (!mtx_cout) {
            Loggable::mtx_cout = new std::mutex();
        }
        if (log && Config::WriteFileLog) {
            std::stringstream ss;
            const char *env_p = std::getenv("MONITOR_LOG_PATH");
            if (env_p == NULL) {
                ss << "./";
            }
            else {
                ss << env_p;
            }

            if (fileName.size()) {
                ss << fileName << ".tzrlog";
                std::string tstr;
                std::stringstream tss;
                while (getline(ss, tstr, '/')) { //construct results path if not exists
                    if (tstr.find(".tzrlog") == std::string::npos) {
                        tss << tstr << "/";
                        mkdir(tss.str().c_str(), 0777);
                    }
                }
                std::cerr << "[MONITOR] "
                          << "opening " << ss.str() << std::endl;
                _of.open(ss.str(), std::ofstream::out);
                _o = &_of;
            }
            else {
                _o = &std::cerr;
            }
        }
        else {
            _o = &std::cerr;
        }
    }

    Loggable() : _log(true), _freestanding(true), _o(NULL) {
        if (!mtx_cout) {
            Loggable::mtx_cout = new std::mutex();
        }
        _o = &std::cout;
        // std::cout << &std::cout << " 2 " << _o << std::endl;
    }

    template <class T>
    Loggable &operator<<(const T &val) {
        if (_log) {
            if (_of.is_open()) {
                _of << val;
            }
            else {
                std::cerr << val;
            }
        }
        return *this;
    }

    Loggable &operator<<(std::ostream &(*f)(std::ostream &)) {
        if (_log) {
            if (_of.is_open()) {
                f(_of);
            }
            else {
                std::cerr << f;
            }
        }
        return *this;
    }

    Loggable &operator<<(std::ostream &(*f)(std::ios &)) {
        if (_log) {
            if (_of.is_open()) {
                f(_of);
            }
            else {
                std::cerr << f;
            }
        }
        return *this;
    }

    Loggable &operator<<(std::ostream &(*f)(std::ios_base &)) {
        if (_log) {
            if (_log && _of.is_open()) {
                f(_of);
            }
            else {
                std::cerr << f;
            }
        }
        return *this;
    }

    static ThreadPool<std::function<void()>> *_writePool;

  protected:
    ~Loggable() {
        if (_log && _of.is_open())
            _of.close();
    }

  private:
    std::ofstream _of;
    bool _log;
    bool _freestanding;
    std::ostream *_o;
    friend log;
    friend debug;
    friend err;

    // static std::atomic<bool> _initiated;
};

#endif /* LOGGABLE_H */
