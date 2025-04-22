<!-- -*-Mode: markdown;-*- -->
<!-- $Id$ -->


Prerequisites
=============================================================================

Environment
  - CMake (>= version 2.8)
  - C++14 compiler, GCC preferred
  - Python 3.7+



Building & Installing
=============================================================================

....

1. Common build options:

   - `CMAKE_INSTALL_PREFIX=<path>`: Install path

   - `-DENABLE_FlowMonitor=<ON|OFF>`: Enable build of FlowMonitor

   - `-DENABLE_FlowAnalysis=<ON|OFF>`: Enable build of FlowAnalysis
   
   - `-DCMAKE_CXX_COMPILER=<path>`: Path for C++ compiler

   - All options:
   ```sh
   cmake -LH <path-to-datalife-root>
   cmake -LA <path-to-datalife-root>
   ```

2. Build the datalife (monitor and analysis) within directory _build_:
   ```sh
   mkdir <build> && cd <build>
   cmake \
     -DCMAKE_INSTALL_PREFIX=<install-path> \
     <datalife-root-path>
   make install
   ```

   Individual packages can also be built, e.g.:
   ```sh
   cd flow-monitor
   mkdir <build> && cd <build>
   cmake ...
   make install
   ```

   <!-- mkdir BUILD && cd BUILD && cmake -DCMAKE_INSTALL_PREFIX=`pwd`/../INSTALL -DENABLE_FlowMonitor=OFF ..   -->

Using
=============================================================================

DataLife has two main steps.

1. Monitor...
   ```sh
    # Give your path to collect all datalife statistic files
    export DATALIFE_OUTPUT_PATH="./datalife_stats"
    # Give your list of target capture file regular expression patterns
    export DATALIFE_FILE_PATTERNS="*.gz, *.tar.gz, *.dcd, ior*.bin" 
    # Run your program with datalife with LD_PRELOAD
    LD_PRELOAD=/your_datalife_path/build/flow-monitor/src/libmonitor.so ./your_program
    # or Run your program with datalife-run
    export PATH="/your_datalife_path/build/bin:$PATH"
    datalife-run ./your_program
    ```

2. Analysis and diagnostics:

   ```sh
    datalife-analyze
    usage: datalife-analyze [-h] [-i INPUT] [-o OUTPUT]

    datalife-analyze produces data flow lifecycle (DFL) graph to guide
    decisions regarding coordinating tasks and data flows on distributed
    resources.

    optional arguments:
      -h, --help            show this help message and exit
      -i INPUT, --input INPUT
                            read I/O monitor stats from directory path
      -o OUTPUT, --output OUTPUT
                            write a graph output to a file
    ```

