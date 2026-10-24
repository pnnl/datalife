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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-blocked-read.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# A thread blocked in read() on a pipe, or on a monitored FIFO, must not hold up
# other threads' I/O: a process that waits for its own write to wake the reader
# (multiprocessing.Pool does) would otherwise hang. Each blocked reader is
# released after 1 s by a raw write the library does not see, so a stall shows as
# an I/O call that took that long instead of a hang.
cat >"$test_root/blocked_read.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

static int failures;
static int blocked[2];      /* the pipe a thread blocks on */
static int release_fd;      /* where the releaser writes */
static int other[2];        /* the pipe the main thread writes to */

static void expect(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", what);
        failures++;
    }
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void pause_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
}

static void *pipe_reader(void *arg) {
    char c;
    (void)arg;
    return (void *)(long)read(blocked[0], &c, 1);
}

static void *fifo_reader(void *arg) {
    char c;
    (void)arg;
    int fd = open("data-fifo", O_RDONLY);
    ssize_t n = fd < 0 ? -1 : read(fd, &c, 1);
    if (fd >= 0) {
        close(fd);
    }
    return (void *)(long)n;
}

static void *releaser(void *arg) {
    (void)arg;
    pause_ms(1000);
    syscall(SYS_write, release_fd, "x", 1);
    return NULL;
}

static int write_pipe(void) {
    return write(other[1], "y", 1) == 1;
}

static int write_monitored(void) {
    int fd = open("data-out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int ok = fd >= 0 && write(fd, "hello", 5) == 5;
    if (fd >= 0) {
        close(fd);
    }
    return ok;
}

static int writev_pipe(void) {
    struct iovec iov[2] = {{"ab", 2}, {"cd", 2}};
    char buf[8] = {0};
    return writev(other[1], iov, 2) == 4 && syscall(SYS_read, other[0], buf, sizeof buf) == 4 &&
           memcmp(buf, "abcd", 4) == 0;
}

/* Times op while another thread is blocked in read() on an empty pipe. */
static void while_pipe_read_blocks(int (*op)(void), const char *what) {
    pthread_t reader, release;
    if (pipe(blocked) != 0 || pipe(other) != 0) {
        expect(0, "pipe");
        return;
    }
    release_fd = blocked[1];
    pthread_create(&reader, NULL, pipe_reader, NULL);
    pthread_create(&release, NULL, releaser, NULL);
    pause_ms(200); /* let the reader block */
    double start = now();
    int ok = op();
    double took = now() - start;
    pthread_join(reader, NULL);
    pthread_join(release, NULL);
    char msg[128];
    snprintf(msg, sizeof msg, "%s while a read() blocks (took %.3f s)", what, took);
    expect(ok && took < 0.5, msg);
    close(blocked[0]);
    close(blocked[1]);
    close(other[0]);
    close(other[1]);
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    signal(SIGPIPE, SIG_IGN); /* a release can come after a FIFO's reader left */
    while_pipe_read_blocks(write_pipe, "write() to another pipe");
    while_pipe_read_blocks(write_monitored, "write() to a monitored file");
    while_pipe_read_blocks(writev_pipe, "writev() to another pipe");

    /* Both ends of a monitored FIFO in this process: the write that wakes the
       reader must not wait for it. */
    pthread_t reader, release;
    expect(mkfifo("data-fifo", 0644) == 0, "mkfifo");
    pthread_create(&reader, NULL, fifo_reader, NULL);
    int w = open("data-fifo", O_WRONLY); /* returns once the reader has opened it */
    release_fd = w;
    pthread_create(&release, NULL, releaser, NULL);
    pause_ms(200);
    double start = now();
    int ok = w >= 0 && write(w, "f", 1) == 1;
    double took = now() - start;
    void *got;
    pthread_join(reader, &got);
    pthread_join(release, NULL);
    char msg[128];
    snprintf(msg, sizeof msg, "write() to a monitored FIFO whose reader is blocked (took %.3f s)", took);
    expect(ok && took < 0.5 && (long)got == 1, msg);
    if (w >= 0) {
        close(w);
    }
    return failures != 0;
}
EOF

cc -Wall -Wextra -Werror -pthread "$test_root/blocked_read.c" -o "$test_root/blocked_read"

run_case() {
    local mode=$1
    local traces="$test_root/traces-$mode"
    local work="$test_root/work-$mode"
    mkdir -p "$traces" "$work"
    local status=0
    timeout 60 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/blocked_read" "$work" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "I/O while another thread's read() blocks failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi
}

run_case 1
run_case 0

# The same pattern as it shows up in practice: a process pool's threads block in
# read() on pipes while others write to them.
if command -v python3 >/dev/null 2>&1; then
    cat >"$test_root/pool.py" <<'EOF'
import hashlib
import multiprocessing
import os
import sys


def work(i):
    with open("data-pool%d" % (i % 4), "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


if __name__ == "__main__":
    os.chdir(sys.argv[1])
    for i in range(4):
        with open("data-pool%d" % i, "wb") as f:
            f.write(bytes([i]) * 65536)
    with multiprocessing.get_context("fork").Pool(4) as pool:
        digests = pool.map(work, range(64))
    print(hashlib.md5("".join(digests).encode()).hexdigest())
EOF
    mkdir -p "$test_root/pool-plain" "$test_root/pool-traces" "$test_root/pool-work"
    expected=$(python3 -I -W ignore "$test_root/pool.py" "$test_root/pool-plain")
    status=0
    timeout 60 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/pool-traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT=1 \
        python3 -I -W ignore "$test_root/pool.py" "$test_root/pool-work" \
        >"$test_root/pool-stdout" 2>"$test_root/pool-output" || status=$?
    # Builds without TIMER_JSON also print their [MONITOR] summary to stdout.
    actual=$(grep -v '^\[MONITOR\]' "$test_root/pool-stdout" || true)
    if [[ $status -ne 0 || "$actual" != "$expected" ]]; then
        echo "multiprocessing.Pool failed with status $status (output '$actual', expected '$expected'):" >&2
        grep -v '^\[MONITOR\]' "$test_root/pool-output" | tail -n 20 >&2
        exit 1
    fi
else
    echo "python3 not found; skipping the multiprocessing.Pool case" >&2
fi

echo "a thread blocked in read() does not hold up other threads' I/O"
