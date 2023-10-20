<!-- -*-Mode: markdown;-*- -->
<!-- $Id: 4098d4ffce45696ec3497ad9e08e712906c9d8fe $ -->


Prerequisites
=============================================================================

Environment
  - C++11 compiler, GCC preferred
  - CMake (>= version 2.6)
  - Python 3.7+



Building & Installing
=============================================================================

....


1. Build the datalife (analysis and monitor) within directory _build_:
   ```sh
   mkdir <build> && cd <build>
   cmake \
     -DCMAKE_INSTALL_PREFIX=<install> \
     <path-to-datalife-root>
   make install
   ```

2. Or individual Build, first monitor:
   ```sh
   cd flow-monitor
   mkdir <build> && cd <build>
   cmake \
     -DCMAKE_INSTALL_PREFIX=<install> \
     <path-to-monitor-root>
   make install
   ```

  To supply a compiler path, use:
  - `-DCMAKE_CXX_COMPILER=...`
  

3. Or individual Build, flow-analysis:
   ```sh
   cd flow-analysis
   mkdir <build> && cd <build>
   cmake \
    -DCMAKE_INSTALL_PREFIX=<install> \
    <path-to-monitor-root>
   ```


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

