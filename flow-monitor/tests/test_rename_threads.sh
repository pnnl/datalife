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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-rename-threads.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Threads that open, use and close monitored files while others rename those
# files onto one another. A file's name changes under the threads that have it
# open, and the name it had is at once that of another file: closing a file must
# release that file, not the one that now has its old name.
cat >"$test_root/rename_threads.c" <<'EOF_C'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static atomic_int stop;
static char block[4096];

/* Uses one of three names in turn, through a descriptor and through streams. */
static void *use_files(void *arg) {
    long id = (long)arg;
    char name[32];
    for (long n = 0; !stop; n++) {
        snprintf(name, sizeof name, "data-shared%ld", (id + n) % 3);
        int fd = open(name, O_RDWR | O_CREAT, 0644);
        if (fd >= 0) {
            if (write(fd, block, sizeof block) < 0 || pread(fd, block, 16, 0) < 0) {
                return (void *)1;
            }
            close(fd);
        }
        FILE *fp = fopen(name, "a");
        if (fp) {
            for (int i = 0; i < 5000; i++) {
                putc_unlocked('d', fp);
            }
            fflush(fp);
            fclose(fp);
        }
        fp = fopen(name, "r");
        if (fp) {
            for (int i = 0; i < 5000 && getc_unlocked(fp) != EOF; i++) {
            }
            fclose(fp);
        }
    }
    return NULL;
}

static void *rename_files(void *arg) {
    char from[32], to[32];
    (void)arg;
    for (long n = 0; !stop; n++) {
        snprintf(from, sizeof from, "data-shared%ld", n % 3);
        snprintf(to, sizeof to, "data-shared%ld", (n + 1) % 3);
        rename(from, to);
        if (n % 7 == 0) { /* and through a name no pattern matches */
            rename(to, "other-name");
            rename("other-name", from);
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    memset(block, 'd', sizeof block);
    pthread_t threads[5];
    for (long i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, use_files, (void *)i);
    }
    for (int i = 3; i < 5; i++) {
        pthread_create(&threads[i], NULL, rename_files, NULL);
    }
    sleep(3);
    stop = 1;
    int failed = 0;
    for (int i = 0; i < 5; i++) {
        void *result = NULL;
        pthread_join(threads[i], &result);
        failed |= result != NULL;
    }
    return failed;
}
EOF_C

cc -Wall -Wextra -Werror -O2 -pthread "$test_root/rename_threads.c" -o "$test_root/rename_threads"

for mode in 1 0; do
    mkdir -p "$test_root/traces-$mode" "$test_root/work-$mode"
    status=0
    { # the braces keep the shell's report of a crash out of the test's output
        timeout 60 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$test_root/traces-$mode" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT="$mode" \
            "$test_root/rename_threads" "$test_root/work-$mode" \
            >"$test_root/output-$mode" 2>&1 || status=$?
    } 2>/dev/null
    if [[ $status -ne 0 ]]; then
        echo "threads using files that other threads rename failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -av '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi
done

echo "files can be renamed while other threads open, use and close them"