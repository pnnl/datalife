//JS: This implementation was taken from https://www.jmlr.org/papers/volume11/ben-haim10a/ben-haim10a.pdf

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <iostream>
#include <vector>
#include <algorithm>
#include "ReaderWriterLock.h"
#include <string>
#include "UnixIO.h"
#include <fcntl.h>
#include <sstream>

#define PRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

class Histogram {
    private:
        ReaderWriterLock lock;
        bool _search;
        bool _absMin;
        double _min;
        double _max;
        unsigned int _maxBuckets;
        std::vector<std::tuple<double, double>> _bins;

        //BM: for extrapolation trace
        int numGetVals;
        int numExtrapolation;
        std::string extrapolationInfo;
        bool _trace;

        //JS: Must find exact match
        int findBinMatch(double key) {
            //JS: Binary search
            if(_search) {
                int left = 0;
                int right = _bins.size() - 1;
                while(left <= right) {
                    int middle = (left + right) / 2;
                    auto middleValue = std::get<0>(_bins[middle]);
                    if(middleValue < key)
                        left = middle + 1;
                    else if(middleValue > key)
                        right = middle - 1;
                    else 
                        return middle;
                }
            }
            else {
                for(size_t i = 0; i < _bins.size(); ++i) {
                    auto tempKey = std::get<0>(_bins[i]);
                    if(key == tempKey)
                        return i;
                }
            }
            return -1;
        }

        //JS: Finds the lower of the two bins where bin[i].key <= key < bin[i+1].key
        int findBin(double key) {
            //JS: Modified binary search
            if(_search) {
                int left = 0;
                int right = _bins.size() - 1;
                int middle = right;
                
                while(left <= right) {
                    middle = (left + right) / 2;
                    //JS: If this breaks, it means we only have two bins left
                    if(middle == left)
                        break;

                    auto middleValue = std::get<0>(_bins[middle]);
                    //JS: Only update to the middle... We don't want to pass it
                    if(middleValue < key)
                        left = middle;
                    else if(middleValue > key)
                        right = middle;
                    else
                        break;
                }
                return middle;
            }
            else {
                for(size_t i = 1; i < _bins.size(); ++i) {
                    auto upper = std::get<0>(_bins[i]);
                    auto lower = std::get<0>(_bins[i-1]);
                    if(lower <= key && key < upper) {
                        return i - 1;
                    }
                }
            }
            return -1;
        }

        void add(double key, double value) {
            //JS: Update min/max for non-strict
            if(_absMin) {
                if(key < _min)
                    _min = key;
                if(key > _max)
                    _max = key;
            }

            //JS: Check if there exists an exact bin
            auto binIndex = findBinMatch(key);
            if(binIndex >= 0) {
                auto newValue = std::get<1>(_bins[binIndex]) + value;
                _bins[binIndex] = std::make_tuple(key, newValue);
            }
            else {
                //JS: Add new bin
                _bins.push_back(std::tuple<double, double>{key, value});
                std::sort(_bins.begin(), _bins.end(), 
                    [](const std::tuple<double, double> &s1, const std::tuple<double, double> &s2) -> bool { 
                        return std::get<0>(s1) < std::get<0>(s2); 
                    });

                //JS: Do resize if bins are more than allowed
                if(_bins.size() > _maxBuckets) {               
                    //JS: Find the closest two bins
                    double minDiff = std::numeric_limits<double>::max();
                    uint64_t minDiffIndex = (uint64_t) -1;
                    for(size_t i = 1; i < _bins.size(); ++i) {
                        auto diff = std::get<0>(_bins[i]) - std::get<0>(_bins[i-1]);
                        if(diff <= minDiff) {
                            minDiff = diff;
                            minDiffIndex = i;
                        }
                    }

                    //JS: Average the bins
                    double qi = std::get<0>(_bins[minDiffIndex-1]);
                    double ki = std::get<1>(_bins[minDiffIndex-1]);
                    double qi_1 = std::get<0>(_bins[minDiffIndex]);
                    double ki_1 = std::get<1>(_bins[minDiffIndex]);
                    double newKey =  (qi * ki + qi_1 * ki_1) / (ki + ki_1);
                    double newValue = ki + ki_1;

                    //JS: Insert new bin and remove old
                    _bins[minDiffIndex] = std::make_tuple(newKey, newValue);
                    _bins.erase(_bins.begin() + minDiffIndex - 1);
                }

                //JS: Update min/max if we are being strict
                if(!_absMin) {
                    _min = std::get<0>(_bins.front());
                    _max = std::get<0>(_bins.back());
                }
            }
        }

        double sum(double key) {
            double s = 0;
            if(key < _min) {
                return s;
            }
            
            if(key >= _max) {
                for(size_t i = 0; i < _bins.size(); ++i)
                    s+=std::get<1>(_bins[i]);
                return s;
            }

            //JS: The paper only calcs bins within the bin keys
            //To go below we could guess half of bin zero based
            //on paper assumption.
            if(key < std::get<0>(_bins.front())) {
                return std::get<1>(_bins.front()) / 2;
            }

            if(key > std::get<0>(_bins.back())) {
                for(size_t i = 0; i < _bins.size() - 1; ++i)
                    s+=std::get<1>(_bins[i]);
                s+=std::get<1>(_bins.back()) / 2;
                return s;
            }

            auto binIndex = findBin(key);

            auto itup = _bins[binIndex];
            auto itup_1 = _bins[binIndex+1];
            
            double pi = std::get<0>(itup);
            double mi = std::get<1>(itup);
            double pi_1 = std::get<0>(itup_1);
            double mi_1 = std::get<1>(itup_1);

            //JS: This calculates the area of an imaginary trapizaoid
            double mb = mi + ( ( (mi_1 - mi) / (pi_1 - pi) ) * (key - pi) );
            s = ( (mi + mb) / 2 ) * ( (key - pi) / (pi_1 - pi) );

            //JS: Add up all the bins below us
            for(int i = 0; i < binIndex; ++i)
                s += std::get<1>(_bins[i]);

            //JS: Paper assumes that half the bin is above and below this number
            s += mi / 2;
            return s;
        }

    public:

        Histogram(unsigned int maxBuckets, bool trace=false, bool absMin=false, bool search=true) :
            _search(search),
            _absMin(absMin),
            _min(std::numeric_limits<double>::max()), 
            _max(std::numeric_limits<double>::min()),
            _maxBuckets(maxBuckets),
            numExtrapolation(0),
            extrapolationInfo(""),
            numGetVals(0),
            _trace(trace) { 
               // std::cerr<<"Trace?:"<<_trace<<std::endl;
            }

        void addData(double key, double value) {
            lock.writerLock();
            add(key, value);
            lock.writerUnlock();
        }

        double getValue(double key, double range, bool doLock) {
            double ret = 0;
            if(doLock)
                lock.readerLock();
            if(_trace){
                numGetVals++;
                 if(key > _max){    
                        numExtrapolation++;
                        std::string tempLine = "HighExtrapolation Max:" + std::to_string(_max) + " distance(%):" + std::to_string((key-_max)/_max*100) + " value:"+ std::to_string(key) + "\n";
                        extrapolationInfo += tempLine;
                }
                else if(key < _min){
                        numExtrapolation++;
                        std::string tempLine = "LowExtrapolation Min:" + std::to_string(_min) + " distance(%):" + std::to_string((_min-key)/_min*100) + " value:"+ std::to_string(key) + "\n";
                        extrapolationInfo += tempLine; 
                }
            }
            
            if(_bins.size() > 1){
                if(key > _max){
                    ret = sum(_max) - sum(_max-range);
                }
                else if(key < _min){
                    ret = sum(_min+range) - sum(_min);
                }
                else{
                    ret = sum(key+range) - sum(key-range);
                }
            }
            else {
                //BM: temp fix, if we only have one bin, we return sum of that bin. Else we go through the if /else to calculate the sum needed
                ret = sum(_max);
            }
            
            if(doLock)
                lock.readerUnlock();
            return ret;
        }

        double getValue(double key) {
            double ret = 0;
            lock.readerLock();
            //JS: range = (max + min)/(buckets - 1)
            // This is the hueristic that was suggested by Nathan.
            // It could be interesting to play with this and see how
            // it effects the quality of results
            if(_bins.size() > 1)
                ret = getValue(key, (_max + _min)/(_bins.size() - 1), false);
            else
                ret = getValue(key, 0.5, false);
                
            lock.readerUnlock();
            //printBins();
            return ret;
        }

        void printBins() {
            lock.readerLock();
            for(size_t i = 0; i < _bins.size(); ++i)
                PRINTF("histo i: %u %lf %lf\n", i, std::get<0>(_bins[i]), std::get<1>(_bins[i]));
            lock.readerUnlock();
        }
        void printLog(int id){
            lock.readerLock();
            if(_trace){
            // unixopen_t unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
            // unixclose_t unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
            // unixwrite_t unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");
            // Use global POSIX function pointers to avoid recursive interception
            std::string fname = "histogram_stats_"+ std::to_string(id) +".txt";
            int fd = (*unixopen)(fname.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0660);
            if (fd != -1) {
                std::stringstream ss;
                ss << "number of histogram queries:" << numGetVals << std::endl;
                ss << "number of extrapolations:" << numExtrapolation << std::endl;
                ss << extrapolationInfo.c_str() << std::endl;
                // unixwrite(fd, ss.str().c_str(), ss.str().length());
                // unixclose(fd);
                (*unixwrite)(fd, ss.str().c_str(), ss.str().length());
                (*unixclose)(fd);
            }
            }
            lock.readerUnlock();
        }
};

#endif /* HISTOGRAM_H */
