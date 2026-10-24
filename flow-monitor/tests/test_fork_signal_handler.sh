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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-fork-signal-handler.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# fork() from a signal handler, as a crash or timeout handler that starts a helper
# process does. The program's only thread keeps doing monitored I/O, so the signal
# often arrives while that thread holds, or is taking, one of libmonitor's locks,
# which only the interrupted code can release.
cat >"$test_root/fork_signal_handler.c" <<'EOF'
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

#define BLOCK 4096
#define MAX_FORKS 1000
#define MAX_FORK_ERRORS 100

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
    if (argc != 3) {
        return 2;
    }
    if (chdir(argv[1]) != 0) {
        perror("chdir");
        return 1;
    }
    forks = atoi(argv[2]);
    if (forks < 1 || forks > MAX_FORKS) {
        return 2;
    }

    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = on_alarm;
    action.sa_flags = SA_RESTART;
    struct itimerval every_500us = {{0, 500}, {0, 500}};
    if (sigaction(SIGALRM, &action, NULL) != 0 || setitimer(ITIMER_REAL, &every_500us, NULL) != 0) {
        perror("SIGALRM");
        return 1;
    }

    char buf[BLOCK];
    memset(buf, 's', sizeof buf);
    int io_errors = 0;
    while (forked < forks && fork_errors < MAX_FORK_ERRORS) {
        int fd = open("data-signal", O_RDWR | O_CREAT, 0644);
        if (fd < 0 || pwrite(fd, buf, BLOCK, 0) != BLOCK || pread(fd, buf, BLOCK, 0) != BLOCK ||
            close(fd) != 0) {
            io_errors++;
        }
        FILE *stream = fopen("data-signal", "r");
        if (!stream || fread(buf, 1, BLOCK, stream) != BLOCK || fclose(stream) != 0) {
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

    /* A rare crashed child is tolerated: libmonitor's child fork handler allocates,
       which can fail in a child forked while the signal interrupted malloc. */
    if (hung || failed || crashed * 100 > forked || fork_errors || io_errors) {
        fprintf(stderr, "%d of %d children hung, %d crashed, %d failed; %d fork errors, %d I/O errors\n",
                hung, (int)forked, crashed, failed, (int)fork_errors, io_errors);
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror "$test_root/fork_signal_handler.c" -o "$test_root/fork_signal_handler"

forks=200

# Legacy output only. With DATALIFE_JSON_OUTPUT=1 libmonitor writes traces from
# std::async threads, and once a process has had a second thread glibc's fork()
# takes malloc's locks, so fork() from a signal handler that interrupted malloc
# hangs in glibc whatever libmonitor's fork handlers do.
mkdir -p "$test_root/traces" "$test_root/work"
status=0
timeout 30 env \
    LD_PRELOAD="$monitor_lib" \
    DATALIFE_OUTPUT_PATH="$test_root/traces" \
    DATALIFE_FILE_PATTERNS='data-*' \
    DATALIFE_JSON_OUTPUT=0 \
    "$test_root/fork_signal_handler" "$test_root/work" "$forks" \
    >"$test_root/output" 2>&1 || status=$?
if [[ $status -eq 124 ]]; then
    echo "fork() from a signal handler hung" >&2
    exit 1
fi
if [[ $status -ne 0 ]]; then
    echo "fork() from a signal handler failed with status $status:" >&2
    grep -v '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
    exit 1
fi
if ! compgen -G "$test_root/traces/data-signal_*_w_trace_stat" >/dev/null; then
    echo "expected trace data-signal_*_w_trace_stat was not emitted" >&2
    ls "$test_root/traces" >&2
    exit 1
fi

echo "fork() from a signal handler does not wait for libmonitor's locks"
