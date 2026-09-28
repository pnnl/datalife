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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-open-modes.XXXXXX")
trap 'rm -rf "$test_root"' EXIT
mkdir -p "$test_root/traces" "$test_root/output"

cat >"$test_root/open_modes.c" <<'EOF'
#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int close_created(int fd, const char *operation) {
    if (fd < 0) {
        perror(operation);
        return 1;
    }
    if (close(fd) != 0) {
        perror("close");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    if (chdir(argv[1]) != 0) {
        perror("chdir");
        return 1;
    }
    umask(0);

    if (close_created(open("open-created", O_WRONLY | O_CREAT | O_EXCL, 0640), "open")) {
        return 1;
    }
    if (close_created(open64("open64-created", O_WRONLY | O_CREAT | O_EXCL, 0624), "open64")) {
        return 1;
    }

    int directory = open(".", O_RDONLY | O_DIRECTORY);
    if (directory < 0) {
        perror("open directory");
        return 1;
    }
    if (close_created(openat(directory, "openat-created", O_WRONLY | O_CREAT | O_EXCL, 0604), "openat")) {
        close(directory);
        return 1;
    }
    if (close(directory) != 0) {
        perror("close directory");
        return 1;
    }

    return 0;
}
EOF

cc -Wall -Wextra -Werror "$test_root/open_modes.c" -o "$test_root/open_modes"

env \
    LD_PRELOAD="$monitor_lib" \
    DATALIFE_OUTPUT_PATH="$test_root/traces" \
    DATALIFE_FILE_PATTERNS='open-created,open64-created,openat-created' \
    DATALIFE_JSON_OUTPUT=1 \
    "$test_root/open_modes" "$test_root/output"

check_mode() {
    local path=$1
    local expected=$2
    local actual
    actual=$(stat -c '%a' "$path")
    if [[ "$actual" != "$expected" ]]; then
        echo "creation mode mismatch for $path: expected $expected, observed $actual" >&2
        exit 1
    fi
}

check_mode "$test_root/output/open-created" 640
check_mode "$test_root/output/open64-created" 624
check_mode "$test_root/output/openat-created" 604

echo "open/open64/openat creation modes preserved"
