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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-fork-signal-malloc.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# fork() from a signal handler that interrupted malloc or free. The child gets the
# heap mid-update, so libmonitor's child fork handler must not use it. The only
# thread does little but allocate and free blocks too large for malloc's
# per-thread cache, so most signals land in malloc's updates of its shared lists.
cat >"$test_root/fork_signal_malloc.c" <<'EOF'
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_FORKS 4000

static pid_t children[MAX_FORKS];
static volatile sig_atomic_t forked;
static volatile sig_atomic_t fork_errors;
static int forks;

static void on_alarm(int sig) {
    (void)sig;
    int saved_errno = errno;
    if (forked < forks) {
        pid_t pid = fork();
        if (pid == 0) {
            _exit(0);
        }
        if (pid > 0) {
            children[forked++] = pid;
        } else {
            fork_errors++;
        }
    }
    errno = saved_errno;
}

int main(int argc, char **argv) {
    if (argc != 3 || chdir(argv[1]) != 0) {
        return 2;
    }
    forks = atoi(argv[2]);
    if (forks < 1 || forks > MAX_FORKS) {
        return 2;
    }
    int fd = open("data-malloc", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = on_alarm;
    action.sa_flags = SA_RESTART;
    struct itimerval every_200us = {{0, 200}, {0, 200}};
    if (sigaction(SIGALRM, &action, NULL) != 0 || setitimer(ITIMER_REAL, &every_200us, NULL) != 0) {
        perror("SIGALRM");
        return 1;
    }

    void *blocks[64];
    unsigned seed = 1;
    int io_errors = 0;
    while (forked < forks && fork_errors < 100) {
        for (int i = 0; i < 64; i++) {
            blocks[i] = malloc(1100 + (rand_r(&seed) % 12) * 256);
        }
        for (int i = 63; i >= 0; i -= 2) {
            free(blocks[i]);
        }
        for (int i = 62; i >= 0; i -= 2) {
            free(blocks[i]);
        }
        if (write(fd, "x", 1) != 1) {
            io_errors++;
        }
    }
    struct itimerval off;
    memset(&off, 0, sizeof off);
    setitimer(ITIMER_REAL, &off, NULL);
    signal(SIGALRM, SIG_IGN);

    static int done[MAX_FORKS];
    int remaining = forked, crashed = 0, failed = 0, hung = 0;
    struct timespec start, now, pause = {0, 1000 * 1000};
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (remaining > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            for (int i = 0; i < forked; i++) {
                if (children[i] == pid) {
                    done[i] = 1;
                }
            }
            remaining--;
            if (WIFSIGNALED(status)) {
                crashed++;
            } else if (WEXITSTATUS(status) != 0) {
                failed++;
            }
            continue;
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (pid < 0 || now.tv_sec - start.tv_sec >= 10) {
            break;
        }
        nanosleep(&pause, NULL);
    }
    for (int i = 0; i < forked; i++) {
        if (!done[i]) {
            hung++;
            kill(children[i], SIGKILL);
            waitpid(children[i], NULL, 0);
        }
    }
    close(fd);

    if (hung || failed || crashed || fork_errors || io_errors) {
        fprintf(stderr, "%d of %d children hung, %d crashed, %d failed; %d fork errors, %d I/O errors\n",
                hung, (int)forked, crashed, failed, (int)fork_errors, io_errors);
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror "$test_root/fork_signal_malloc.c" -o "$test_root/fork_signal_malloc"

# Legacy output only, as in test_fork_signal_handler.sh: the process must stay
# single-threaded, or glibc's fork() itself waits for malloc's locks.
mkdir -p "$test_root/traces" "$test_root/work"
status=0
timeout 60 env \
    LD_PRELOAD="$monitor_lib" \
    DATALIFE_OUTPUT_PATH="$test_root/traces" \
    DATALIFE_FILE_PATTERNS='data-*' \
    DATALIFE_JSON_OUTPUT=0 \
    "$test_root/fork_signal_malloc" "$test_root/work" 1000 \
    >"$test_root/output" 2>&1 || status=$?
if [[ $status -ne 0 ]]; then
    echo "fork() from a signal handler that interrupted malloc failed with status $status:" >&2
    grep -v '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
    exit 1
fi

echo "children forked while the heap is mid-update run libmonitor's fork handler safely"
