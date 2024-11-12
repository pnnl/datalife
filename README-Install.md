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

