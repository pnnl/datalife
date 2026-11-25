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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-temp-names.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Programs write a temporary file next to the file it will become: "out.tmp"
# beside "out", or under a directory such as fasterq-dump's "fasterq.tmp.<host>.<pid>".
# A monitored file whose path holds ".tmp" must be a file of its own: closing it
# must not release the file whose name is the same without ".tmp", and a long
# path must not overflow anything.
cat >"$test_root/temp_names.c" <<'EOF_C'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK 4096

static char buf[BLOCK];
static int failures;

static void expect(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", what);
        failures++;
    }
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    memset(buf, 'd', sizeof buf);

    /* The temporary file is closed while the file beside it is still in use. */
    int out = open("data-out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int tmp = open("data-out.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(out >= 0 && tmp >= 0, "open of data-out and data-out.tmp");
    expect(write(tmp, buf, BLOCK) == BLOCK, "write to data-out.tmp");
    expect(close(tmp) == 0, "close of data-out.tmp");
    for (int i = 0; i < 8; i++) {
        expect(write(out, buf, BLOCK) == BLOCK, "write to data-out after data-out.tmp was closed");
    }
    expect(close(out) == 0, "close of data-out");

    /* The temporary file read back: a new open of it, not the one closed above. */
    tmp = open("data-out.tmp", O_RDONLY);
    expect(tmp >= 0 && read(tmp, buf, BLOCK) == BLOCK, "read of data-out.tmp");
    expect(close(tmp) == 0, "second close of data-out.tmp");

    /* A path of more than 1000 characters with ".tmp" in it. */
    char path[2048];
    int length = snprintf(path, sizeof path, "%s", argv[1]);
    for (int level = 0; level < 5; level++) {
        length += snprintf(path + length, sizeof path - length, "/work.tmp.%0200d", level);
        expect(mkdir(path, 0755) == 0, "mkdir of a long directory name");
    }
    snprintf(path + length, sizeof path - length, "/data-long");
    int deep = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(deep >= 0 && write(deep, buf, BLOCK) == BLOCK, "write to a file under a long path");
    expect(close(deep) == 0, "close of a file under a long path");

    return failures != 0;
}
EOF_C

cc -Wall -Wextra -Werror "$test_root/temp_names.c" -o "$test_root/temp_names"

for mode in 1 0; do
    mkdir -p "$test_root/traces-$mode" "$test_root/work-$mode"
    status=0
    { # the braces keep the shell's report of a crash out of the test's output
        timeout 20 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$test_root/traces-$mode" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT="$mode" \
            "$test_root/temp_names" "$test_root/work-$mode" \
            >"$test_root/output-$mode" 2>&1 || status=$?
    } 2>/dev/null
    if [[ $status -ne 0 ]]; then
        echo "a program using a file beside its temporary file failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    # Each file has its own statistics: 8 blocks written to data-out, 1 to
    # data-out.tmp, which is then read once.
    if [[ $mode -eq 1 ]]; then
        for expected in "data-out w 8 32768" "data-out.tmp w 1 4096" "data-out.tmp r 1 4096" "data-long w 1 4096"; do
            read -r name kind want <<<"$expected"
            trace=$(compgen -G "$test_root/traces-$mode/$name.[0-9]*.${kind}_blk_trace.json" | head -n 1 || true)
            if [[ -z $trace ]]; then
                echo "no $kind trace was written for $name" >&2
                exit 1
            fi
            if command -v python3 >/dev/null 2>&1; then
                got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"])' "$trace")
                if [[ $got != "$want" ]]; then
                    echo "access_frequency and data_volume in the $kind trace of $name: '$got', expected '$want'" >&2
                    exit 1
                fi
            fi
        done
    else
        for expected in "data-out w 8" "data-out.tmp w 1" "data-out.tmp r 1" "data-long w 1"; do
            read -r name kind want <<<"$expected"
            stat=$(compgen -G "$test_root/traces-$mode/${name}_[0-9]*_${kind}_stat" | head -n 1 || true)
            got=$(tail -n +2 "${stat:-/dev/null}" | wc -l)
            if [[ $got -ne $want ]]; then
                echo "${kind}_stat of $name lists $got blocks, expected $want" >&2
                exit 1
            fi
        done
    fi
done

echo "files with .tmp in their path are monitored as files of their own"