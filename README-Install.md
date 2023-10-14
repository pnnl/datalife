<!-- -*-Mode: markdown;-*- -->
<!-- $Id: 4098d4ffce45696ec3497ad9e08e712906c9d8fe $ -->


Prerequisites
=============================================================================

Environment
  - C++11 compiler, GCC preferred
  - CMake (>= version 2.6)
  - Python



Building & Installing
=============================================================================

....


1. Build the flow monitor
   ```sh
   cd <memgaze>/flow-monitor
   make ...
   make perf # optional
   ```

  To build within directory _build_:
  ```sh
  mkdir <build> && cd <build>
  cmake \
    -DCMAKE_INSTALL_PREFIX=<install> \
    <path-to-tazer-root>
  make install
  ```

  To supply a compiler path, use:
  - `-DCMAKE_CXX_COMPILER=...`
  

2. Build flow-analysis...
   ```sh
   ...
   ```


Using
=============================================================================

DataLife has two main steps.

1. Monitor...


2. Analysis and diagnostics...

