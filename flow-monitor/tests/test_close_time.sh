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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-close-time.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# The library writes a file's traces from inside the program's close() of the
# file. That time is the library's, not the call's: the timer must not report it
# as time the program spent in close(), nor count it as I/O time, and reports it
# as `monitor trace_output` instead.
#
# Writing the traces takes a moment that a test cannot tell from the close
# itself, so a library preloaded after the monitor makes it take long: every
# open of a file under the output directory waits a quarter of a second. The
# monitor's own opens reach it, as they reach the C library.
cat >"$test_root/slow_output.c" <<'EOF'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void wait_if_output(const char *path) {
    const char *directory = getenv("SLOW_DIRECTORY");
    if (path && directory && strncmp(path, directory, strlen(directory)) == 0) {
        struct timespec quarter = {0, 250000000};
        nanosleep(&quarter, NULL);
    }
}

FILE *fopen(const char *path, const char *mode) {
    static FILE *(*real)(const char *, const char *);
    if (!real) real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen");
    wait_if_output(path);
    return real(path, mode);
}

FILE *fopen64(const char *path, const char *mode) {
    static FILE *(*real)(const char *, const char *);
    if (!real) real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen64");
    wait_if_output(path);
    return real(path, mode);
}

int open(const char *path, int flags, ...) {
    static int (*real)(const char *, int, ...);
    if (!real) real = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    va_list arguments;
    va_start(arguments, flags);
    int mode = va_arg(arguments, int);
    va_end(arguments);
    wait_if_output(path);
    return real(path, flags, mode);
}

int open64(const char *path, int flags, ...) {
    static int (*real)(const char *, int, ...);
    if (!real) real = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open64");
    va_list arguments;
    va_start(arguments, flags);
    int mode = va_arg(arguments, int);
    va_end(arguments);
    wait_if_output(path);
    return real(path, flags, mode);
}
EOF

# Two monitored files, one closed with close() and one with fclose(). The
# program tells how long the two calls took: with the library they take as long
# as the traces take to write.
cat >"$test_root/close_files.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    int fd = open("data-descriptor", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE *fp = fopen("data-stream", "w");
    if (fd < 0 || !fp || write(fd, "payload\n", 8) != 8 || fputs("payload\n", fp) == EOF) {
        return 1;
    }
    double start = now();
    if (close(fd) != 0 || fclose(fp) != 0) {
        return 1;
    }
    FILE *waited = fopen("waited", "w");
    if (!waited || fprintf(waited, "%.3f\n", now() - start) < 0 || fclose(waited) != 0) {
        return 1;
    }
    return 0;
}
EOF

cc -Wall -Wextra -Werror -shared -fPIC "$test_root/slow_output.c" -o "$test_root/libslow_output.so" -ldl
cc -Wall -Wextra -Werror "$test_root/close_files.c" -o "$test_root/close_files"

# Prints the time and the calls of one timer entry, from the [MONITOR] lines on
# stdout (builds without TIMER_JSON) or the timer's JSON file.
timer_entry() {
    local mode=$1 type=$2 call=$3
    local line json
    line=$(grep "^\[MONITOR\] $type $call " "$test_root/output-$mode" | head -n 1 || true)
    if [[ -n $line ]]; then
        echo "$line" | awk '{print $4, $5}'
        return
    fi
    json=$(compgen -G "$test_root/traces-$mode/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
entry = timer[sys.argv[2]][sys.argv[3]]
print(entry[0], entry[1])' "$json" "$type" "$call"
    fi
}

# Prints one number of the task's summary, the same two ways.
task_entry() {
    local mode=$1 name=$2
    local line json
    line=$(grep "^\[MONITOR\] task " "$test_root/output-$mode" | head -n 1 || true)
    if [[ -n $line ]]; then
        echo "$line" | awk -v name="$name" '{for (i = 1; i < NF; i++) if ($i == name) print $(i + 1)}'
        return
    fi
    json=$(compgen -G "$test_root/traces-$mode/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
print(next(iter(json.load(open(sys.argv[1])).values()))["task"][sys.argv[2]])' "$json" "$name"
    fi
}

# Whether the first number is less than the second.
less() {
    awk -v a="$1" -v b="$2" 'BEGIN {exit !(a + 0 < b + 0)}'
}

for mode in 1 0; do
    traces="$test_root/traces-$mode"
    work="$test_root/work-$mode"
    mkdir -p "$traces" "$work"
    status=0
    timeout 60 env \
        LD_PRELOAD="$monitor_lib $test_root/libslow_output.so" \
        SLOW_DIRECTORY="$traces" \
        DATALIFE_OUTPUT_PATH="$traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/close_files" "$work" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "the program failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    # The test's premise: the two calls took as long as writing the traces of
    # two files, a quarter of a second each at least.
    waited=$(cat "$work/waited")
    if less "$waited" 0.5; then
        echo "close() and fclose() took $waited s: the traces were not written during them, or not slowly (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi

    read -r close_time close_calls <<<"$(timer_entry "$mode" monitor close)"
    read -r output_time output_calls <<<"$(timer_entry "$mode" monitor trace_output)"
    if [[ -z ${close_time:-} ]]; then
        continue # no python3 to read the timer's JSON file with
    fi
    if [[ $close_calls != 2 ]]; then
        echo "monitored close calls: '$close_calls', expected 2 (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if ! less "$close_time" 0.2; then
        echo "the timer reports $close_time s in close() of monitored files: the time the library took to write their traces is in it (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if [[ ${output_calls:-} != 2 ]] || less "$output_time" 0.5; then
        echo "monitor trace_output is '${output_time:-} ${output_calls:-}' (time calls), expected two calls of a quarter of a second or more (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi

    # Nor is it in the totals of the program's I/O, or in what is left of the wall
    # time, its computing.
    for total in io_time_s io_busy_time_s compute_time_s; do
        got=$(task_entry "$mode" "$total")
        if [[ -n $got ]] && ! less "$got" 0.2; then
            echo "the task's $total is $got: the time the library took to write traces is in it (DATALIFE_JSON_OUTPUT=$mode)" >&2
            exit 1
        fi
    done
done

echo "the time the library takes to write traces is not reported as the program's close()"