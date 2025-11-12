#!/bin/bash


# Usage: `LD_PRELOAD=path-to-lib.so <application> <app-args>`


DATALIFE_LIB_PATH=/qfs/people/tang584/install/datalife/lib/libmonitor.so
# python: /qfs/projects/ops/rh7_gpu/python/miniconda3.9/bin/../lib/libstdc++.so.6: version `GLIBCXX_3.4.29' not found (required by /qfs/people/tang584/install/datalife/lib/libmonitor.so)
# python: /qfs/projects/ops/rh7_gpu/python/miniconda3.9/bin/../lib/libstdc++.so.6: version `CXXABI_1.3.13' not found (required by /qfs/people/tang584/install/datalife/lib/libmonitor.so)


# Cleanup previous logs
rm -rf *blk_trace.json *.datalife.json

# # datalife monitor env variables
# export MONITOR_SOCKETS_PER_CONN=0
# export MONITOR_ENABLE_SHARED_MEMORY=0
# export MONITOR_SCALABLE_CACHE=0
# export MONITOR_NETWORK_CACHE=0
# export MONITOR_PREFETCH_NUM_BLKS=0
# export MONITOR_PREFETCH_DELTA=0
# export MONITOR_URL_TIMEOUT=0
# export MONITOR_DELETE_DOWNLOADS=0
# export MONITOR_DOWNLOAD_FOR_SIZE=0

# export MONITOR_PRINT_STATS=1


# Run datalife simple test
# Measure time in milliseconds

sudo /sbin/sysctl vm.drop_caches=3
t1=$(date +%s%3N)
LD_PRELOAD=$DATALIFE_LIB_PATH \
    python io_test.py #| tee tazer_test.log
# datalife-run python io_test.py 2>&1 | tee tazer_test.log
t2=$(date +%s%3N)
echo "Time taken[datalife-run]: $((t2-t1)) milliseconds"

rm -rf ./test_io_file.txt
# mv ///scratch/$USER/*blk_trace ./
sleep 5

# Run simple test
# Measure time in milliseconds
sudo /sbin/sysctl vm.drop_caches=3
t3=$(date +%s%3N)
python io_test.py #| tee -a tazer_test.log
t4=$(date +%s%3N)
echo "Time taken[program]: $((t4-t3)) milliseconds"