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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-fsync.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# fsync() on a monitored file must reach the system, as on any other file. A
# library preloaded after libmonitor stands in for libc's fsync(): libmonitor
# passes calls on to the next definition, so this one sees each fsync() that
# reaches the system and counts it in fsync-count.
cat >"$test_root/fsync_count.c" <<'EOF'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

int fsync(int fd) {
    int count = (int)syscall(SYS_openat, AT_FDCWD, "fsync-count", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (count >= 0) {
        syscall(SYS_write, count, "x", 1);
        syscall(SYS_close, count);
    }
    int (*next)(int) = (int (*)(int))dlsym(RTLD_NEXT, "fsync");
    return next(fd);
}
EOF

cat >"$test_root/fsync_files.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    int monitored = open("data-sync", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int plain = open("plain-sync", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (monitored < 0 || plain < 0 || write(monitored, "sync", 4) != 4 || write(plain, "sync", 4) != 4) {
        perror("setup");
        return 1;
    }
    if (fsync(monitored) != 0 || fsync(plain) != 0) {
        perror("fsync");
        return 1;
    }
    close(monitored);
    close(plain);
    struct stat st;
    long calls = stat("fsync-count", &st) == 0 ? (long)st.st_size : 0;
    if (calls != 2) {
        fprintf(stderr, "%ld of 2 fsync() calls reached the system\n", calls);
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -shared -fPIC "$test_root/fsync_count.c" -o "$test_root/libfsync_count.so" -ldl
cc -Wall -Wextra -Werror "$test_root/fsync_files.c" -o "$test_root/fsync_files"

run_case() {
    local mode=$1
    local traces="$test_root/traces-$mode"
    local work="$test_root/work-$mode"
    mkdir -p "$traces" "$work"
    local status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib $test_root/libfsync_count.so" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/fsync_files" "$work" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "fsync() of a monitored file failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi
}

run_case 1
run_case 0
echo "fsync() of a monitored file reaches the system"
