#!/bin/bash


TRACKER_SRC_DIR=../build/src
VOL_NAME="tracker"

export HDF5_USE_FILE_LOCKING='FALSE' # TRUE FALSE BESTEFFORT


export WORKFLOW_NAME="h5_write_read"
export PATH_FOR_TASK_FILES="/tmp/$USER/$WORKFLOW_NAME"
mkdir -p $PATH_FOR_TASK_FILES
> $PATH_FOR_TASK_FILES/${WORKFLOW_NAME}_vfd.curr_task # clear the file
> $PATH_FOR_TASK_FILES/${WORKFLOW_NAME}_vol.curr_task # clear the file

IO_FILE="$(pwd)/sample.h5"
rm -rf $IO_FILE $(pwd)/*.h5

SIMPLE_VOL_IO (){
    schema_file=data-stat-vol.yaml
    rm -rf ./*$schema_file
    
        HDF5_VOL_CONNECTOR="${VOL_NAME} under_vol=0;under_info={};path=${schema_file};level=2;format=" \
        HDF5_PLUGIN_PATH=$TRACKER_SRC_DIR/vol:$HDF5_PLUGIN_PATH \
        python3 example_test.py $IO_FILE # h5_write_read.py example_test.py
}

SIMPLE_VFD_IO (){
    schema_file=data-stat-dl.yaml
    rm -rf ./*vfd-${schema_file}*
    TRACKER_VFD_PAGE_SIZE=65536

    echo "TRACKER_VFD_DIR = $TRACKER_SRC_DIR/vfd"
    
    # HDF5_VOL_CONNECTOR="under_vol=0;under_info={};path=${schema_file}" \

    # Only VFD
    set -x
    # LD_LIBRARY_PATH=$TRACKER_SRC_DIR/vfd:$LD_LIBRARY_PATH \
    # export CURR_TASK="example_test"

    HDF5_DRIVER_CONFIG="true ${TRACKER_VFD_PAGE_SIZE}" \
        HDF5_DRIVER=hdf5_tracker_vfd \
        HDF5_LOG_FILE_PATH="${schema_file}" \
        HDF5_PLUGIN_PATH=$TRACKER_SRC_DIR/vfd \
        python h5_write_read.py $IO_FILE
}

# get execution time in ms
start_time=$(date +%s%3N)
SIMPLE_VFD_IO
# SIMPLE_VOL_IO
end_time=$(date +%s%3N)
echo "Execution time: $((end_time-start_time)) ms"