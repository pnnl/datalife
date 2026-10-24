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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-concurrent-open.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Threads read a monitored file through their own descriptors while other threads
# keep opening and closing it, so positions are added to the file's record while
# the readers update theirs. To run it against a sanitizer build of libmonitor,
# set DATALIFE_TEST_PRELOAD to the sanitizer's runtime library, which must be
# preloaded first.
cat >"$test_root/concurrent_open.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BLOCK 4096
#define BLOCKS 16
#define READERS 4
#define OPENERS 2
#define OPENS 2500

static atomic_int openers_done;
static atomic_int errors;

static void *reader(void *arg) {
    (void)arg;
    char buf[BLOCK];
    int fd = open("data-shared", O_RDONLY);
    if (fd < 0) {
        atomic_fetch_add(&errors, 1);
        return NULL;
    }
    while (atomic_load(&openers_done) < OPENERS) {
        if (lseek(fd, 0, SEEK_SET) != 0) {
            atomic_fetch_add(&errors, 1);
            break;
        }
        for (int b = 0; b < BLOCKS; b++) {
            if (read(fd, buf, BLOCK) != BLOCK || buf[0] != (char)('a' + b) || buf[BLOCK - 1] != (char)('a' + b)) {
                atomic_fetch_add(&errors, 1);
                break;
            }
        }
    }
    if (close(fd) != 0) {
        atomic_fetch_add(&errors, 1);
    }
    return NULL;
}

static void *opener(void *arg) {
    (void)arg;
    for (int i = 0; i < OPENS; i++) {
        int fd = open("data-shared", O_RDONLY);
        if (fd < 0 || close(fd) != 0) {
            atomic_fetch_add(&errors, 1);
        }
    }
    atomic_fetch_add(&openers_done, 1);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    char buf[BLOCK];
    int fd = open("data-shared", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (int b = 0; fd >= 0 && b < BLOCKS; b++) {
        memset(buf, 'a' + b, sizeof buf);
        if (write(fd, buf, BLOCK) != BLOCK) {
            perror("write");
            return 1;
        }
    }
    /* Kept open throughout, so the file's record, and every position added to
       it, lives until the end. */
    int keep = open("data-shared", O_RDONLY);
    if (fd < 0 || close(fd) != 0 || keep < 0) {
        perror("setup");
        return 1;
    }

    pthread_t threads[READERS + OPENERS];
    for (int t = 0; t < READERS + OPENERS; t++) {
        pthread_create(&threads[t], NULL, t < READERS ? reader : opener, NULL);
    }
    for (int t = 0; t < READERS + OPENERS; t++) {
        pthread_join(threads[t], NULL);
    }
    close(keep);
    if (atomic_load(&errors)) {
        fprintf(stderr, "%d I/O errors or wrong data\n", atomic_load(&errors));
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -pthread "$test_root/concurrent_open.c" -o "$test_root/concurrent_open"

# JSON output only: the legacy output appends the file's whole read history to
# its trace at every close, which for these thousands of closes takes minutes.
mkdir -p "$test_root/traces" "$test_root/work"
status=0
timeout 120 env \
    LD_PRELOAD="${DATALIFE_TEST_PRELOAD:+$DATALIFE_TEST_PRELOAD }$monitor_lib" \
    DATALIFE_OUTPUT_PATH="$test_root/traces" \
    DATALIFE_FILE_PATTERNS='data-*' \
    DATALIFE_JSON_OUTPUT=1 \
    "$test_root/concurrent_open" "$test_root/work" \
    >"$test_root/output" 2>&1 || status=$?
if [[ $status -ne 0 ]]; then
    echo "reads while other threads open the file failed with status $status:" >&2
    grep -v '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
    exit 1
fi

echo "a file's descriptors can be opened while others are in use"
