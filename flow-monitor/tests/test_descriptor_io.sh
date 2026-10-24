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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-descriptor-io.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Several descriptors open on one monitored file. I/O through each must use that
# descriptor: its offset, its flags, and the file it refers to even after another
# descriptor for the same name is closed and its number reused.
cat >"$test_root/descriptor_io.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static int failures;

static void expect(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", what);
        failures++;
    }
}

static int read_is(int fd, const char *want) {
    char buf[64] = {0};
    ssize_t n = (ssize_t)strlen(want);
    return read(fd, buf, n) == n && memcmp(buf, want, n) == 0;
}

/* Reads the file with raw system calls, so the check does not depend on the library. */
static int content_is(const char *path, const char *want) {
    char buf[64] = {0};
    int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    ssize_t n = (ssize_t)syscall(SYS_read, fd, buf, sizeof buf - 1);
    syscall(SYS_close, fd);
    return n == (ssize_t)strlen(want) && memcmp(buf, want, n) == 0;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }

    /* Each descriptor has its own offset. */
    int a = open("data-offsets", O_RDWR | O_CREAT | O_TRUNC, 0644);
    expect(a >= 0 && write(a, "0123456789", 10) == 10 && lseek(a, 0, SEEK_SET) == 0, "setup of data-offsets");
    int b = open("data-offsets", O_RDONLY);
    expect(read_is(a, "0123"), "read through the first descriptor");
    expect(read_is(b, "0123"), "a second descriptor reads from its own offset");
    expect(lseek(b, 6, SEEK_SET) == 6, "lseek on the second descriptor");
    expect(read_is(a, "45"), "lseek on one descriptor leaves the other's offset alone");
    expect(read_is(b, "67"), "read after lseek on the second descriptor");
    close(b);
    close(a);

    /* Writes use the flags of the descriptor they are made through. */
    int r = open("data-flags", O_RDONLY | O_CREAT | O_TRUNC, 0644);
    int w = open("data-flags", O_WRONLY);
    expect(r >= 0 && w >= 0 && write(w, "abc", 3) == 3, "write through a write-only descriptor while a read-only one is open");
    close(w);
    close(r);
    expect(content_is("data-flags", "abc"), "data-flags holds the write");

    int p = open("data-append", O_RDWR | O_CREAT | O_TRUNC, 0644);
    expect(p >= 0 && write(p, "XY", 2) == 2 && lseek(p, 0, SEEK_SET) == 0, "setup of data-append");
    int q = open("data-append", O_WRONLY | O_APPEND);
    expect(q >= 0 && write(q, "Z", 1) == 1, "write through an O_APPEND descriptor");
    close(q);
    close(p);
    expect(content_is("data-append", "XYZ"), "the O_APPEND descriptor appended");

    /* The first descriptor is closed and its number reused for an unmonitored file. */
    int first = open("data-reuse", O_RDWR | O_CREAT | O_TRUNC, 0644);
    int second = open("data-reuse", O_RDWR);
    close(first);
    int other = open("other.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    expect(other == first, "setup: other.txt reuses the closed descriptor's number");
    expect(write(second, "hello", 5) == 5, "write after the first descriptor was closed");
    expect(pwrite(second, "!!", 2, 5) == 2, "pwrite after the first descriptor was closed");
    char buf[8] = {0};
    expect(pread(second, buf, 7, 0) == 7 && memcmp(buf, "hello!!", 7) == 0, "pread after the first descriptor was closed");
    close(other);
    close(second); /* its trace records the size of data-reuse, through this descriptor */
    expect(content_is("data-reuse", "hello!!"), "the writes landed in data-reuse");
    expect(content_is("other.txt", ""), "and not in the file that reused the number");

    /* A descriptor opened after a stream on the same file. */
    FILE *stream = fopen("data-mixed", "w+");
    expect(stream && fwrite("stream", 1, 6, stream) == 6 && fflush(stream) == 0, "write through the stream");
    int d = open("data-mixed", O_RDONLY);
    expect(read_is(d, "stream"), "a descriptor opened after a stream reads from its own offset");
    close(d);
    if (stream) {
        fclose(stream);
    }

    return failures != 0;
}
EOF

cc -Wall -Wextra -Werror "$test_root/descriptor_io.c" -o "$test_root/descriptor_io"

run_case() {
    local mode=$1
    local traces="$test_root/traces-$mode"
    local work="$test_root/work-$mode"
    mkdir -p "$traces" "$work"
    local status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/descriptor_io" "$work" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "I/O through a second descriptor failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    if [[ $mode -eq 1 ]]; then
        local trace
        trace=$(compgen -G "$traces/data-reuse.*.w_blk_trace.json" | head -n 1 || true)
        if [[ -z $trace ]] || ! grep -q '"file_size_bytes": 7,' "$trace"; then
            echo "trace of data-reuse does not record its size (7 bytes) at the last close:" >&2
            cat "${trace:-/dev/null}" >&2
            exit 1
        fi
    fi
}

run_case 1
run_case 0
echo "I/O through each descriptor uses that descriptor"
