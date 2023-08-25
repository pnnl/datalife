#include "Cache.h"
#include "Request.h"
#include "Loggable.h"
#include "Timer.h"

std::atomic<uint64_t> Request::ID_CNT(0);
std::atomic<uint64_t> Request::RET_ID_CNT(0);

RequestTrace Request::trace( std::string tag){
    return trace(true,tag);
}
RequestTrace Request::trace(bool trigger, std::string tag){
    if (trigger && globalTrigger){
        return RequestTrace(&ss,true) << "[" << id << " " << Timer::getCurrentTime()<<"] "<<tag<<" --";
    }
    else{
        return RequestTrace(&ss,false);
    }    
}

void Request::flushTrace(){
    ss << std::endl;
    std::cout << ss.str();
}

Request::~Request(){
    Request::RET_ID_CNT.fetch_add(1);
    // Loggable::debug()<<"request-- deleting req: "<<id <<"("<<Request::ID_CNT.load()<<", "<<Request::RET_ID_CNT.load()<<")"<<std::endl;
    originating->cleanUpBlockData(data);
    if (printTrace){
        Loggable::log()<<str();
    }
}

std::string Request::str(){
    std::stringstream tss;
    tss<<"Req ["<<id<<"]"<<std::endl
    <<"orignal cache: "<<originating->name()<<std::endl
    <<"blk: " <<blkIndex<<std::endl
    <<"file: "<<fileIndex<<std::endl
    <<"size: "<<size<<std::endl
    <<"time: "<<(double)time/ 1000000000.0<<std::endl
    <<"retry time: "<<(double)retryTime/ 1000000000.0<<std::endl
    <<"waitingCache: "<<waitingCache <<std::endl
    <<"Reserved Map: [";
    for (auto vals : reservedMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<" index Map: [";
    for (auto vals : indexMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<"blkindex Map: [";
    for (auto vals : blkIndexMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<" fileindex Map: [";
    for (auto vals : fileIndexMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<" status Map: [";
    for (auto vals : statusMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl;
    tss<<ss.str()<<std::endl;;
    return tss.str();

}