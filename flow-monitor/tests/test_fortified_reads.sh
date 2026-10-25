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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-fortified-reads.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Programs built with _FORTIFY_SOURCE (the default when optimizing on most
# distributions) read through __read_chk, __pread_chk, __fread_chk, __fgets_chk and
# their unlocked variants whenever the compiler knows the buffer's size but not the
# request's. Those reads must be monitored like the plain calls.
cat >"$test_root/fortified_reads.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BLOCK 4096

static char buf[4 * BLOCK]; /* a known size, so the compiler uses the checked variants */

int main(int argc, char **argv) {
    if (argc != 3 || chdir(argv[1]) != 0) {
        return 2;
    }
    size_t n = (size_t)atoi(argv[2]); /* 4096 at run time */

    /* 4 blocks of 'd', prepared without the library seeing it. */
    memset(buf, 'd', sizeof buf);
    int fd = (int)syscall(SYS_openat, AT_FDCWD, "data-fortified", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || syscall(SYS_write, fd, buf, sizeof buf) != (long)sizeof buf || syscall(SYS_close, fd) != 0) {
        return 1;
    }

    fd = open("data-fortified", O_RDONLY); /* 4 reads: blocks 0-3 */
    for (int i = 0; i < 4; i++) {
        if (read(fd, buf, n) != (ssize_t)n) return 1;
    }
    if (pread(fd, buf, n, 0) != (ssize_t)n || pread(fd, buf, n, BLOCK) != (ssize_t)n) return 1; /* blocks 0, 1 */
    if (close(fd) != 0) return 1;

    FILE *stream = fopen("data-fortified", "r"); /* 4 stream reads: blocks 0-3 */
    for (int i = 0; i < 4; i++) {
        if (fread(buf, 1, n, stream) != n) return 1;
    }
    if (fclose(stream) != 0) return 1;

    stream = fopen("data-fortified", "r"); /* 4 unlocked stream reads: blocks 0-3 */
    for (int i = 0; i < 4; i++) {
        if (fread_unlocked(buf, 1, n, stream) != n) return 1;
    }
    if (fclose(stream) != 0) return 1;

    stream = fopen("data-fortified", "r"); /* 4 lines of n - 1 bytes (no newlines), straddling blocks */
    for (int i = 0; i < 4; i++) {
        if (!fgets(buf, (int)n, stream)) return 1;
    }
    if (fclose(stream) != 0) return 1;

    stream = fopen("data-fortified", "r");
    for (int i = 0; i < 4; i++) {
        if (!fgets_unlocked(buf, (int)n, stream)) return 1;
    }
    return fclose(stream);
}
EOF

cc -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=3 "$test_root/fortified_reads.c" -o "$test_root/fortified_reads"

# The program must really use the checked variants, or the test proves nothing.
imports=$(nm -D --undefined-only "$test_root/fortified_reads" | awk '{print $2}' | sed 's/@.*//')
for symbol in __read_chk __pread_chk __fread_chk __fread_unlocked_chk __fgets_chk __fgets_unlocked_chk; do
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
        "$test_root/fortified_reads" "$test_root/work-$mode" 4096 \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "the fortified program failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    # read, pread, fread and fread_unlocked all count as monitor reads: 14 calls, 57344 bytes;
    # the 8 fgets calls as fgets.
    got=$(timer_entry "$mode" monitor read)
    if [[ -n $got && $got != "14 57344" ]]; then
        echo "monitored reads of the fortified program: '$got' (calls bytes), expected '14 57344' (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    got=$(timer_entry "$mode" monitor fgets)
    if [[ -n $got && ${got%% *} != 8 ]]; then
        echo "monitored fgets calls of the fortified program: '${got%% *}', expected 8 (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi

    # The block histogram: every access of the 4 blocks, in both output modes.
    if [[ $mode -eq 1 ]]; then
        trace=$(compgen -G "$test_root/traces-$mode/data-fortified.*.r_blk_trace.json" | head -n 1 || true)
        if [[ -z $trace ]]; then
            echo "no read trace was written for the fortified program" >&2
            exit 1
        fi
        if command -v python3 >/dev/null 2>&1; then
            got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"])' "$trace")
            # Block touches: 4 + 2 + 4 + 4 by the block-sized reads, and 7 by each set of 4
            # fgets calls, whose 4095-byte lines straddle blocks (1 + 2 + 2 + 2). The volume
            # is each block's frequency times its first access's bytes, 4096 for all four:
            # block 0: 8, block 1: 8, block 2: 7, block 3: 5 touches.
            if [[ $got != "28 114688" ]]; then
                echo "access_frequency and data_volume of the fortified program: '$got', expected '28 114688'" >&2
                exit 1
            fi
        fi
    else
        stat=$(compgen -G "$test_root/traces-$mode/data-fortified_*_r_stat" | head -n 1 || true)
        got=$(tail -n +2 "${stat:-/dev/null}" | tr '\n' ';')
        # Per block: 1 read + 1 pread (blocks 0, 1) + 1 fread + 1 fread_unlocked, plus the
        # fgets and fgets_unlocked lines straddling blocks: 2+2 in blocks 0, 1 and 2, 1+1 in 3.
        if [[ $got != "0 8 4096;1 8 4096;2 7 4096;3 5 4096;" ]]; then
            echo "r_stat rows of the fortified program: '$got', expected '0 8 4096;1 8 4096;2 7 4096;3 5 4096;'" >&2
            exit 1
        fi
    fi
done

echo "reads through the fortified variants are monitored"
