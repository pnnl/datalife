#include "Prefetcher.h"
#include "Loggable.h"

#include <vector>
#include <iterator>

Prefetcher::Prefetcher(std::string name) : Loggable(Config::PrefetcherLog, "Prefetcher"),
                                           _name(name){

    //*this << "[MONITOR] >>>>> " << "HOLA! I'm your father! " << _name << ">>>>> " << std::endl;
}


Prefetcher::~Prefetcher() {
    //*this << "[MONITOR] >>>>> " << "Dying :( ... " << _name << ">>>>> " << std::endl;
}


std::string Prefetcher::blocks2String(std::vector<uint64_t> v) {
    std::ostringstream oss;

    if (!v.empty()){
        oss << "[";
        std::copy(v.begin(), v.end()-1, std::ostream_iterator<int>(oss, ","));
        oss << v.back() << "]";
    }

    return oss.str();
}
