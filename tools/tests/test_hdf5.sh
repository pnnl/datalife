#!/bin/bash


# Usage: `LD_PRELOAD=path-to-lib.so <application> <app-args>`

export DATALIFE_OUTPUT_PATH="./datalife_stats"
DATALIFE_LIB_PATH=../../build/flow-monitor/src/libmonitor.so
#/qfs/people/tang584/install/datalife/lib/libmonitor.so
# python: /qfs/projects/ops/rh7_gpu/python/miniconda3.9/bin/../lib/libstdc++.so.6: version `GLIBCXX_3.4.29' not found (required by /qfs/people/tang584/install/datalife/lib/libmonitor.so)
# python: /qfs/projects/ops/rh7_gpu/python/miniconda3.9/bin/../lib/libstdc++.so.6: version `CXXABI_1.3.13' not found (required by /qfs/people/tang584/install/datalife/lib/libmonitor.so)


# Cleanup previous logs
rm -rf $DATALIFE_OUTPUT_PATH/*_stat

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

exp_ext=$(echo "$(pwd)" | rev | cut -d'/' -f1-5 | rev)
export EXPERIMENT_PATH="/qfs/projects/oddite/$USER/$exp_ext"
export HDF5_USE_FILE_LOCKING=FALSE
TEST_LOG=datalife_hdf5_test.log
filename="test_IO_file.h5"
rm -rf $TEST_LOG

sudo /sbin/sysctl vm.drop_caches=3
t1=$(date +%s%3N)
LD_PRELOAD=$DATALIFE_LIB_PATH \
    python vlen_h5_write.py $filename 2>&1 | tee -a $TEST_LOG

LD_PRELOAD=$DATALIFE_LIB_PATH \
    python vlen_h5_read2.py $filename 2>&1 | tee -a $TEST_LOG
# datalife-run python io_test.py 2>&1 | tee datalife_hdf5_test.log
t2=$(date +%s%3N)
echo "Time taken[datalife-run]: $((t2-t1)) milliseconds" | tee -a $TEST_LOG

# rm -rf ./$filename
# mv ///scratch/$USER/*blk_trace ./
# sleep 5

# # Run simple test
# # Measure time in milliseconds
# sudo /sbin/sysctl vm.drop_caches=3
# t3=$(date +%s%3N)
# python vlen_h5_write.py $filename | tee -a datalife_hdf5_test.log
# python vlen_h5_read2.py $filename | tee -a datalife_hdf5_test.log
# t4=$(date +%s%3N)
# echo "Time taken[program]: $((t4-t3)) milliseconds"

# Use LTrace to investigate the operations