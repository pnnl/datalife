#!/bin/bash


DATALIFE_LIB_PATH=../../build/flow-monitor/src/libmonitor.so
# python: /qfs/projects/ops/rh7_gpu/python/miniconda3.9/bin/../lib/libstdc++.so.6: version `GLIBCXX_3.4.29' not found (required by /qfs/people/tang584/install/datalife/lib/libmonitor.so)
# python: /qfs/projects/ops/rh7_gpu/python/miniconda3.9/bin/../lib/libstdc++.so.6: version `CXXABI_1.3.13' not found (required by /qfs/people/tang584/install/datalife/lib/libmonitor.so)


# Cleanup previous logs
rm -rf *_stat
rm -rf datalife_test.log

# Run simple test
# Measure time in milliseconds
sudo /sbin/sysctl vm.drop_caches=3
t3=$(date +%s%3N)
./io_test 2>&1 | tee -a datalife_test.log
t4=$(date +%s%3N)
echo "Time taken[program]: $((t4-t3)) milliseconds" | tee -a datalife_test.log

sudo /sbin/sysctl vm.drop_caches=3
t1=$(date +%s%3N)
LD_PRELOAD=$DATALIFE_LIB_PATH \
    ./io_test 2>&1 | tee -a datalife_test.log
t2=$(date +%s%3N)
echo "Time taken[datalife-run]: $((t2-t1)) milliseconds" | tee -a datalife_test.log


