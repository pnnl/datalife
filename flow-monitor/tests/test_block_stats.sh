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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-block-stats.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Block statistics of accesses with known block patterns. Blocks are 4096 bytes;
# an access touches the blocks holding its first and last bytes and those between.
# Each case is one open-access-close session. Files are prepared with raw system
# calls, so only that session is monitored.
cat >"$test_root/block_truth.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BLOCK 4096

static char buf[1 << 20];

/* Writes blocks * BLOCK bytes to data-truth without the library seeing it. */
static int prepare(int blocks) {
    int fd = (int)syscall(SYS_openat, AT_FDCWD, "data-truth", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    memset(buf, 'd', sizeof buf);
    for (int b = 0; b < blocks; b++) {
        if (syscall(SYS_write, fd, buf, BLOCK) != BLOCK) {
            return 1;
        }
    }
    return (int)syscall(SYS_close, fd);
}

int main(int argc, char **argv) {
    if (argc != 3 || chdir(argv[1]) != 0) {
        return 2;
    }
    const char *pattern = argv[2];
    int fd;
    FILE *stream;
    if (strcmp(pattern, "aligned4") == 0) { /* 4 reads of one block each */
        if (prepare(4) != 0 || (fd = open("data-truth", O_RDONLY)) < 0) return 1;
        for (int i = 0; i < 4; i++) if (read(fd, buf, BLOCK) != BLOCK) return 1;
        return close(fd);
    }
    if (strcmp(pattern, "half") == 0) { /* 2 reads of half a block, both in block 0 */
        if (prepare(1) != 0 || (fd = open("data-truth", O_RDONLY)) < 0) return 1;
        for (int i = 0; i < 2; i++) if (read(fd, buf, BLOCK / 2) != BLOCK / 2) return 1;
        return close(fd);
    }
    if (strcmp(pattern, "big") == 0) { /* 1 read of 256 blocks */
        if (prepare(256) != 0 || (fd = open("data-truth", O_RDONLY)) < 0) return 1;
        if (read(fd, buf, sizeof buf) != (ssize_t)sizeof buf) return 1;
        return close(fd);
    }
    if (strcmp(pattern, "pread") == 0) { /* block 2, then block 0 */
        if (prepare(4) != 0 || (fd = open("data-truth", O_RDONLY)) < 0) return 1;
        if (pread(fd, buf, BLOCK, 2 * BLOCK) != BLOCK || pread(fd, buf, BLOCK, 0) != BLOCK) return 1;
        return close(fd);
    }
    if (strcmp(pattern, "write4") == 0) { /* 4 writes of one block each */
        if ((fd = open("data-truth", O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) return 1;
        memset(buf, 'w', sizeof buf);
        for (int i = 0; i < 4; i++) if (write(fd, buf, BLOCK) != BLOCK) return 1;
        return close(fd);
    }
    if (strcmp(pattern, "fread4") == 0) { /* 4 stream reads of one block each */
        if (prepare(4) != 0 || !(stream = fopen("data-truth", "r"))) return 1;
        for (int i = 0; i < 4; i++) if (fread(buf, 1, BLOCK, stream) != BLOCK) return 1;
        return fclose(stream);
    }
    if (strcmp(pattern, "fwrite4") == 0) { /* 4 stream writes of one block each */
        if (!(stream = fopen("data-truth", "w"))) return 1;
        memset(buf, 'w', sizeof buf);
        for (int i = 0; i < 4; i++) if (fwrite(buf, 1, BLOCK, stream) != BLOCK) return 1;
        return fclose(stream);
    }
    return 2;
}
EOF

cc -Wall -Wextra -Werror "$test_root/block_truth.c" -o "$test_root/block_truth"

json=0
if command -v python3 >/dev/null 2>&1; then
    json=1
else
    echo "python3 not found; the JSON traces are not checked" >&2
fi

# Expected results per pattern: access kind, histogram rows "block frequency bytes",
# access sequence, total touches, total bytes, and the access pattern (both layouts).
declare -A kind rows seq touches bytes range pairs
kind[aligned4]=r rows[aligned4]="0 1 4096;1 1 4096;2 1 4096;3 1 4096;" seq[aligned4]="0;1;2;3;"
touches[aligned4]=4 bytes[aligned4]=16384 range[aligned4]="[0, 3, 3, -1]" pairs[aligned4]="[0, 0, 1, 1, 2, 2, 3, 3]"
kind[half]=r rows[half]="0 2 2048;" seq[half]="0;0;"
touches[half]=2 bytes[half]=4096 range[half]="[0, 0, 0, -1]" pairs[half]="[0, 0, 0, 0]"
kind[big]=r rows[big]=$(for ((b = 0; b < 256; b++)); do printf '%d 1 4096;' "$b"; done) seq[big]=$(for ((b = 0; b < 256; b++)); do printf '%d;' "$b"; done)
touches[big]=256 bytes[big]=1048576 range[big]="[0, 255, 255, -1]" pairs[big]="[0, 255]"
kind[pread]=r rows[pread]="0 1 4096;2 1 4096;" seq[pread]="2;0;"
touches[pread]=2 bytes[pread]=8192 range[pread]="[2, 0, 2, -2]" pairs[pread]="[2, 2, 0, 0]"
kind[write4]=w rows[write4]=${rows[aligned4]} seq[write4]=${seq[aligned4]}
touches[write4]=4 bytes[write4]=16384 range[write4]=${range[aligned4]} pairs[write4]=${pairs[aligned4]}
kind[fread4]=r rows[fread4]=${rows[aligned4]} seq[fread4]=${seq[aligned4]}
touches[fread4]=4 bytes[fread4]=16384 range[fread4]=${range[aligned4]} pairs[fread4]=${pairs[aligned4]}
kind[fwrite4]=w rows[fwrite4]=${rows[aligned4]} seq[fwrite4]=${seq[aligned4]}
touches[fwrite4]=4 bytes[fwrite4]=16384 range[fwrite4]=${range[aligned4]} pairs[fwrite4]=${pairs[aligned4]}

fail() {
    echo "$1" >&2
    exit 1
}

for mode in 1 0; do
    for pattern in aligned4 half big pread write4 fread4 fwrite4; do
        traces="$test_root/traces-$pattern-$mode"
        work="$test_root/work-$pattern-$mode"
        mkdir -p "$traces" "$work"
        status=0
        timeout 20 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$traces" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT="$mode" \
            "$test_root/block_truth" "$work" "$pattern" \
            >"$test_root/output" 2>&1 || status=$?
        if [[ $status -ne 0 ]]; then
            grep -v '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
            fail "pattern $pattern failed with status $status (DATALIFE_JSON_OUTPUT=$mode)"
        fi
        k=${kind[$pattern]}
        if [[ $mode -eq 0 ]]; then
            stat=$(compgen -G "$traces/data-truth_*_${k}_stat" | head -n 1 || true)
            trace=$(compgen -G "$traces/data-truth_*_${k}_trace_stat" | head -n 1 || true)
            [[ -n $stat && -n $trace ]] || fail "pattern $pattern: no ${k}_stat or ${k}_trace_stat written"
            got=$(tail -n +2 "$stat" | tr '\n' ';')
            [[ $got == "${rows[$pattern]}" ]] || fail "pattern $pattern: ${k}_stat rows are '$got', expected '${rows[$pattern]}'"
            got=$(tr '\n' ';' <"$trace")
            [[ $got == "${seq[$pattern]}" ]] || fail "pattern $pattern: ${k}_trace_stat is '$got', expected '${seq[$pattern]}'"
        elif [[ $json -eq 1 ]]; then
            trace=$(compgen -G "$traces/data-truth.*.${k}_blk_trace.json" | head -n 1 || true)
            [[ -n $trace ]] || fail "pattern $pattern: no ${k}_blk_trace.json written"
            got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"], d["io_blk_range"])' "$trace")
            want="${touches[$pattern]} ${bytes[$pattern]} ${range[$pattern]}"
            want_pairs="${touches[$pattern]} ${bytes[$pattern]} ${pairs[$pattern]}"
            [[ $got == "$want" || $got == "$want_pairs" ]] ||
                fail "pattern $pattern: access_frequency, data_volume, io_blk_range are '$got', expected '$want' (or '$want_pairs' with BLK_IDX)"
        fi
    done
done

echo "block statistics match the accesses made"
