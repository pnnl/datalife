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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-rename-random.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Random sequences of opens, whole blocks written and read, closes, renames and
# unlinks over a few names, through descriptors and streams. The program keeps
# the books the library should keep: what was written and read under each name,
# moved to the new name by every rename the library follows (the file was
# monitored, and no monitored file is open under the new name), whether the file
# is open or closed, to a name that matches a pattern or not, onto a name with a
# record of its own or not. In the end the library must report exactly that, for
# each block of each file, and list each rename with whether it followed it.
cat >"$test_root/rename_random.c" <<'EOF_C'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK 4096
#define BLOCKS 8
#define NAMES 6
#define HANDLES 6

static unsigned long long state;

static unsigned pick(unsigned n) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(state >> 33) % n;
}

/* The first four match the pattern data-*, the last two do not. */
static const char *names[NAMES] = {"data-a", "data-b", "data-c", "data-d.tmp", "plain-x", "plain-y"};

static int matches(int name) {
    return name < 4;
}

/* What the library must report under each name: how often each block was written
   and read. known: the library has something under the name, if only an empty
   record, because a monitored file was opened under it or renamed to it. */
static struct record {
    int known;
    long written[BLOCKS], read[BLOCKS];
} records[NAMES];

/* A monitored file that is open: the library has one object for it, whatever the
   number of descriptors, and the object goes by one name. */
static struct object {
    int users, name;
} objects[1000];
static int object_count;
static int open_object[NAMES]; /* the object that goes by each name, or -1 */

static struct handle {
    int used, fd, object; /* object: -1 if the library does not monitor it */
    FILE *fp;
} handles[HANDLES];

static char block[BLOCK];
static FILE *expected;

static void move_record(int from, int to) {
    for (int b = 0; b < BLOCKS; b++) {
        records[to].written[b] += records[from].written[b];
        records[to].read[b] += records[from].read[b];
    }
    memset(&records[from], 0, sizeof records[from]);
    records[to].known = 1;
}

/* After rename(from, to) succeeded: what the library must have done. */
static void renamed(int from, int to) {
    int followed = 0;
    int moving = open_object[from];
    if (open_object[to] < 0) { /* never onto a monitored file that is open */
        if (moving >= 0) {
            objects[moving].name = to;
            open_object[to] = moving;
            open_object[from] = -1;
            followed = 1;
        } else {
            followed = records[from].known;
        }
        if (followed) {
            move_record(from, to);
        }
    }
    if (followed || matches(from) || matches(to)) {
        fprintf(expected, "rename %s %s %d\n", names[from], names[to], followed);
    }
}

int main(int argc, char **argv) {
    if (argc != 4 || chdir(argv[2]) != 0) {
        return 2;
    }
    state = strtoull(argv[1], NULL, 10) * 2654435761ULL + 12345;
    expected = fopen(argv[3], "w");
    if (!expected) {
        return 3;
    }
    memset(block, 'd', sizeof block);
    for (int n = 0; n < NAMES; n++) {
        open_object[n] = -1;
    }

    int operations = 30 + pick(170);
    for (int i = 0; i < operations; i++) {
        int what = pick(100);
        struct handle *h = &handles[pick(HANDLES)];
        if (what < 25) { /* open, through a descriptor or a stream */
            if (h->used) {
                continue;
            }
            int name = pick(NAMES);
            if (pick(3) == 0) {
                h->fp = fopen(names[name], "r+");
                if (!h->fp) {
                    h->fp = fopen(names[name], "w+");
                }
                if (!h->fp) {
                    return 3;
                }
                h->fd = -1;
            } else {
                h->fd = open(names[name], O_RDWR | O_CREAT, 0644);
                if (h->fd < 0) {
                    return 3;
                }
                h->fp = NULL;
            }
            h->used = 1;
            h->object = -1;
            if (matches(name)) {
                if (open_object[name] < 0) {
                    objects[object_count].users = 0;
                    objects[object_count].name = name;
                    open_object[name] = object_count++;
                }
                h->object = open_object[name];
                objects[h->object].users++;
                records[name].known = 1;
            }
        } else if (what < 65) { /* a whole block, written or read */
            if (!h->used) {
                continue;
            }
            int b = pick(BLOCKS), writing = what < 50;
            off_t offset = (off_t)b * BLOCK;
            ssize_t done;
            if (h->fp) {
                if (fseeko(h->fp, offset, SEEK_SET) != 0) {
                    return 3;
                }
                done = writing ? (ssize_t)fwrite(block, 1, BLOCK, h->fp) : (ssize_t)fread(block, 1, BLOCK, h->fp);
                if (writing && fflush(h->fp) != 0) {
                    return 3;
                }
            } else {
                done = writing ? pwrite(h->fd, block, BLOCK, offset) : pread(h->fd, block, BLOCK, offset);
            }
            if (done != 0 && done != BLOCK) {
                return 3; /* the files hold whole blocks only */
            }
            if (done == BLOCK && h->object >= 0) {
                struct record *r = &records[objects[h->object].name];
                (writing ? r->written : r->read)[b]++;
            }
        } else if (what < 80) { /* close */
            if (!h->used) {
                continue;
            }
            if ((h->fp ? fclose(h->fp) : close(h->fd)) != 0) {
                return 3;
            }
            h->used = 0;
            if (h->object >= 0 && --objects[h->object].users == 0) {
                open_object[objects[h->object].name] = -1;
            }
        } else if (what < 97) { /* rename, whether or not either file is open */
            int from = pick(NAMES), to = pick(NAMES);
            if (from == to) {
                continue;
            }
            int result;
            switch (pick(3)) {
            case 0: result = rename(names[from], names[to]); break;
            case 1: result = renameat(AT_FDCWD, names[from], AT_FDCWD, names[to]); break;
            default:
#ifdef RENAME_NOREPLACE
                result = renameat2(AT_FDCWD, names[from], AT_FDCWD, names[to], 0);
#else
                result = rename(names[from], names[to]);
#endif
                break;
            }
            if (result == 0) {
                renamed(from, to);
            }
        } else {
            unlink(names[pick(NAMES)]);
        }
    }
    for (int i = 0; i < HANDLES; i++) {
        if (handles[i].used && (handles[i].fp ? fclose(handles[i].fp) : close(handles[i].fd)) != 0) {
            return 3;
        }
    }

    for (int n = 0; n < NAMES; n++) {
        for (int kind = 0; kind < 2; kind++) {
            long *counts = kind ? records[n].read : records[n].written;
            long total = 0;
            for (int b = 0; b < BLOCKS; b++) {
                total += counts[b];
            }
            if (total == 0) {
                continue;
            }
            fprintf(expected, "file %s %c", names[n], kind ? 'r' : 'w');
            for (int b = 0; b < BLOCKS; b++) {
                if (counts[b]) {
                    fprintf(expected, " %d:%ld", b, counts[b]);
                }
            }
            fprintf(expected, "\n");
        }
    }
    return fclose(expected) != 0;
}
EOF_C

cc -Wall -Wextra -Werror "$test_root/rename_random.c" -o "$test_root/rename_random"

# Prints the renames in the timer's output as "rename from to followed", from the
# [MONITOR] lines on stdout (builds without TIMER_JSON) or the timer's JSON file.
timer_renames() {
    local json
    if grep -aq '^\[MONITOR\] task pid ' "$test_root/output"; then
        grep -a '^\[MONITOR\] rename ' "$test_root/output" | awk '{print "rename", $3, $5, $7}' || true
        return
    fi
    json=$(compgen -G "$test_root/traces/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
for rename in timer.get("renames", []):
    print("rename", rename["from"], rename["to"], int(rename["followed"]))' "$json"
    else
        grep '^rename ' "$test_root/expected" || true # nothing to read them with
    fi
}

# Prints what the library reports for each file as "file name kind block:count ...",
# like the program, from the histograms of the legacy output; or, from the JSON
# output, which has no counts for each block, "file name kind accesses bytes".
reported_files() {
    local mode=$1 path name kind
    if [[ $mode -eq 0 ]]; then
        for path in "$test_root"/traces/*_[rw]_stat; do
            [[ -f $path ]] || continue
            name=$(basename "$path" | sed -E 's/_[0-9]+_[rw]_stat$//')
            kind=$(basename "$path" | sed -E 's/.*_([rw])_stat$/\1/')
            # A histogram is written for every file closed: one without rows is no report.
            tail -n +2 "$path" | awk -v name="$name" -v kind="$kind" '
                {row = row " " $1 ":" $2; if ($3 != 4096) row = row "(" $3 " bytes)"}
                END {if (row != "") print "file", name, kind row}'
        done
    else
        for path in "$test_root"/traces/*_blk_trace.json; do
            [[ -f $path ]] || continue
            name=$(basename "$path" | sed -E 's/\.[0-9]+-.*\.[rw]_blk_trace\.json$//')
            kind=$(basename "$path" | sed -E 's/.*\.([rw])_blk_trace\.json$/\1/')
            python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print("file", sys.argv[2], sys.argv[3], d["access_frequency"], d["data_volume"])' "$path" "$name" "$kind"
        done
    fi
}

# The program's books in the form reported_files prints.
expected_files() {
    local mode=$1
    if [[ $mode -eq 0 ]]; then
        grep '^file ' "$test_root/expected" || true
    else
        grep '^file ' "$test_root/expected" | awk '{
            accesses = 0
            for (i = 4; i <= NF; i++) { split($i, count, ":"); accesses += count[2] }
            print $1, $2, $3, accesses, accesses * 4096
        }' || true
    fi
}

# Shows how two lists differ.
differences() {
    diff <(echo "$1") <(echo "$2") | sed -n 's/^< /  expected: /p; s/^> /  reported: /p' >&2 || true
}

for seed in $(seq 1 60); do
    for mode in 1 0; do
        if [[ $mode -eq 1 ]] && ! command -v python3 >/dev/null 2>&1; then
            continue # nothing to read the JSON traces with
        fi
        rm -rf "$test_root/work" "$test_root/traces"
        mkdir -p "$test_root/work" "$test_root/traces"
        status=0
        timeout 20 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$test_root/traces" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT="$mode" \
            "$test_root/rename_random" "$seed" "$test_root/work" "$test_root/expected" \
            >"$test_root/output" 2>&1 || status=$?
        if [[ $status -ne 0 ]]; then
            echo "sequence $seed failed with status $status (DATALIFE_JSON_OUTPUT=$mode)" >&2
            grep -av '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
            exit 1
        fi
        want=$(expected_files "$mode" | sort)
        got=$(reported_files "$mode" | sort)
        if [[ $got != "$want" ]]; then
            echo "sequence $seed: the files reported are not those the program used (DATALIFE_JSON_OUTPUT=$mode)" >&2
            differences "$want" "$got"
            exit 1
        fi
        want=$(grep '^rename ' "$test_root/expected" || true)
        got=$(timer_renames)
        if [[ $got != "$want" ]]; then
            echo "sequence $seed: the renames reported are not those the program made (DATALIFE_JSON_OUTPUT=$mode)" >&2
            differences "$want" "$got"
            exit 1
        fi
    done
done

echo "random sequences of renames leave every file's I/O under the name it ends up with"