#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 /absolute/path/to/libmonitor.so" >&2
    exit 2
fi

monitor_lib=$(realpath "$1")
if [[ ! -f "$monitor_lib" ]]; then
    echo "monitor library does not exist: $monitor_lib" >&2
    exit 2
fi

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-copy-close.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# A file's traces are written when a descriptor of it is closed, a copy made
# with dup() included. A program that keeps a monitored file open and copies
# and closes its descriptor over and over (a shell with its output in the file,
# around every redirection) closes it many times with nothing read or written
# in between. Such a close has nothing new to write: it must not start the
# threads that write the JSON traces, nor open the files of the other output
# mode again. What is recorded later is still written at the close after it.
#
# A library preloaded after the monitor counts the threads started and the
# files opened under the output directory; the monitor's own reach it as they
# reach the C library. The program asks it for the counts.
cat >"$test_root/count_output.c" <<'EOF'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int threads_started;
static int output_opens;

void copy_close_counts(int *threads, int *opens) {
    *threads = __atomic_load_n(&threads_started, __ATOMIC_SEQ_CST);
    *opens = __atomic_load_n(&output_opens, __ATOMIC_SEQ_CST);
}

static void count_if_output(const char *path) {
    const char *directory = getenv("COUNT_DIRECTORY");
    if (path && directory && strncmp(path, directory, strlen(directory)) == 0) {
        __atomic_add_fetch(&output_opens, 1, __ATOMIC_SEQ_CST);
    }
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attributes, void *(*start)(void *), void *argument) {
    static int (*real)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    if (!real) real = (int (*)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *))dlsym(RTLD_NEXT, "pthread_create");
    __atomic_add_fetch(&threads_started, 1, __ATOMIC_SEQ_CST);
    return real(thread, attributes, start, argument);
}

FILE *fopen(const char *path, const char *mode) {
    static FILE *(*real)(const char *, const char *);
    if (!real) real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen");
    count_if_output(path);
    return real(path, mode);
}

FILE *fopen64(const char *path, const char *mode) {
    static FILE *(*real)(const char *, const char *);
    if (!real) real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen64");
    count_if_output(path);
    return real(path, mode);
}

int open(const char *path, int flags, ...) {
    static int (*real)(const char *, int, ...);
    if (!real) real = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    va_list arguments;
    va_start(arguments, flags);
    int mode = va_arg(arguments, int);
    va_end(arguments);
    count_if_output(path);
    return real(path, flags, mode);
}

int open64(const char *path, int flags, ...) {
    static int (*real)(const char *, int, ...);
    if (!real) real = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open64");
    va_list arguments;
    va_start(arguments, flags);
    int mode = va_arg(arguments, int);
    va_end(arguments);
    count_if_output(path);
    return real(path, flags, mode);
}
EOF

# Block 0 is written and the descriptor it was written through closed, which
# writes the traces. The copy that is left is then copied and closed 200 times
# untouched, and the program notes what that cost. Block 1 is written through
# the copy last, and the copy closed.
cat >"$test_root/copy_close.c" <<'EOF'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    static char block[4096];
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    void (*counts)(int *, int *) = (void (*)(int *, int *))dlsym(RTLD_DEFAULT, "copy_close_counts");
    if (!counts) {
        return 2;
    }
    memset(block, 'x', sizeof block);
    int fd = open("data-kept", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || write(fd, block, sizeof block) != (ssize_t)sizeof block) {
        return 1;
    }
    int kept = dup(fd);
    if (kept < 0 || close(fd) != 0) {
        return 1;
    }

    int threads_before, opens_before, threads_after, opens_after;
    counts(&threads_before, &opens_before);
    for (int i = 0; i < 200; i++) {
        int copy = dup(kept);
        if (copy < 0 || close(copy) != 0) {
            return 1;
        }
    }
    counts(&threads_after, &opens_after);

    if (write(kept, block, sizeof block) != (ssize_t)sizeof block || close(kept) != 0) {
        return 1;
    }
    FILE *cost = fopen("cost", "w");
    if (!cost || fprintf(cost, "%d %d\n", threads_after - threads_before, opens_after - opens_before) < 0 || fclose(cost) != 0) {
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -shared -fPIC "$test_root/count_output.c" -o "$test_root/libcount_output.so" -ldl -pthread
cc -Wall -Wextra -Werror "$test_root/copy_close.c" -o "$test_root/copy_close" -ldl

for mode in 1 0; do
    traces="$test_root/traces-$mode"
    work="$test_root/work-$mode"
    mkdir -p "$traces" "$work"
    status=0
    timeout 60 env \
        LD_PRELOAD="$monitor_lib $test_root/libcount_output.so" \
        COUNT_DIRECTORY="$traces" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/copy_close" "$work" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "the program failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    cost=$(cat "$work/cost")
    if [[ $cost != "0 0" ]]; then
        echo "200 closes of an untouched copy of a monitored descriptor started ${cost% *} threads and opened ${cost#* } output files: the traces were written again with nothing new in them (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi

    # Both blocks are in what was written at the last close.
    if [[ $mode -eq 1 ]]; then
        trace=$(compgen -G "$traces/data-kept.*.w_blk_trace.json" | head -n 1 || true)
        if [[ -z $trace ]]; then
            echo "no write trace was written for data-kept" >&2
            exit 1
        fi
        if command -v python3 >/dev/null 2>&1; then
            got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"], d["file_size_bytes"])' "$trace")
            if [[ $got != "2 8192 8192" ]]; then
                echo "access_frequency, data_volume and file_size_bytes of data-kept: '$got', expected '2 8192 8192'" >&2
                exit 1
            fi
        fi
    else
        stat=$(compgen -G "$traces/data-kept_*_w_stat" | head -n 1 || true)
        got=$(tail -n +2 "${stat:-/dev/null}" | tr '\n' ';')
        if [[ $got != "0 1 4096;1 1 4096;" ]]; then
            echo "w_stat rows of data-kept: '$got', expected '0 1 4096;1 1 4096;'" >&2
            exit 1
        fi
        sequence=$(compgen -G "$traces/data-kept_*_w_trace_stat" | head -n 1 || true)
        got=$(tr '\n' ';' <"${sequence:-/dev/null}")
        if [[ $got != "0;1;" ]]; then
            echo "w_trace_stat of data-kept: '$got', expected '0;1;'" >&2
            exit 1
        fi
    fi
done

echo "closing a copy of a monitored descriptor with nothing new recorded does not write its traces again"