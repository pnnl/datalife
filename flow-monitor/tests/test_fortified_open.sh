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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-fortified-open.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Programs built with _FORTIFY_SOURCE (the default when optimizing on most
# distributions) call __open_2, __open64_2, __openat_2 and __openat64_2 for an
# open without a mode whose flags are not a compile-time constant. A file opened
# that way must be monitored like one opened with the plain calls: fasterq-dump
# in distribution builds of the SRA toolkit opens its output through __open_2
# to append the rest of the reads. openat64, what openat is for programs built with
# _FILE_OFFSET_BITS=64, is covered too.
cat >"$test_root/fortified_open.c" <<'EOF'
#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK 4096

static char buf[BLOCK];

/* Writes block number `block` of the file, at its offset, as fasterq-dump does. */
static int write_block(int fd, int block) {
    return fd >= 0 && pwrite(fd, buf, BLOCK, (off_t)block * BLOCK) == BLOCK && close(fd) == 0;
}

int main(int argc, char **argv) {
    if (argc != 3 || chdir(argv[1]) != 0) {
        return 2;
    }
    int extra = strcmp(argv[2], "creat") == 0 ? O_CREAT : 0; /* known only at run time; O_CREAT for the abort case */
    int wronly = O_WRONLY | extra;
    int rdonly = O_RDONLY | extra;
    memset(buf, 'd', sizeof buf);

    /* openat64 with a mode: the plain call. */
    if (!write_block(openat64(AT_FDCWD, "data-fortified-open", O_WRONLY | O_CREAT | O_TRUNC, 0644), 0)) return 1;
    /* The four fortified calls. */
    if (!write_block(open("data-fortified-open", wronly), 1)) return 1;
    if (!write_block(open64("data-fortified-open", wronly), 2)) return 1;
    if (!write_block(openat(AT_FDCWD, "data-fortified-open", wronly), 3)) return 1;
    if (!write_block(openat64(AT_FDCWD, "data-fortified-open", wronly), 4)) return 1;

    int fd = open("data-fortified-open", rdonly);
    for (int i = 0; i < 5; i++) {
        if (read(fd, buf, BLOCK) != BLOCK || buf[BLOCK - 1] != 'd') return 1;
    }
    if (read(fd, buf, BLOCK) != 0) return 1; /* 5 blocks and no more */
    if (close(fd) != 0) return 1;

    /* The library must not change what these opens do. A FIFO opened without
       blocking returns at once, though nothing writes to it. */
    if (mkfifo("fifo", 0600) != 0) return 1;
    fd = open("fifo", rdonly | O_NONBLOCK);
    if (fd < 0 || close(fd) != 0) {
        fprintf(stderr, "FAILED: non-blocking open of a FIFO\n");
        return 1;
    }
    /* A file that does not exist is not created by opening it for update. */
    errno = 0;
    fd = open("data-missing", O_RDWR | extra);
    if (fd != -1 || errno != ENOENT || access("data-missing", F_OK) == 0) {
        fprintf(stderr, "FAILED: open of a missing file for update returned %d (%s)\n", fd, strerror(errno));
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2 "$test_root/fortified_open.c" -o "$test_root/fortified_open"

# The program must really use the fortified variants, or the test proves nothing.
imports=$(nm -D --undefined-only "$test_root/fortified_open" | awk '{print $2}' | sed 's/@.*//')
for symbol in __open_2 __open64_2 __openat_2 __openat64_2 openat64; do
    if ! grep -qx "$symbol" <<<"$imports"; then
        echo "this compiler does not call $symbol for a fortified program; nothing to test" >&2
        exit 0
    fi
done

# Prints "calls bytes" of one timer entry from the [MONITOR] lines on stdout (builds
# without TIMER_JSON) or the timer's JSON file.
timer_entry() {
    local mode=$1 type=$2 call=$3
    local line json
    line=$(grep "^\[MONITOR\] $type $call " "$test_root/output-$mode" | head -n 1 || true)
    if [[ -n $line ]]; then
        echo "$line" | awk '{print $5, $6}'
        return
    fi
    json=$(compgen -G "$test_root/traces-$mode/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
entry = timer[sys.argv[2]][sys.argv[3]]
print(entry[1], entry[2])' "$json" "$type" "$call"
    fi
}

for mode in 1 0; do
    mkdir -p "$test_root/traces-$mode" "$test_root/work-$mode"
    status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/traces-$mode" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/fortified_open" "$test_root/work-$mode" plain \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -eq 124 ]]; then
        echo "the fortified program did not finish: an open that does not block without the library blocks with it (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if [[ $status -ne 0 ]]; then
        echo "the fortified program failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi
    if [[ $(stat -c %s "$test_root/work-$mode/data-fortified-open") -ne 20480 ]]; then
        echo "data-fortified-open does not hold the 5 blocks written (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi

    # One block through each of the five descriptors, then five blocks read back
    # and a sixth read at the end of the file.
    for expected in "write 5 20480" "read 6 20480"; do
        call=${expected%% *}
        got=$(timer_entry "$mode" monitor "$call")
        if [[ -n $got && $got != "${expected#* }" ]]; then
            echo "monitored ${call}s of the fortified program: '$got' (calls bytes), expected '${expected#* }' (DATALIFE_JSON_OUTPUT=$mode)" >&2
            exit 1
        fi
    done

    if [[ $mode -eq 1 ]]; then
        for kind in w r; do
            trace=$(compgen -G "$test_root/traces-$mode/data-fortified-open.*.${kind}_blk_trace.json" | head -n 1 || true)
            if [[ -z $trace ]]; then
                echo "no $kind trace was written for the file opened through the fortified calls" >&2
                exit 1
            fi
            if command -v python3 >/dev/null 2>&1; then
                got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"], d["file_size_bytes"])' "$trace")
                if [[ $got != "5 20480 20480" ]]; then
                    echo "access_frequency, data_volume and file_size_bytes in the $kind trace: '$got', expected '5 20480 20480'" >&2
                    exit 1
                fi
            fi
        done
    else
        for kind in w r; do
            stat=$(compgen -G "$test_root/traces-$mode/data-fortified-open_*_${kind}_stat" | head -n 1 || true)
            got=$(tail -n +2 "${stat:-/dev/null}" | tr '\n' ';')
            if [[ $got != "0 1 4096;1 1 4096;2 1 4096;3 1 4096;4 1 4096;" ]]; then
                echo "${kind}_stat rows of the fortified program: '$got', expected one access of each of 5 blocks" >&2
                exit 1
            fi
        done
    fi
done

# Flags that need a mode, passed without one: glibc aborts the program, and so
# must the library.
mkdir -p "$test_root/work-abort" "$test_root/traces-abort"
for preload in "" "$monitor_lib"; do
    status=0
    { # the braces keep the shell's own "Aborted" report out of the test's output
        (
            ulimit -c 0
            exec timeout 20 env \
                LD_PRELOAD="$preload" \
                DATALIFE_OUTPUT_PATH="$test_root/traces-abort" \
                DATALIFE_FILE_PATTERNS='data-*' \
                DATALIFE_JSON_OUTPUT=1 \
                "$test_root/fortified_open" "$test_root/work-abort" creat
        ) >"$test_root/output-abort" 2>&1 || status=$?
    } 2>/dev/null
    if [[ $status -ne 134 ]]; then
        echo "open with O_CREAT and no mode ended with status $status, expected 134 (SIGABRT) (LD_PRELOAD='$preload'):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-abort" | tail -n 20 >&2
        exit 1
    fi
    if ! grep -q 'invalid open call' "$test_root/output-abort"; then
        echo "open with O_CREAT and no mode did not report glibc's error (LD_PRELOAD='$preload'):" >&2
        tail -n 20 "$test_root/output-abort" >&2
        exit 1
    fi
done

echo "files opened through the fortified variants are monitored"