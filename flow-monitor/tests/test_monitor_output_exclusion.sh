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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-output-exclusion.XXXXXX")
trap 'rm -rf "$test_root"' EXIT
mkdir -p "$test_root/traces" "$test_root/work"

cat >"$test_root/write_files.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

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

static int write_file_at(const char *directory_path) {
    int directory = open(directory_path, O_RDONLY | O_DIRECTORY);
    if (directory < 0) {
        perror(directory_path);
        return 1;
    }
    int fd = openat(directory, "sifting-openat-output", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("openat");
        close(directory);
        return 1;
    }
    if (write(fd, "payload\n", 8) != 8) {
        perror("write");
        close(fd);
        close(directory);
        return 1;
    }
    return close(fd) != 0 || close(directory) != 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        return 2;
    }
    return write_file(argv[1]) || write_file(argv[2]) || write_file_at(argv[3]);
}
EOF

cc -Wall -Wextra -Werror "$test_root/write_files.c" -o "$test_root/write_files"

run_case() {
    local mode=$1
    local expected_trace=$2
    local traces="$test_root/traces-$mode"
    mkdir -p "$traces"

    timeout 10 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='sift*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/write_files" \
        "$test_root/work/sifting-$mode.py" \
        "$traces/sifting-direct-output" \
        "$traces"

    mapfile -t generated < <(find "$traces" -maxdepth 1 -type f -printf '%f\n' | sort)
    if (( ${#generated[@]} > 8 )); then
        printf 'monitor output grew unexpectedly (%d files):\n' "${#generated[@]}" >&2
        printf '  %s\n' "${generated[@]}" >&2
        exit 1
    fi

    if printf '%s\n' "${generated[@]}" | grep -Eq '(blk_trace\.json|trace_stat).*(blk_trace\.json|trace_stat)'; then
        printf 'monitor recursively traced its own output:\n' >&2
        printf '  %s\n' "${generated[@]}" >&2
        exit 1
    fi

    if printf '%s\n' "${generated[@]}" | grep -Eq '^sifting-(direct|openat)-output.*(blk_trace\.json|trace_stat)$'; then
        echo "a file under DATALIFE_OUTPUT_PATH was monitored" >&2
        exit 1
    fi

    if ! printf '%s\n' "${generated[@]}" | grep -Eq "$expected_trace"; then
        printf 'expected workflow trace was not emitted:\n' >&2
        printf '  %s\n' "${generated[@]}" >&2
        exit 1
    fi
}

run_case 1 '^sifting-1\.py\..*w_blk_trace\.json$'
run_case 0 '^sifting-0\.py_.*_w_trace_stat$'

echo "monitor-owned output excluded from instrumentation"
