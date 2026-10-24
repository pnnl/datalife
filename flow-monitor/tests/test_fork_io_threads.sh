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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-fork-io-threads.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Threads keep doing monitored I/O while the main thread forks, so fork() often
# happens while one of them holds a libmonitor lock. Each child then does I/O of
# its own, including on a file the parent opened before the fork and, depending on
# parent_io, wrote to.
cat >"$test_root/fork_io_threads.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BLOCK 4096
#define THREADS 3
#define MAX_FORKS 100

static atomic_int stop_threads;
static atomic_int thread_errors;

static void *io_loop(void *arg) {
    char path[64];
    char buf[BLOCK];
    memset(buf, 't', sizeof buf);
    snprintf(path, sizeof path, "data-thread-%d", (int)(intptr_t)arg);
    while (!atomic_load(&stop_threads)) {
        int fd = open(path, O_RDWR | O_CREAT, 0644);
        if (fd < 0 || pwrite(fd, buf, BLOCK, 0) != BLOCK || pread(fd, buf, BLOCK, 0) != BLOCK ||
            close(fd) != 0) {
            atomic_fetch_add(&thread_errors, 1);
        }
        FILE *stream = fopen(path, "r");
        if (!stream || fread(buf, 1, BLOCK, stream) != BLOCK || fclose(stream) != 0) {
            atomic_fetch_add(&thread_errors, 1);
        }
    }
    return NULL;
}

/* The same I/O whatever the parent did before the fork. */
static int child_io(int inherited, int index) {
    char buf[BLOCK];
    memset(buf, 'c', sizeof buf);
    if (pwrite(inherited, buf, BLOCK, 0) != BLOCK || pwrite(inherited, buf, BLOCK, BLOCK) != BLOCK ||
        close(inherited) != 0) {
        perror("data-inherited");
        return 1;
    }
    char path[64];
    snprintf(path, sizeof path, "data-child-%d", index);
    FILE *stream = fopen(path, "w");
    if (!stream || fwrite(buf, 1, BLOCK, stream) != BLOCK || fclose(stream) != 0) {
        perror(path);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        return 2;
    }
    if (chdir(argv[1]) != 0) {
        perror("chdir");
        return 1;
    }
    int forks = atoi(argv[2]);
    int parent_io = atoi(argv[3]);
    if (forks < 1 || forks > MAX_FORKS) {
        return 2;
    }

    FILE *pid_file = fopen("parent.pid", "w");
    if (!pid_file || fprintf(pid_file, "%d\n", (int)getpid()) < 0 || fclose(pid_file) != 0) {
        perror("parent.pid");
        return 1;
    }

    int inherited = open("data-inherited", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (inherited < 0) {
        perror("data-inherited");
        return 1;
    }
    char buf[BLOCK];
    memset(buf, 'p', sizeof buf);
    for (int i = 0; parent_io && i < 8; i++) {
        if (pwrite(inherited, buf, BLOCK, (off_t)(64 + 3 * i) * BLOCK) != BLOCK) {
            perror("pwrite");
            return 1;
        }
    }

    pthread_t threads[THREADS];
    for (int t = 0; t < THREADS; t++) {
        if (pthread_create(&threads[t], NULL, io_loop, (void *)(intptr_t)t) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    pid_t children[MAX_FORKS];
    struct timespec pause = {0, 1000 * 1000};
    for (int i = 0; i < forks; i++) {
        nanosleep(&pause, NULL);
        children[i] = fork();
        if (children[i] < 0) {
            perror("fork");
            return 1;
        }
        if (children[i] == 0) {
            exit(child_io(inherited, i));
        }
    }

    int done[MAX_FORKS] = {0};
    int remaining = forks, failed = 0, hung = 0;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        for (int i = 0; i < forks; i++) {
            int status;
            if (!done[i] && waitpid(children[i], &status, WNOHANG) == children[i]) {
                done[i] = 1;
                remaining--;
                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    failed++;
                }
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (remaining == 0 || now.tv_sec - start.tv_sec >= 10) {
            break;
        }
        nanosleep(&pause, NULL);
    }
    for (int i = 0; i < forks; i++) {
        if (!done[i]) {
            hung++;
            kill(children[i], SIGKILL);
            waitpid(children[i], NULL, 0);
        }
    }

    atomic_store(&stop_threads, 1);
    for (int t = 0; t < THREADS; t++) {
        pthread_join(threads[t], NULL);
    }
    if (close(inherited) != 0) {
        perror("close");
        return 1;
    }
    if (hung || failed || atomic_load(&thread_errors)) {
        fprintf(stderr, "%d of %d children hung, %d failed, %d I/O errors in the parent's threads\n",
                hung, forks, failed, atomic_load(&thread_errors));
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -pthread "$test_root/fork_io_threads.c" -o "$test_root/fork_io_threads"

forks=20

run_case() {
    local mode=$1
    local parent_io=$2
    local run="$mode-$parent_io"
    mkdir -p "$test_root/traces-$run" "$test_root/work-$run"
    local status=0
    timeout 60 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/traces-$run" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/fork_io_threads" "$test_root/work-$run" "$forks" "$parent_io" \
        >"$test_root/output-$run" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "fork with I/O threads failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$run" | tail -n 20 >&2
        exit 1
    fi
}

json_number() {
    sed -n "s/.*\"$1\": \([0-9]*\).*/\1/p" "$2" | head -n 1
}

# Prints "access_frequency data_volume" from each child's trace of data-inherited.
child_inherited_stats() {
    local run=$1
    local parent_pid trace
    parent_pid=$(cat "$test_root/work-$run/parent.pid")
    for trace in "$test_root/traces-$run"/data-inherited.*.w_blk_trace.json; do
        [[ -f $trace && $trace != *".$parent_pid-"* ]] || continue
        echo "$(json_number access_frequency "$trace") $(json_number data_volume "$trace")"
    done
}

run_case 1 0
run_case 1 1
run_case 0 1

mapfile -t stats < <(child_inherited_stats 1-0; child_inherited_stats 1-1)
if (( ${#stats[@]} != 2 * forks )); then
    echo "expected $((2 * forks)) child traces of data-inherited, found ${#stats[@]}" >&2
    exit 1
fi
if (( $(printf '%s\n' "${stats[@]}" | sort -u | wc -l) != 1 )); then
    echo "child traces of data-inherited depend on the parent's I/O (count access_frequency data_volume):" >&2
    printf '%s\n' "${stats[@]}" | sort | uniq -c >&2
    exit 1
fi

echo "forked children run and trace their own I/O while other threads do I/O"
