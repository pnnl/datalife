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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-readv-writev.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# readv() and writev() on a monitored file are traced like read() and write(), and
# keep the behavior of the real calls: one call moves all the buffers, and a
# readv() returns what is there without waiting for more.
cat >"$test_root/vector_io.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

static int failures;

static void expect(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", what);
        failures++;
    }
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    alarm(10); /* a readv() that waited for more than is there would hang */

    static char a[4096], b[4096], c[100], x[4096], y[4096], z[4096];
    memset(a, 'a', sizeof a);
    memset(b, 'b', sizeof b);
    memset(c, 'c', sizeof c);
    struct iovec out[3] = {{a, sizeof a}, {b, sizeof b}, {c, sizeof c}};
    int fd = open("data-vec", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(fd >= 0 && writev(fd, out, 3) == 8292, "writev() of three buffers to a monitored file");
    close(fd);

    struct iovec in[3] = {{x, sizeof x}, {y, sizeof y}, {z, sizeof z}};
    fd = open("data-vec", O_RDONLY);
    expect(fd >= 0 && readv(fd, in, 3) == 8292, "readv() of a monitored file up to its end");
    expect(memcmp(x, a, sizeof a) == 0 && memcmp(y, b, sizeof b) == 0 && memcmp(z, c, sizeof c) == 0,
           "readv() filled each buffer in order");
    close(fd);

    int p[2];
    expect(pipe(p) == 0, "pipe");
    struct iovec two[2] = {{"ab", 2}, {"cd", 2}};
    char buf[8] = {0};
    expect(writev(p[1], two, 2) == 4 && syscall(SYS_read, p[0], buf, sizeof buf) == 4 && memcmp(buf, "abcd", 4) == 0,
           "writev() of two buffers to a pipe is one write");
    char m[8] = {0}, n[8] = {0};
    struct iovec part[2] = {{m, sizeof m}, {n, sizeof n}};
    expect(syscall(SYS_write, p[1], "xyz", 3) == 3 && readv(p[0], part, 2) == 3 && memcmp(m, "xyz", 3) == 0,
           "readv() of a pipe returns the 3 bytes there");
    close(p[0]);
    close(p[1]);
    return failures != 0;
}
EOF
cc -Wall -Wextra -Werror "$test_root/vector_io.c" -o "$test_root/vector_io"

# C++ file streams write any large block with writev().
cpp_test=0
if command -v c++ >/dev/null 2>&1; then
    cat >"$test_root/stream_io.cpp" <<'EOF'
#include <fstream>
#include <string>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    std::string block(8192, 'o');
    std::ofstream out("data-ofs", std::ios::binary);
    out.write(block.data(), block.size());
    out.write(block.data(), block.size());
    out.close();
    return out.good() ? 0 : 1;
}
EOF
    c++ -Wall -Wextra -Werror "$test_root/stream_io.cpp" -o "$test_root/stream_io"
    cpp_test=1
else
    echo "c++ not found; skipping the std::ofstream case" >&2
fi

run() {
    local mode=$1 name=$2 program=$3
    local traces="$test_root/traces-$name-$mode"
    local work="$test_root/work-$name-$mode"
    mkdir -p "$traces" "$work"
    local status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$program" "$work" \
        >"$test_root/output-$name-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "$name failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$name-$mode" | tail -n 20 >&2
        exit 1
    fi
}

# Prints "calls bytes" of one timer entry, such as "monitor writev": from the
# [MONITOR] lines on stdout (builds without TIMER_JSON) or the timer's JSON file.
timer_entry() {
    local mode=$1 name=$2 type=$3 call=$4
    local line json
    line=$(grep "^\[MONITOR\] $type $call " "$test_root/output-$name-$mode" | head -n 1 || true)
    if [[ -n $line ]]; then
        echo "$line" | awk '{print $5, $6}'
        return
    fi
    json=$(compgen -G "$test_root/traces-$name-$mode/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
entry = timer[sys.argv[2]][sys.argv[3]]
print(entry[1], entry[2])' "$json" "$type" "$call"
    fi
}

expect_timer() {
    local mode=$1 name=$2 type=$3 call=$4 want=$5
    local got
    got=$(timer_entry "$mode" "$name" "$type" "$call")
    if [[ -n $got && $got != "$want" ]]; then
        echo "timer entry '$type $call' is '$got', expected '$want' (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
}

# The trace of name's writes ("w") or reads ("r") has entries.
expect_trace() {
    local mode=$1 traces=$2 file=$3 kind=$4
    local trace
    if [[ $mode -eq 1 ]]; then
        trace=$(compgen -G "$traces/$file.*.${kind}_blk_trace.json" | head -n 1 || true)
    else
        trace=$(compgen -G "$traces/${file}_*_${kind}_trace_stat" | head -n 1 || true)
    fi
    if [[ -z $trace || ! -s $trace ]]; then
        echo "no $kind trace of $file (DATALIFE_JSON_OUTPUT=$mode); vector I/O went unmonitored" >&2
        ls "$traces" >&2
        exit 1
    fi
}

for mode in 1 0; do
    run "$mode" vector "$test_root/vector_io"
    expect_timer "$mode" vector monitor writev "1 8292"
    expect_timer "$mode" vector monitor readv "1 8292"
    expect_trace "$mode" "$test_root/traces-vector-$mode" data-vec w
    expect_trace "$mode" "$test_root/traces-vector-$mode" data-vec r

    if [[ $cpp_test -eq 1 ]]; then
        run "$mode" stream "$test_root/stream_io"
        written=0
        for call in write writev; do
            entry=$(timer_entry "$mode" stream monitor "$call")
            if [[ -z $entry ]]; then
                written=unknown
                break
            fi
            written=$((written + ${entry##* }))
        done
        if [[ $written != unknown && $written -ne 16384 ]]; then
            echo "std::ofstream wrote 16384 bytes, the timer recorded $written (DATALIFE_JSON_OUTPUT=$mode)" >&2
            exit 1
        fi
        expect_trace "$mode" "$test_root/traces-stream-$mode" data-ofs w
    fi
done

echo "readv() and writev() are monitored and behave as the real calls"
