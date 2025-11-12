# My Test Folder

This folder contains various scripts and logs for testing the DataLife project.

## I/O Test Scripts

This section describes the scripts and files used for I/O testing.

*   `io_test.py`: A Python script for testing basic I/O performance by writing and reading a file. This is the simplest working test.
*   `vlen_h5_read2.py`: A Python script that reads variable-length datasets from an HDF5 file. This script is implemented for observing different types of I/O behavior.
*   `vlen_h5_write.py`: A Python script that writes variable-length datasets to an HDF5 file. This script is implemented for observing different types of I/O behavior.
*   `test_datalife_param.sh`: A shell script to run the `io_test.py` script with the DataLife monitoring library (`libmonitor.so`) and measure its performance.
*   `test_hdf5.sh`: A shell script to test HDF5 file I/O with the DataLife monitor.
*   `*.json`: These are the expected output files from the DataLife monitor. The naming format is `<filename>.<pid>.<type>.json`. For example, `test_io_file.txt.14100.r_blk_trace.json` is a read block trace for the file `test_io_file.txt` with process ID 14100. The `monitor_timer.*.datalife.json` file contains timing statistics for various I/O operations, while the `*_blk_trace.json` files contain traces of read and write block ranges.

## DataLife Analysis

*   `datalife_analysis.py`: A Python script to process and analyze statistics from DataLife log files. It provides a simple printout of the processed statistics.

## WfCommons Test

This section describes the files used for testing with `wfcommons`, a workflow generation tool.

*   `wfcommonstest/`: This directory contains files related to testing with `wfcommons`.
    *   `seism_workflow.py`: A Python script for generating a seismology workflow using `wfcommons`.
    *   `seismology-workflow.json`: A JSON file describing a seismology workflow.
    *   `seismology-workflow.json.txt`: A copy of `seismology-workflow.json`.
    *   `test_datalife_param.sh`: A shell script for running `seism_workflow.py` with the DataLife monitor.

## Others

This section contains other files and logs.

*   `seismic_error_log/`: A directory containing old log files from previous seismic workflow tests.
*   `tazer_test.log`: An old log file from a previous test run.