# Dataflow Monitor (`flow-monitor`)

## Overview

The Dataflow Monitor is a dynamic library that can be preloaded into an application to trace its POSIX I/O calls (e.g., `open`, `read`, `write`). It generates detailed trace files in JSON format that capture file access patterns, I/O sizes, and performance metrics. This data is intended for post-mortem analysis by the `flow-analysis` toolkit to understand and optimize data workflows.

## Building

### Requirements
*   A C++14 compatible compiler (GCC is preferred).
*   CMake (version 2.8 or newer).

### Instructions
To build the `libmonitor.so` shared library, run the following commands from the `flow-monitor` directory:

```sh
mkdir build && cd build
cmake ..
make
```
This will create the shared library at `build/src/libmonitor.so`.

## Usage

The monitor is enabled by preloading the `libmonitor.so` library when launching an application.

```sh
LD_PRELOAD=/path/to/libmonitor.so your_application [app-args]
```

### Environment Variables

The monitor's behavior can be configured through the following environment variables:

*   #### `DATALIFE_OUTPUT_PATH`
    *   **Purpose**: Specifies the directory where the output trace files (in `.json` format) will be saved.
    *   **Default**: If not set, it defaults to an empty string (`""`), which means trace files will be created in the **current working directory** of the application being executed.
    *   **IMPORTANT**: The specified directory **must already exist**. The monitor will not create the directory, and file generation will fail if the path is invalid.

*   #### `DATALIFE_FILE_PATTERNS`
    *   **Purpose**: A comma-separated list of glob patterns that determine which files to trace.
    *   **Default**: If not set, the default pattern is `*.datalifetest`.
    *   **Example**: To trace all `.dat` and `.log` files, you would set:
        ```sh
        export DATALIFE_FILE_PATTERNS="*.dat,*.log"
        ```

## Minimal Example

After building the library, you can run the following commands to trace the I/O of the `dd` command on a test file.

```sh
# 1. Create an output directory and a test file.
mkdir -p /tmp/datalife_traces
dd if=/dev/zero of=my_test_file.datalifetest bs=1M count=10

# 2. Set environment variables and run a command with the monitor preloaded.
export DATALIFE_OUTPUT_PATH=/tmp/datalife_traces
export DATALIFE_FILE_PATTERNS="*.datalifetest"
LD_PRELOAD=./build/src/libmonitor.so dd if=my_test_file.datalifetest of=/dev/null bs=1M

# 3. Verify the output trace file.
ls -l /tmp/datalife_traces

# 4. Clean up the test file.
rm my_test_file.datalifetest
```
After running this, you should see a `.json` trace file in the `/tmp/datalife_traces` directory corresponding to the `dd` command's read operation.
