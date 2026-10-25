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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-legacy-output.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Legacy output (DATALIFE_JSON_OUTPUT=0) for a file a process opens, reads and
# closes many times: one 16 KiB file read fully per session. The access sequence
# must hold each access once, and the block histogram must be one table of this
# process's totals, whatever the number of sessions or the threads closing.
cat >"$test_root/sessions.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BLOCK 4096
#define BLOCKS 4

static int sessions;
static atomic_int errors;

static void *run_sessions(void *arg) {
    (void)arg;
    char buf[BLOCK];
    for (int s = 0; s < sessions; s++) {
        int fd = open("data-sessions", O_RDONLY);
        for (int b = 0; fd >= 0 && b < BLOCKS; b++) {
            if (read(fd, buf, BLOCK) != BLOCK) {
                atomic_fetch_add(&errors, 1);
            }
        }
        if (fd < 0 || close(fd) != 0) {
            atomic_fetch_add(&errors, 1);
        }
    }
    return NULL;
}

/* usage: sessions dir n threads [exec]   With "exec", the program re-executes
   itself after its sessions to run them again as the same pid. */
int main(int argc, char **argv) {
    if (argc < 4 || argc > 5 || chdir(argv[1]) != 0) {
        return 2;
    }
    sessions = atoi(argv[2]);
    int threads = atoi(argv[3]);
    if (strcmp(argv[1], ".") != 0) {
        /* Prepared with raw system calls, so only the sessions are monitored. */
        char buf[BLOCK];
        memset(buf, 'd', sizeof buf);
        int fd = (int)syscall(SYS_openat, AT_FDCWD, "data-sessions", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        for (int b = 0; b < BLOCKS; b++) {
            if (syscall(SYS_write, fd, buf, BLOCK) != BLOCK) {
                return 1;
            }
        }
        syscall(SYS_close, fd);
    }

    pthread_t t[64];
    for (int i = 0; i < threads; i++) {
        pthread_create(&t[i], NULL, run_sessions, NULL);
    }
    for (int i = 0; i < threads; i++) {
        pthread_join(t[i], NULL);
    }
    if (atomic_load(&errors)) {
        return 1;
    }
    if (argc == 5) {
        /* Same pid, new process image: its tables follow the first image's in the files. */
        char *args[] = {argv[0], ".", argv[2], argv[3], NULL};
        execv("/proc/self/exe", args);
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -pthread "$test_root/sessions.c" -o "$test_root/sessions"

fail() {
    echo "$1" >&2
    exit 1
}

# run name sessions threads [exec]: runs the program in legacy mode; sets $stat and $trace.
run() {
    local name=$1
    shift
    local traces="$test_root/traces-$name"
    local work="$test_root/work-$name"
    mkdir -p "$traces" "$work"
    local status=0
    timeout 60 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT=0 \
        "$test_root/sessions" "$work" "$@" \
        >"$test_root/output-$name" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        grep -v '^\[MONITOR\]' "$test_root/output-$name" | tail -n 20 >&2
        fail "$name: the program failed with status $status"
    fi
    stat=$(compgen -G "$traces/data-sessions_*_r_stat" | head -n 1 || true)
    trace=$(compgen -G "$traces/data-sessions_*_r_trace_stat" | head -n 1 || true)
    [[ -n $stat && -n $trace ]] || fail "$name: no r_stat or r_trace_stat written"
}

# expect name accesses tables threads: the sequence has every block read once per
# session (in order, unless threads interleaved them); the histogram has one header
# and one table per process image with the totals.
expect() {
    local name=$1 accesses=$2 tables=$3 threads=$4
    local lines want b
    lines=$(wc -l <"$trace")
    [[ $lines -eq $((accesses * 4)) ]] || fail "$name: r_trace_stat has $lines entries for $((accesses * 4)) block reads"
    for b in 0 1 2 3; do
        [[ $(grep -c "^$b\$" "$trace") -eq $accesses ]] || fail "$name: r_trace_stat lists block $b $(grep -c "^$b\$" "$trace") times, expected $accesses"
    done
    if [[ $threads -eq 1 ]]; then
        [[ $(head -n 4 "$trace" | tr '\n' ' ') == "0 1 2 3 " ]] || fail "$name: r_trace_stat does not start with blocks 0 1 2 3"
    fi
    [[ $(grep -c 'Block no' "$stat") -eq 1 ]] || fail "$name: r_stat has $(grep -c 'Block no' "$stat") header lines"
    want=$(for ((t = 0; t < tables; t++)); do for b in 0 1 2 3; do printf '%d %d 4096;' "$b" $((accesses / tables)); done; done)
    [[ $(tail -n +2 "$stat" | tr '\n' ';') == "$want" ]] ||
        fail "$name: r_stat rows are '$(tail -n +2 "$stat" | tr '\n' ';' | cut -c1-120)', expected '$want'"
}

run three 3 1
expect three 3 1 1

run many 400 1
expect many 400 1 1
size=$(stat -c '%s' "$trace")
[[ $size -lt 20000 ]] || fail "many: r_trace_stat is $size bytes for 1600 entries"

run threads 20 8
expect threads 160 1 8

run exec 2 1 exec
expect exec 4 2 1

echo "legacy output holds each access once and one histogram table per process"
