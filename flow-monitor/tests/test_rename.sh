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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-rename.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# fasterq-dump writes its output in temporary chunks, renames the first to the
# FASTQ file, then reads the others back and appends them to it. What a program
# writes under a temporary name must be reported under the name the file ends up
# with, whether the file is closed or still open when it is renamed, and the
# rename itself must be recorded.
cat >"$test_root/rename_files.c" <<'EOF'
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
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

/* Writes `blocks` blocks to a new file and closes it. */
static void write_file(const char *path, int blocks) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(fd >= 0, path);
    for (int i = 0; i < blocks; i++) {
        expect(write(fd, buf, BLOCK) == BLOCK, path);
    }
    expect(close(fd) == 0, path);
}

/* The file's size, with raw system calls: the check does not depend on the library. */
static long size_of(const char *path) {
    int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    long size = (long)syscall(SYS_lseek, fd, 0, SEEK_END);
    syscall(SYS_close, fd);
    return size;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    memset(buf, 'd', sizeof buf);

    /* As fasterq-dump: chunks written and closed, the first renamed to the result,
       the second read back and written after it. */
    write_file("data-chunk.0", 2);
    write_file("data-chunk.1", 2);
    expect(rename("data-chunk.0", "data-result") == 0, "rename of data-chunk.0");
    int result = open("data-result", O_WRONLY);
    int chunk = open("data-chunk.1", O_RDONLY);
    expect(result >= 0 && chunk >= 0, "open of data-result and data-chunk.1");
    for (int block = 2; block < 4; block++) {
        expect(read(chunk, buf, BLOCK) == BLOCK, "read back of data-chunk.1");
        expect(pwrite(result, buf, BLOCK, (off_t)block * BLOCK) == BLOCK, "write after the first chunk of data-result");
    }
    expect(close(chunk) == 0 && close(result) == 0, "close of data-chunk.1 and data-result");
    expect(size_of("data-result") == 4 * BLOCK, "data-result holds both chunks");

    /* Renamed while open, to a name no pattern matches, and written to afterwards. */
    int live = open("data-live.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(live >= 0 && write(live, buf, BLOCK) == BLOCK, "write to data-live.tmp");
#ifdef RENAME_NOREPLACE
    int renamed = renameat2(AT_FDCWD, "data-live.tmp", AT_FDCWD, "final-live", RENAME_NOREPLACE);
    if (renamed != 0 && (errno == EINVAL || errno == ENOSYS)) { /* not on this file system */
        renamed = rename("data-live.tmp", "final-live");
    }
#else /* a C library without renameat2() */
    int renamed = rename("data-live.tmp", "final-live");
#endif
    expect(renamed == 0, "rename of data-live.tmp while open");
    expect(write(live, buf, BLOCK) == BLOCK, "write after the rename");
    expect(close(live) == 0, "close of final-live");
    expect(size_of("final-live") == 2 * BLOCK, "final-live holds both writes");

    /* Renamed over a file the program read before: that file's record stays. */
    write_file("data-merge", 1);
    int old = open("data-merge", O_RDONLY);
    expect(old >= 0 && read(old, buf, BLOCK) == BLOCK && close(old) == 0, "read of the old data-merge");
    write_file("data-merge.new", 2);
    expect(renameat(AT_FDCWD, "data-merge.new", AT_FDCWD, "data-merge") == 0, "rename over data-merge");
    expect(size_of("data-merge") == 2 * BLOCK, "data-merge is the new file");

    /* Renamed over a file that is open: both keep their own records. */
    int busy = open("data-busy", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int busy_tmp = open("data-busy.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(busy >= 0 && busy_tmp >= 0, "open of data-busy and data-busy.tmp");
    expect(write(busy, buf, BLOCK) == BLOCK && write(busy_tmp, buf, BLOCK) == BLOCK, "first writes to data-busy and data-busy.tmp");
    expect(rename("data-busy.tmp", "data-busy") == 0, "rename over the open data-busy");
    expect(write(busy, buf, BLOCK) == BLOCK && write(busy_tmp, buf, BLOCK) == BLOCK, "writes after the rename over data-busy");
    expect(close(busy) == 0 && close(busy_tmp) == 0, "close of both data-busy files");

    /* A file the library did not monitor, given a name it would have. */
    write_file("plain.tmp", 1);
    expect(rename("plain.tmp", "data-late") == 0, "rename of plain.tmp");

    /* A name need not be UTF-8: these two renames are as good as any other. */
    write_file("data-latin", 1);
    expect(rename("data-latin", "data-caf\xe9") == 0, "rename of a monitored file to a name that is not UTF-8");
    write_file("plain-latin", 1);
    expect(rename("plain-latin", "data-na\xefve") == 0, "rename of another file to a name that is not UTF-8");

    /* Files that are none of the library's business. */
    write_file("other.txt", 1);
    expect(rename("other.txt", "other2.txt") == 0, "rename of other.txt");
    expect(rename("no-such-file", "data-nothing") == -1 && errno == ENOENT, "a failed rename reports its error");

    return failures != 0;
}
EOF

cc -Wall -Wextra -Werror "$test_root/rename_files.c" -o "$test_root/rename_files"

# The renames the timer must report, in order: "from to followed".
expected_renames="data-chunk.0 data-result 1
data-live.tmp final-live 1
data-merge.new data-merge 1
data-busy.tmp data-busy 0
plain.tmp data-late 0"

# Prints the renames in the timer's output, from the [MONITOR] lines on stdout
# (builds without TIMER_JSON) or the timer's JSON file. Not the two to names that
# are not UTF-8, which the two outputs spell differently.
timer_renames() {
    local mode=$1 json
    if grep -aq '^\[MONITOR\] task pid ' "$test_root/output-$mode"; then
        grep -a '^\[MONITOR\] rename ' "$test_root/output-$mode" | LC_ALL=C awk '$5 !~ /^data-(caf|na)/ {print $3, $5, $7}' || true
        return
    fi
    json=$(compgen -G "$test_root/traces-$mode/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
for rename in timer.get("renames", []):
    if not rename["to"].startswith(("data-caf", "data-na")):
        print(rename["from"], rename["to"], int(rename["followed"]))' "$json"
    else
        echo "$expected_renames" # nothing to read them with
    fi
}

# Files that must have no output of their own: their I/O is reported under their new name.
renamed_away=(data-chunk.0 data-live.tmp data-merge.new)

for mode in 1 0; do
    mkdir -p "$test_root/traces-$mode" "$test_root/work-$mode"
    status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/traces-$mode" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/rename_files" "$test_root/work-$mode" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "the program renaming its files failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    got=$(timer_renames "$mode")
    if [[ $got != "$expected_renames" ]]; then
        echo "renames reported by the timer (DATALIFE_JSON_OUTPUT=$mode):" >&2
        echo "$got" >&2
        echo "expected:" >&2
        echo "$expected_renames" >&2
        exit 1
    fi

    if [[ $mode -eq 1 ]] && ! compgen -G "$test_root/traces-$mode/data-caf*.w_blk_trace.json" >/dev/null; then
        echo "no trace was written for the file renamed to a name that is not UTF-8" >&2
        exit 1
    fi

    if [[ $mode -eq 1 ]]; then
        for name in "${renamed_away[@]}"; do
            if compgen -G "$test_root/traces-$mode/$name.[0-9]*_blk_trace.json" >/dev/null; then
                echo "a trace is left under $name, the name the file had before it was renamed" >&2
                exit 1
            fi
        done
        # name, kind, then access_frequency, data_volume and file_size_bytes:
        #   data-result   2 blocks as data-chunk.0 and 2 after the rename
        #   final-live    1 block as data-live.tmp and 1 after the rename, through the same descriptor
        #   data-merge    the old file read once; 2 blocks written as data-merge.new
        #   data-busy*    not followed: 2 blocks each, in two files of 2 blocks
        for expected in \
            "data-result w 4 16384 16384" \
            "data-chunk.1 w 2 8192 8192" \
            "data-chunk.1 r 2 8192 8192" \
            "final-live w 2 8192 8192" \
            "data-merge r 1 4096 4096" \
            "data-merge w 3 12288 8192" \
            "data-busy w 2 8192 8192" \
            "data-busy.tmp w 2 8192 8192"; do
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
print(d["access_frequency"], d["data_volume"], d["file_size_bytes"], d["file_name"])' "$trace")
                if [[ $got != "$want $name" ]]; then
                    echo "access_frequency, data_volume, file_size_bytes and file_name in the $kind trace of $name: '$got', expected '$want $name'" >&2
                    exit 1
                fi
            fi
        done
        # The size recorded for a renamed file is under its new name.
        json=$(compgen -G "$test_root/traces-$mode/monitor_timer.*.json" | head -n 1 || true)
        if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
            got=$(python3 -I -c '
import json, sys
files = next(iter(json.load(open(sys.argv[1])).values()))["files"]
print(" ".join("%s=%s/%s" % (name, files[name]["file_size_bytes"], files[name]["closes"]) if name in files else name + "=none"
               for name in sys.argv[2:]))' "$json" data-chunk.0 data-result data-live.tmp final-live data-merge.new data-merge)
            want="data-chunk.0=none data-result=16384/2 data-live.tmp=none final-live=8192/1 data-merge.new=none data-merge=8192/3"
            if [[ $got != "$want" ]]; then
                echo "sizes and closes recorded by the timer: '$got', expected '$want'" >&2
                exit 1
            fi
        fi
    else
        for name in "${renamed_away[@]}"; do
            if compgen -G "$test_root/traces-$mode/${name}_[0-9]*_stat" >/dev/null; then
                echo "statistics are left under $name, the name the file had before it was renamed" >&2
                exit 1
            fi
        done
        # name, file, then its rows: histograms hold "block accesses bytes", sequences the blocks in order.
        for expected in \
            "data-result w_stat 0 1 4096;1 1 4096;2 1 4096;3 1 4096;" \
            "data-result w_trace_stat 0;1;2;3;" \
            "final-live w_stat 0 1 4096;1 1 4096;" \
            "final-live w_trace_stat 0;1;" \
            "data-merge r_stat 0 1 4096;" \
            "data-merge w_stat 0 2 4096;1 1 4096;" \
            "data-merge w_trace_stat 0;0;1;" \
            "data-busy w_stat 0 1 4096;1 1 4096;" \
            "data-busy.tmp w_stat 0 1 4096;1 1 4096;"; do
            read -r name file want <<<"$expected"
            stat=$(compgen -G "$test_root/traces-$mode/${name}_[0-9]*_${file}" | head -n 1 || true)
            if [[ $file == *_trace_stat ]]; then
                got=$(tr '\n' ';' <"${stat:-/dev/null}")
            else
                got=$(tail -n +2 "${stat:-/dev/null}" | tr '\n' ';')
            fi
            if [[ $got != "$want" ]]; then
                echo "rows of the $file of $name: '$got', expected '$want'" >&2
                exit 1
            fi
        done
    fi
done

echo "I/O on a renamed file is reported under its new name"