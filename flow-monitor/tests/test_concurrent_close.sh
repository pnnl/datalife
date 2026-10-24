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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-concurrent-close.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Threads close their monitored files at the same moment, the first closes in the
# process. The timer must still record every file's size: each run lists all 16.
cat >"$test_root/first_close.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define THREADS 16

static pthread_barrier_t barrier;

static void *closer(void *arg) {
    char path[32];
    snprintf(path, sizeof path, "data-first%d", (int)(intptr_t)arg);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0 && write(fd, "x", 1) != 1) {
        perror("write");
    }
    pthread_barrier_wait(&barrier);
    if (fd >= 0) {
        close(fd);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    pthread_t threads[THREADS];
    pthread_barrier_init(&barrier, NULL, THREADS);
    for (intptr_t t = 0; t < THREADS; t++) {
        pthread_create(&threads[t], NULL, closer, (void *)t);
    }
    for (int t = 0; t < THREADS; t++) {
        pthread_join(threads[t], NULL);
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -pthread "$test_root/first_close.c" -o "$test_root/first_close"

runs=50
for run in $(seq "$runs"); do
    traces="$test_root/traces-$run"
    work="$test_root/work-$run"
    mkdir -p "$traces" "$work"
    status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT=1 \
        "$test_root/first_close" "$work" \
        >"$test_root/output" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "run $run failed with status $status:" >&2
        grep -v '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
        exit 1
    fi
    # The file list is in the timer's JSON file (TIMER_JSON builds) or on stdout.
    listed=$(cat "$test_root/output" "$traces"/monitor_timer.*.json 2>/dev/null |
        grep -c -E '^\[MONITOR\] file data-first|^ +"data-first[0-9]+": \{' || true)
    if [[ $listed -ne 16 ]]; then
        echo "run $run: the timer lists $listed of the 16 closed files" >&2
        exit 1
    fi
    rm -rf "$traces" "$work"
done

echo "the timer records every file closed at the same moment by several threads"
