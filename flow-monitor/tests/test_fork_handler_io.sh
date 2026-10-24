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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-fork-handler-io.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Fork handlers that do I/O, as a library that logs or flushes around fork() has.
# The test program calls into the library, so the library is one of its
# dependencies: its handlers are registered before libmonitor's and run while
# libmonitor's fork handlers hold its locks.
cat >"$test_root/handler_io.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static int registered;

static void append(const char *name) {
    char path[64];
    snprintf(path, sizeof path, "%s.fd", name);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        if (write(fd, "x", 1) != 1) {
            perror("write");
        }
        close(fd);
    }
    snprintf(path, sizeof path, "%s.stream", name);
    FILE *stream = fopen(path, "a");
    if (stream) {
        fputc('x', stream);
        fclose(stream);
    }
}

static void prepare(void) { append("handler-prepare"); }
static void parent(void) { append("handler-parent"); }
static void child(void) { append("handler-child"); }

__attribute__((constructor)) static void register_handlers(void) {
    registered = pthread_atfork(prepare, parent, child) == 0;
}

/* The test program calls this, so the linker keeps the library even with --as-needed. */
int handler_io_registered(void) { return registered; }
EOF

cat >"$test_root/fork_handler_io.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int handler_io_registered(void);

static int write_file(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(path);
        return 1;
    }
    if (write(fd, "payload\n", 8) != 8) {
        perror("write");
        close(fd);
        return 1;
    }
    if (close(fd) != 0) {
        perror("close");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        return 2;
    }
    if (chdir(argv[1]) != 0) {
        perror("chdir");
        return 1;
    }
    if (!handler_io_registered()) {
        fprintf(stderr, "fork handlers were not registered\n");
        return 1;
    }
    int forks = atoi(argv[2]);
    for (int i = 0; i < forks; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            char path[64];
            snprintf(path, sizeof path, "data-child-%d", i);
            return write_file(path);
        }
        int status;
        if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "child %d failed\n", i);
            return 1;
        }
    }
    return write_file("data-parent");
}
EOF

cc -Wall -Wextra -Werror -pthread -shared -fPIC "$test_root/handler_io.c" -o "$test_root/libhandler_io.so"
cc -Wall -Wextra -Werror -pthread "$test_root/fork_handler_io.c" -o "$test_root/fork_handler_io" \
    -L"$test_root" -lhandler_io -Wl,-rpath,"$test_root"

forks=4

expect_size() {
    local path=$1
    local actual
    actual=$(stat -c '%s' "$path" 2>/dev/null || echo missing)
    if [[ "$actual" != "$forks" ]]; then
        echo "fork handler output $path: expected $forks bytes, observed $actual" >&2
        exit 1
    fi
}

expect_trace() {
    local traces=$1
    local pattern=$2
    if ! compgen -G "$traces/$pattern" >/dev/null; then
        echo "expected trace $pattern was not emitted; I/O after fork() went unmonitored" >&2
        ls "$traces" >&2
        exit 1
    fi
}

run_case() {
    local mode=$1
    local traces="$test_root/traces-$mode"
    local work="$test_root/work-$mode"
    mkdir -p "$traces" "$work"
    local status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*,handler-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/fork_handler_io" "$work" "$forks" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -eq 124 ]]; then
        echo "fork() hung with fork handlers doing I/O (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if [[ $status -ne 0 ]]; then
        echo "test program failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    local phase kind
    for phase in prepare parent child; do
        for kind in fd stream; do
            expect_size "$work/handler-$phase.$kind"
        done
    done

    local i
    for ((i = 0; i < forks; i++)); do
        if [[ $mode -eq 1 ]]; then
            expect_trace "$traces" "data-child-$i.*.w_blk_trace.json"
        else
            expect_trace "$traces" "data-child-${i}_*_w_trace_stat"
        fi
    done
    if [[ $mode -eq 1 ]]; then
        expect_trace "$traces" "data-parent.*.w_blk_trace.json"
    else
        expect_trace "$traces" "data-parent_*_w_trace_stat"
    fi
}

run_case 1
run_case 0
echo "fork handlers registered before libmonitor's can do I/O"
