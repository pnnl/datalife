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
    *   **Matching**: A pattern is matched against the base name of a file (the part after the last `/`), not its path. A file that a tool writes under another name is traced only if a pattern matches that name too. `fasterq-dump`, for example, writes its output to temporary chunks named `<accession>.<host>.<pid>.<n>.<m>` before it assembles the FASTQ file, so `*.fastq` alone misses them and a pattern such as `SRR*` is needed as well. The same goes for a file made with `mkstemp`: one from the template `result.XXXXXX` is traced under the name it gets if a pattern such as `result.*` matches it.
    *   **Renamed files**: When the process that reads or writes a traced file renames it (`rename`, `renameat`, `renameat2`), what was recorded under the old name is reported under the new one, added to anything recorded there, and the trace files already written for the old name are replaced by ones for the new name. A temporary file that becomes the result is therefore reported as the result. Each such rename is listed under `renames` in the process's `monitor_timer.*.json` (or on a `[MONITOR] rename` line when the library is built without `TIMER_JSON`), with `"followed": true`. A rename the monitor cannot follow is listed with `"followed": false` when either name matches a pattern: a file written by one process and renamed by another (`mv`, for example), or a file renamed onto one that is still open. Names are compared as the program spells them, and a renamed directory is not followed.

*   #### `DATALIFE_JSON_OUTPUT`
    *   **Purpose**: Enables JSON-only output mode when set to `1`. When enabled, the monitor will generate trace data in JSON format without creating additional stat files.
    *   **Default**: If not set, defaults to `0` (disabled).
    *   **Example**: To enable JSON output mode:
        ```sh
        export DATALIFE_JSON_OUTPUT=1
        ```

## What is recorded

The monitor records I/O on a traced file when the program makes it through one of the calls below. The scripts in `tests/` check them; `tests/test_entry_points.sh` moves a megabyte through each of the calls other than the plain `read`, `write`, `fread` and `fwrite` ones, and `tests/test_libc_entry_points.sh` runs one program through all of them, built plain, fortified and with `_FILE_OFFSET_BITS=64`.

| | Calls |
|---|---|
| Opening | `open`, `open64`, `openat`, `openat64`, `creat`, `creat64`, `mkstemp`, `mkostemp`, `mkstemps`, `mkostemps`, `fopen`, `fopen64`, `fdopen`, `freopen`, `freopen64`, the 64-bit names of these, and the names they have in programs built with `_FORTIFY_SOURCE` (`__open_2`, ...) |
| Descriptors | `read`, `write`, `pread`, `pwrite`, `readv`, `writev`, `preadv`, `pwritev`, `preadv2`, `pwritev2`, `lseek`, `sendfile`, `copy_file_range`, `fsync`, `close`, and the 64-bit and fortified names of these; `mmap` and `mmap64` (a mapping counts as one read or write of its length). What is written through a descriptor that appends (`O_APPEND`, or `pwritev2` with `RWF_APPEND`) is recorded at the end of the file, where it goes. |
| Copies of descriptors | `dup`, `dup2`, `dup3`, `fcntl(F_DUPFD)`, `fcntl(F_DUPFD_CLOEXEC)`: the copy is traced like the descriptor |
| Streams | `fread`, `fwrite`, `fgets`, `fputs`, `fgetc`, `fputc`, `getc`, `putc`, `getline`, `getdelim`, `fscanf`, `vfscanf`, `fprintf`, `vfprintf`, `fseek`, `fseeko`, `fsetpos`, `rewind`, `fflush`, `fclose`, their 64-bit, `_unlocked` and fortified names, and the macros `getc_unlocked` and `putc_unlocked` |
| Standard streams | `printf`, `vprintf`, `puts`, `putchar`, `scanf`, `vscanf`, `getchar`, once `freopen` or `dup2` has put `stdout` or `stdin` on a traced file. What a program that does not close them left in their buffers is counted when the library is unloaded. |
| Names | `rename`, `renameat`, `renameat2` (see **Renamed files** above) |

A stream is traced when its descriptor is, however it came by it: `fopen`, `fdopen`, `freopen`, or `dup2` onto the descriptor of a stream that is already open.

In the timer output, the bytes of a traced file are under `read` and `write` of `monitor`, and under `readv` and `writev` for the calls that take vectors of buffers.

Not recorded:

*   I/O through a descriptor the program did not open itself: a file the shell put on `stdin` or `stdout` (`program < input > output`), or one inherited from the process that started it.
*   Files from `tmpfile`, which have no name.
*   `dprintf` and `vdprintf`, the wide-character calls (`fgetwc`, `fputws`, ...), `getw` and `putw`, and `fflush(NULL)`.
*   `splice`, `tee`, the `aio_*` calls and `io_uring`.
*   What a program reads or writes in a mapping after `mmap` (only the mapping is counted), and system calls a program makes without the C library, as statically linked programs do.

Limits of what is recorded:

*   The trace files of a file are written when the file is closed with `close`, `fclose`, or `dup2`, `dup3` or `freopen` on its descriptor or stream. A file that is still open when the program ends is counted in the timer output only.
*   A descriptor closed another way (`close_range`, `closefrom`, `fcloseall`, `closedir` after `fdopendir`) is not seen to close. Its file gets no trace files then, and if the number goes to something that is not traced (another file, a pipe, a socket), I/O on that is counted for the file until the number is closed with one of the calls above. A traced file that gets the number takes it over.
*   While one thread puts another file on a descriptor with `dup2` or `dup3`, a call another thread is making through that descriptor may be counted for the file that was there. While one thread calls `freopen` on a stream, a call another thread begins on the stream at that moment may go unrecorded.
*   What a stream had read ahead into its buffer before `dup2` put a traced file on its descriptor is not counted when the program takes it.
*   A descriptor and its copies (`dup`, `dup2`, ...) are one open file with one offset, and are recorded at one position; a stream on each (a log, and `stdout` and `stderr` put on it with `dup2`) is recorded with its own buffer. Two streams on the same descriptor (`fdopen` twice on one number) share one record of what is in their buffers, and I/O through both is counted more than once.
*   An `_unlocked` call (`fwrite_unlocked`, `putc_unlocked`, ...) on a stream that another thread holds with `flockfile` at that moment is made without waiting, as the C library makes it, and is not recorded as a call: what it leaves in the stream's buffer is recorded with the next call on the stream, and what it reads or flushes is not.
*   A child process made with `clone` or `_Fork`, without the handlers `fork` runs, is traced as a process of its own where the kernel offers `kcmp`. Elsewhere the library takes it for a `vfork` child, and records nothing it opens or closes.
*   The wrappers are not safe to call from a signal handler in the way `open` and `close` themselves are: they allocate memory. A handler that only calls `read` or `write` on descriptors that are not traced, as handlers that wake a main loop through a pipe do, is safe.

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