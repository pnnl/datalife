<!-- -*-Mode: markdown;-*- -->
<!-- $Id: 4098d4ffce45696ec3497ad9e08e712906c9d8fe $ -->


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

   - `-DDO_MAKE_FlowMonitor=<ON|OFF>`: Enable build of FlowMonitor

   - `-DDO_MAKE_FlowAnalysis=<ON|OFF>`: Enable build of FlowAnalysis
   
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


Using
=============================================================================

DataLife has two main steps.

1. Monitor...


2. Analysis and diagnostics:

   First, make a copy of the analysis cmd to <build>/bin directory by:
   ```sh
     make flow-analysis-bin
   ```

   And execute the cmd:

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

