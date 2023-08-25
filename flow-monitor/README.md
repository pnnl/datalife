Dataflow Monitor
================

Building
=============================================================================

Requirements:
  1. C++11 compiler, GCC preferred
  2. CMake (>= version 2.6)

To build within directory _build_:
  ```sh
  mkdir <build> && cd <build>
  cmake \
    -DCMAKE_INSTALL_PREFIX=<install> .. \
  make install
  ```

To supply a compiler path, use:
- `-DCMAKE_CXX_COMPILER=...`

Use instructions
=============================================================================

Usage: `LD_PRELOAD=path-to-lib.so <application> <app-args>`

