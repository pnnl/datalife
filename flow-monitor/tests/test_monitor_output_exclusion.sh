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

# Whether a file is under the output directory is asked at every open, and the
# library does not resolve both paths anew each time: it keeps the output
# directory resolved, and the directory of the file a thread opened last. The
# answer must be the same however the file is reached and whatever was opened
# before it: by a relative name, through a link to the output directory, through
# a link to a file in it, and in the working directory again after each.
cat >"$test_root/write_each.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 3 || chdir(argv[1]) != 0) {
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        int fd = open(argv[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0 || write(fd, "payload\n", 8) != 8 || close(fd) != 0) {
            perror(argv[i]);
            return 1;
        }
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror "$test_root/write_each.c" -o "$test_root/write_each"

run_reached_case() {
    local mode=$1
    local traces="$test_root/traces-reached-$mode"
    local work="$test_root/work-reached-$mode"
    mkdir -p "$traces" "$work/inner"
    ln -s "$traces" "$work/link-to-output"
    : >"$traces/sifting-link-target"
    ln -s "$traces/sifting-link-target" "$work/sifting-file-link"

    timeout 10 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='sift*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/write_each" "$work" \
        sifting-a.py \
        sifting-b.py \
        link-to-output/sifting-through-link \
        sifting-c.py \
        "../traces-reached-$mode/sifting-relative" \
        inner/sifting-d.py \
        sifting-file-link \
        "$work/./sifting-e.py" \
        "$traces/sifting-direct" \
        inner/../sifting-f.py

    mapfile -t generated < <(find "$traces" -maxdepth 1 -type f -printf '%f\n' | sort)
    for name in through-link relative link-target file-link direct; do
        if printf '%s\n' "${generated[@]}" | grep -Eq "^sifting-$name.*(blk_trace\.json|trace_stat)$"; then
            echo "sifting-$name is under DATALIFE_OUTPUT_PATH and was monitored (DATALIFE_JSON_OUTPUT=$mode)" >&2
            exit 1
        fi
    done
    for name in a b c d e f; do
        if ! printf '%s\n' "${generated[@]}" | grep -Eq "^sifting-$name\.py[._].*(w_blk_trace\.json|_w_trace_stat)$"; then
            echo "sifting-$name.py is not under DATALIFE_OUTPUT_PATH and was not monitored (DATALIFE_JSON_OUTPUT=$mode):" >&2
            printf '  %s\n' "${generated[@]}" >&2
            exit 1
        fi
    done
}

run_reached_case 1
run_reached_case 0

# What a thread keeps of the directories it has resolved goes away at the
# thread's exit, and a program may still open files after that: from a
# destructor of thread-specific data, as a logging or cleanup hook does. Those
# opens must work as any other.
cat >"$test_root/open_at_thread_exit.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_key_t key;
static int failed;

static void open_in(const char *directory, const char *name) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", directory, name);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || write(fd, "payload\n", 8) != 8 || close(fd) != 0) {
        failed = 1;
    }
}

static void at_thread_exit(void *arg) {
    (void)arg;
    open_in("first", "plain-at-exit");
    open_in("third", "plain-at-exit");
    open_in("third", "sifting-at-exit.py");
}

static void *work(void *arg) {
    pthread_setspecific(key, arg);
    open_in("first", "plain-in-thread");
    open_in("second", "plain-in-thread");
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0 || pthread_key_create(&key, at_thread_exit) != 0) {
        return 2;
    }
    for (int round = 0; round < 20; round++) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, work, &key) != 0 || pthread_join(thread, NULL) != 0) {
            return 2;
        }
    }
    return failed;
}
EOF

cc -Wall -Wextra -Werror -pthread "$test_root/open_at_thread_exit.c" -o "$test_root/open_at_thread_exit"

for mode in 1 0; do
    traces="$test_root/traces-exit-$mode"
    work="$test_root/work-exit-$mode"
    mkdir -p "$traces" "$work/first" "$work/second" "$work/third"
    status=0
    timeout 30 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='sift*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/open_at_thread_exit" "$work" >"$test_root/output-exit-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "a program that opens files at the exit of its threads ended with status $status (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if ! find "$traces" -maxdepth 1 -type f -printf '%f\n' | grep -Eq '^sifting-at-exit\.py[._].*(w_blk_trace\.json|_w_trace_stat)$'; then
        echo "a file opened at the exit of a thread was not monitored (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
done

echo "monitor-owned output excluded from instrumentation"