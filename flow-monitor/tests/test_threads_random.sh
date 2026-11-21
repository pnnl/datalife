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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-threads-random.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Four threads, each doing a random sequence of what programs do with files at
# once: streams of their own, with the unlocked macros and the calls that have
# wrappers; temporary files renamed to results while open or closed; one stream
# they all write to and one they all read from, some locking it and using the
# macros while others use calls that lock it themselves; and files they all open
# under three names while renaming them onto one another. Whatever the order the
# threads run in, the program must finish, and the library must have recorded
# exactly the bytes the threads read and wrote between them.
#
# Not here: a call with a wrapper made on a stream the thread has locked with
# flockfile(), while another thread uses the same stream. The two threads block
# each other, with or without this test.
cat >"$test_root/threads_random.c" <<'EOF_C'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK 4096
#define THREADS 4

/* What the timer must show in the end: every thread adds what it read and wrote. */
static atomic_long bytes_read, bytes_written;
static atomic_int failed;

static FILE *shared_out, *shared_in;
static unsigned long long seed;

struct rng {
    unsigned long long state;
};

static unsigned pick(struct rng *r, unsigned n) {
    r->state = r->state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(r->state >> 33) % n;
}

/* A stream of the thread's own, used with the macros and with calls that have a wrapper. */
static void own_stream(struct rng *r, long id, char *buf) {
    char name[32];
    snprintf(name, sizeof name, "data-own%ld", id);
    static const char *modes[] = {"w+", "a+", "r+"};
    FILE *fp = fopen(name, modes[pick(r, 3)]);
    if (!fp) {
        fp = fopen(name, "w+");
    }
    if (!fp) {
        failed = 1;
        return;
    }
    int last = 0;
    for (int i = 0, n = 1 + pick(r, 8); i < n; i++) {
        int what = pick(r, 6);
        if (what < 2) {
            if (last == 1) fseek(fp, 0, SEEK_CUR);
            if (what == 0) {
                int count = 1 + pick(r, 3000), put = 0;
                for (; put < count && putc_unlocked('o', fp) != EOF; put++) {
                }
                bytes_written += put;
            } else {
                bytes_written += fwrite(buf, 1, 1 + pick(r, 6000), fp);
            }
            last = 2;
        } else if (what < 4) {
            if (last == 2) fseek(fp, 0, SEEK_CUR);
            if (what == 2) {
                int count = 1 + pick(r, 3000), got = 0;
                for (; got < count && getc_unlocked(fp) != EOF; got++) {
                }
                bytes_read += got;
            } else {
                bytes_read += fread(buf, 1, 1 + pick(r, 6000), fp);
            }
            last = 1;
        } else if (what == 4) {
            fseek(fp, pick(r, 20000), SEEK_SET);
            last = 0;
        } else {
            fflush(fp);
            if (last == 2) last = 0;
        }
    }
    fclose(fp);
}

/* A temporary file that becomes a result, renamed before or after it is closed, then read back. */
static void own_rename(struct rng *r, long id, char *buf) {
    char tmp[32], final[32];
    snprintf(tmp, sizeof tmp, "data-own%ld.tmp", id);
    snprintf(final, sizeof final, pick(r, 4) ? "data-result%ld" : "result%ld", id);
    int fd = open(tmp, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        failed = 1;
        return;
    }
    int blocks = 1 + pick(r, 4), early = pick(r, 2);
    for (int b = 0; b < blocks; b++) {
        if (b == 1 && early && rename(tmp, final) != 0) failed = 1;
        ssize_t n = pwrite(fd, buf, BLOCK, (off_t)b * BLOCK);
        if (n > 0) bytes_written += n;
    }
    if (blocks == 1 && early && rename(tmp, final) != 0) failed = 1;
    close(fd);
    if (!early && rename(tmp, final) != 0) failed = 1;
    if (strncmp(final, "data-", 5) == 0) { /* a name the library monitors */
        fd = open(final, O_RDONLY);
        ssize_t n = fd >= 0 ? pread(fd, buf, BLOCK, 0) : -1;
        if (n > 0) bytes_read += n;
        if (fd >= 0) close(fd);
    }
}

/* Files that all threads open under three names, while they rename them onto one another. */
static void ring(struct rng *r, char *buf) {
    char a[32], b[32];
    snprintf(a, sizeof a, "data-ring%u", pick(r, 3));
    if (pick(r, 3) == 0) {
        snprintf(b, sizeof b, pick(r, 5) ? "data-ring%u" : "ring-elsewhere", pick(r, 3));
        rename(a, b);
        if (strcmp(b, "ring-elsewhere") == 0 && pick(r, 2)) {
            rename(b, a);
        }
        return;
    }
    int fd = open(a, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        return;
    }
    ssize_t n = pwrite(fd, buf, BLOCK, (off_t)pick(r, 4) * BLOCK);
    if (n > 0) bytes_written += n;
    n = pread(fd, buf, 16, 0);
    if (n > 0) bytes_read += n;
    close(fd);
}

static void *work(void *arg) {
    long id = (long)arg;
    struct rng r = {seed * 2654435761ULL + 977 * (id + 1)};
    static _Thread_local char buf[8192];
    memset(buf, 'a' + (int)id, sizeof buf);
    for (int i = 0, n = 150 + pick(&r, 150); i < n && !failed; i++) {
        int what = pick(&r, 100);
        if (what < 15) {
            own_stream(&r, id, buf);
        } else if (what < 30) {
            own_rename(&r, id, buf);
        } else if (what < 50) { /* the shared stream, locked, with the macros */
            int count = 1 + pick(&r, 6000), put = 0;
            flockfile(shared_out);
            for (; put < count && putc_unlocked('s', shared_out) != EOF; put++) {
            }
            if (pick(&r, 3) == 0) fflush(shared_out);
            funlockfile(shared_out);
            bytes_written += put;
        } else if (what < 62) { /* the shared stream, with calls that lock it themselves */
            switch (pick(&r, 3)) {
            case 0: bytes_written += fwrite(buf, 1, 1 + pick(&r, 6000), shared_out); break;
            case 1: { const char *volatile line = "a line through fputs\n"; fputs(line, shared_out); break; } /* not in the timer's bytes; volatile, or the compiler calls fwrite() */
            default: fflush(shared_out); break;
            }
        } else if (what < 75) { /* the shared input, locked, with the macros */
            int count = 1 + pick(&r, 6000), got = 0;
            flockfile(shared_in);
            for (; got < count && getc_unlocked(shared_in) != EOF; got++) {
            }
            funlockfile(shared_in);
            bytes_read += got;
        } else if (what < 85) {
            if (pick(&r, 4)) {
                bytes_read += fread(buf, 1, 1 + pick(&r, 6000), shared_in);
            } else {
                rewind(shared_in);
            }
        } else {
            ring(&r, buf);
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 4 || chdir(argv[2]) != 0) {
        return 2;
    }
    seed = strtoull(argv[1], NULL, 10);
    static char fill[BLOCK];
    memset(fill, 'i', sizeof fill);
    int fd = open("data-shared-in", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (int b = 0; b < 64; b++) {
        ssize_t n = write(fd, fill, sizeof fill);
        if (n > 0) bytes_written += n;
    }
    close(fd);
    shared_in = fopen("data-shared-in", "r");
    shared_out = fopen("data-shared-out", "a");
    if (!shared_in || !shared_out) {
        return 3;
    }
    pthread_t threads[THREADS];
    for (long i = 0; i < THREADS; i++) {
        pthread_create(&threads[i], NULL, work, (void *)i);
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    fclose(shared_in);
    fclose(shared_out);
    FILE *totals = fopen(argv[3], "w");
    if (!totals) {
        return 3;
    }
    fprintf(totals, "%ld %ld\n", (long)bytes_read, (long)bytes_written);
    fclose(totals);
    return failed;
}
EOF_C

# Optimized, for the macros; not fortified, as in test_stream_inline_io.sh.
cc -Wall -Wextra -Werror -O2 -U_FORTIFY_SOURCE -pthread "$test_root/threads_random.c" -o "$test_root/threads_random"

# Prints "read written", the bytes of the timer's monitor read and write entries,
# from the [MONITOR] lines on stdout (builds without TIMER_JSON) or the timer's
# JSON file.
timer_bytes() {
    local lines json
    lines=$(grep -a -E '^\[MONITOR\] monitor (read|write) ' "$test_root/output" | head -n 2 || true)
    if [[ -n $lines ]]; then
        echo "$lines" | awk '{bytes[$3] = $6} END {print bytes["read"], bytes["write"]}'
        return
    fi
    json=$(compgen -G "$test_root/traces/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
print(timer["monitor"]["read"][2], timer["monitor"]["write"][2])' "$json"
    fi
}

for seed in $(seq 1 40); do
    mode=$((seed % 2))
    rm -rf "$test_root/work" "$test_root/traces"
    mkdir -p "$test_root/work" "$test_root/traces"
    status=0
    { # the braces keep the shell's report of a crash out of the test's output
        timeout 60 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$test_root/traces" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT="$mode" \
            "$test_root/threads_random" "$seed" "$test_root/work" "$test_root/totals" \
            >"$test_root/output" 2>&1 || status=$?
    } 2>/dev/null
    if [[ $status -eq 124 ]]; then
        echo "sequence $seed did not finish: threads block each other (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if [[ $status -ne 0 ]]; then
        echo "sequence $seed failed with status $status (DATALIFE_JSON_OUTPUT=$mode)" >&2
        grep -av '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
        exit 1
    fi
    want=$(cat "$test_root/totals")
    got=$(timer_bytes)
    if [[ -n $got && $got != "$want" ]]; then
        echo "sequence $seed: the library recorded '$got' bytes (read written), the threads read and wrote '$want' (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
done

echo "random sequences of operations on several threads are recorded byte for byte"