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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-locked-stream.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# A thread that holds a stream with flockfile() may call anything on it, while
# other threads wait in their own calls for funlockfile(). The library's
# wrappers must not make the two wait for each other: one that is waiting for
# the stream must hold nothing the thread that has it needs. Here a thread
# locks a monitored stream, a second one starts a call on it, and the first
# goes on reading, writing and seeking before it unlocks. What the second finds
# when it gets the stream is what the first left: another file, if the first
# reopened the stream.
cat >"$test_root/locked_stream.c" <<'EOF'
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>

#define BLOCK 4096

static char data[BLOCK];
static char buf[BLOCK];
static int failures;

static void expect(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", what);
        failures++;
    }
}

/* The second thread: runs `waiting` once the first has the stream locked. */
static FILE *shared;
static atomic_int stage;
static void *(*waiting)(void *);

static void *second_thread(void *arg) {
    while (stage != 1) {
        usleep(1000);
    }
    stage = 2;
    return waiting(arg); /* waits for the first thread's funlockfile() */
}

static void *write_50(void *arg) {
    (void)arg;
    return fwrite(data, 1, 50, shared) == 50 ? NULL : (void *)1;
}

static void *read_50(void *arg) {
    static char mine[50];
    (void)arg;
    return fread(mine, 1, 50, shared) == 50 ? NULL : (void *)1;
}

static void *print_50(void *arg) {
    (void)arg;
    return fprintf(shared, "%049d\n", 2) == 50 ? NULL : (void *)1;
}

static void *write_unlocked_50(void *arg) {
    const char *volatile text = "nineteen characters"; /* volatile: or the compiler calls fwrite_unlocked() */
    (void)arg;
    return fwrite_unlocked(data, 1, 30, shared) == 30 && fputs_unlocked(text, shared) != EOF &&
                   (fputc_unlocked)('c', shared) == 'c'
               ? NULL
               : (void *)1;
}

static void *close_stream(void *arg) {
    (void)arg;
    return fclose(shared) == 0 ? NULL : (void *)1;
}

static void *reopen_and_write_50(void *arg) {
    (void)arg;
    return freopen("data-relocked-b", "w", shared) == shared && fwrite(data, 1, 50, shared) == 50 ? NULL : (void *)1;
}

static pthread_t start_second(void *(*call)(void *)) {
    pthread_t thread;
    stage = 0;
    waiting = call;
    expect(pthread_create(&thread, NULL, second_thread, NULL) == 0, "start of the second thread");
    return thread;
}

/* Called with the stream locked: returns once the second thread is in its call. */
static void let_second_wait(void) {
    stage = 1;
    while (stage != 2) {
        usleep(1000);
    }
    usleep(100000);
}

static void join_second(pthread_t thread, const char *what) {
    void *call_failed = NULL;
    expect(pthread_join(thread, &call_failed) == 0 && call_failed == NULL, what);
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    alarm(20);
    /* Lines of 100 bytes. */
    memset(data, 'd', sizeof data);
    for (int i = 99; i < BLOCK; i += 100) {
        data[i] = '\n';
    }
    const char *volatile text = "twenty bytes of text"; /* volatile: or the compiler calls fwrite() */

    /* Writes: 100 bytes, then with the second thread waiting 20, 1 and 100
       more; the second thread's 50 follow. */
    shared = fopen("data-locked-write", "w");
    expect(shared != NULL, "fopen of data-locked-write");
    pthread_t second = start_second(write_50);
    flockfile(shared);
    expect(fwrite(data, 1, 100, shared) == 100, "fwrite() with the stream locked");
    let_second_wait();
    expect(fputs(text, shared) != EOF, "fputs() while another thread waits");
    expect(fputc('c', shared) == 'c', "fputc() while another thread waits");
    expect(ftell(shared) == 121 && !feof(shared), "ftell() and feof() while another thread waits");
    expect(fwrite(data, 1, 100, shared) == 100, "fwrite() while another thread waits");
    funlockfile(shared);
    join_second(second, "the second thread's fwrite()");
    expect(fclose(shared) == 0, "fclose of data-locked-write");

    /* Reads: bytes 0-99, then with the second thread waiting a line, a
       character, a seek to 1000, 100 bytes and a line; the second thread's 50
       follow. */
    int fd = (int)syscall(SYS_openat, AT_FDCWD, "data-locked-read", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(fd >= 0 && syscall(SYS_write, fd, data, BLOCK) == BLOCK && syscall(SYS_close, fd) == 0, "setup of data-locked-read");
    shared = fopen("data-locked-read", "r");
    expect(shared != NULL, "fopen of data-locked-read");
    second = start_second(read_50);
    flockfile(shared);
    expect(fread(buf, 1, 100, shared) == 100, "fread() with the stream locked");
    let_second_wait();
    expect(fgets(buf, 200, shared) && strlen(buf) == 100, "fgets() while another thread waits");
    expect(fgetc(shared) == 'd', "fgetc() while another thread waits");
    expect(fseek(shared, 1000, SEEK_SET) == 0, "fseek() while another thread waits");
    expect(fread_unlocked(buf, 1, 100, shared) == 100, "fread_unlocked() while another thread waits");
    expect(fgets_unlocked(buf, 200, shared) && strlen(buf) == 100, "fgets_unlocked() while another thread waits");
    funlockfile(shared);
    join_second(second, "the second thread's fread()");
    expect(ftell(shared) == 1250 && fclose(shared) == 0, "fclose of data-locked-read");

    /* The second thread waits to move the stream to another file. */
    shared = fopen("data-relocked-a", "w");
    expect(shared != NULL, "fopen of data-relocked-a");
    second = start_second(reopen_and_write_50);
    flockfile(shared);
    expect(fwrite(data, 1, 100, shared) == 100, "fwrite() with the stream locked");
    let_second_wait();
    expect(fwrite(data, 1, 100, shared) == 100, "fwrite() while another thread waits in freopen()");
    funlockfile(shared);
    join_second(second, "the second thread's freopen() and fwrite()");
    expect(fclose(shared) == 0, "fclose of the reopened stream");

    /* The first thread moves the stream to another file while the second
       waits to print to it: what it prints goes to the new file. */
    shared = fopen("data-reheld-a", "w");
    expect(shared != NULL, "fopen of data-reheld-a");
    second = start_second(print_50);
    flockfile(shared);
    expect(fwrite(data, 1, 100, shared) == 100, "fwrite() with the stream locked");
    let_second_wait();
    expect(freopen("data-reheld-b", "w", shared) == shared, "freopen() while another thread waits in fprintf()");
    funlockfile(shared);
    join_second(second, "the second thread's fprintf()");
    expect(fclose(shared) == 0, "fclose of the stream moved to data-reheld-b");

    /* The first thread keeps the stream locked and leaves a write to the
       second, which makes it with a call that does not lock: the second must
       not wait for the first. Its bytes are recorded when the stream is closed. */
    shared = fopen("data-held", "w");
    expect(shared != NULL, "fopen of data-held");
    flockfile(shared);
    expect(fwrite_unlocked(data, 1, 100, shared) == 100, "fwrite_unlocked() by the thread that has the stream");
    expect(pthread_create(&second, NULL, write_unlocked_50, NULL) == 0, "start of the thread that writes for it");
    join_second(second, "fwrite_unlocked() by another thread while the stream is locked");
    funlockfile(shared);
    expect(fclose(shared) == 0, "fclose of data-held");

    /* The second thread waits to close the stream: the first does not use it
       once it is unlocked. */
    shared = fopen("data-locked-close", "w");
    expect(shared != NULL, "fopen of data-locked-close");
    second = start_second(close_stream);
    flockfile(shared);
    expect(fwrite(data, 1, 100, shared) == 100, "fwrite() with the stream locked");
    let_second_wait();
    expect(fwrite(data, 1, 100, shared) == 100, "fwrite() while another thread waits in fclose()");
    funlockfile(shared);
    join_second(second, "the second thread's fclose()");

    return failures != 0;
}
EOF

cc -Wall -Wextra -Werror -O2 -U_FORTIFY_SOURCE -pthread "$test_root/locked_stream.c" -o "$test_root/locked_stream"

# A program that moves its standard output to a new log with dup2() while its
# threads print: each line goes to the old log or the new one, and nothing of the
# library's may go away under a thread that is in the middle of a call.
cat >"$test_root/move_output.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

static atomic_int stop;

static void *print_lines(void *arg) {
    (void)arg;
    while (!stop) {
        printf("line %d %s\n", 12345, "abcdefghijklmnopqrstuvwxyz");
        fputs("more\n", stdout);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    alarm(60);
    int saved = dup(STDOUT_FILENO);
    int nowhere = open("/dev/null", O_WRONLY);
    if (saved < 0 || nowhere < 0 || dup2(nowhere, STDOUT_FILENO) != STDOUT_FILENO) {
        return 1;
    }
    pthread_t threads[3];
    for (int i = 0; i < 3; i++) {
        if (pthread_create(&threads[i], NULL, print_lines, NULL) != 0) {
            return 1;
        }
    }
    for (int i = 0; i < 2000; i++) {
        char name[32];
        snprintf(name, sizeof name, "data-log-%d", i % 4);
        int fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0 || dup2(fd, STDOUT_FILENO) != STDOUT_FILENO || close(fd) != 0 || dup2(nowhere, STDOUT_FILENO) != STDOUT_FILENO) {
            return 1;
        }
    }
    stop = 1;
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    return dup2(saved, STDOUT_FILENO) == STDOUT_FILENO ? 0 : 1;
}
EOF
cc -Wall -Wextra -Werror -O2 -U_FORTIFY_SOURCE -pthread "$test_root/move_output.c" -o "$test_root/move_output"

# Prints the bytes of one timer entry from the [MONITOR] lines on stdout (builds
# without TIMER_JSON) or the timer's JSON file.
timer_bytes() {
    local mode=$1 type=$2 call=$3
    local line json
    line=$(grep "^\[MONITOR\] $type $call " "$test_root/output-$mode" | head -n 1 || true)
    if [[ -n $line ]]; then
        echo "$line" | awk '{print $6}'
        return
    fi
    json=$(compgen -G "$test_root/traces-$mode/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
print(timer[sys.argv[2]][sys.argv[3]][2])' "$json" "$type" "$call"
    fi
}

# For each file and direction, its size when it was closed and its block
# touches: block, accesses, bytes of the block's first access.
expected=(
    "data-locked-write w 271 0 5 100;"
    "data-locked-read r 4096 0 6 100;"
    "data-relocked-a w 200 0 2 100;"
    "data-relocked-b w 50 0 1 50;"
    "data-reheld-a w 100 0 1 100;"
    "data-reheld-b w 50 0 1 50;"
    "data-held w 150 0 2 100;"
    "data-locked-close w 200 0 2 100;"
)

for mode in 1 0; do
    mkdir -p "$test_root/traces-$mode" "$test_root/work-$mode"
    status=0
    timeout 30 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/traces-$mode" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/locked_stream" "$test_root/work-$mode" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -eq 124 || $status -eq 142 ]]; then
        echo "the program did not finish: a thread that has a stream locked and one that waits for it block each other (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if [[ $status -ne 0 ]]; then
        echo "the program failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    # Bytes read: 100 + 100 + 1 + 100 + 100 + 50. Bytes written: 271 + 200 + 50 +
    # 100 + 50 + 150 + 200.
    for want in "read 451" "write 1021"; do
        got=$(timer_bytes "$mode" monitor "${want% *}")
        if [[ -n $got && $got != "${want#* }" ]]; then
            echo "monitored ${want% *} bytes: '$got', expected '${want#* }' (DATALIFE_JSON_OUTPUT=$mode)" >&2
            exit 1
        fi
    done

    for entry in "${expected[@]}"; do
        read -r name kind size rows <<<"$entry"
        if [[ $mode -eq 1 ]]; then
            trace=$(compgen -G "$test_root/traces-$mode/$name.*.${kind}_blk_trace.json" | head -n 1 || true)
            if [[ -z $trace ]]; then
                echo "no $kind trace was written for $name" >&2
                exit 1
            fi
            if command -v python3 >/dev/null 2>&1; then
                want=$(awk -v size="$size" 'BEGIN {RS = ";"} NF == 3 {accesses += $2; volume += $2 * $3} END {print accesses, volume, size}' <<<"$rows")
                got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"], d["file_size_bytes"])' "$trace")
                if [[ $got != "$want" ]]; then
                    echo "access_frequency, data_volume and file_size_bytes of $name: '$got', expected '$want'" >&2
                    exit 1
                fi
            fi
        else
            stat=$(compgen -G "$test_root/traces-$mode/${name}_*_${kind}_stat" | head -n 1 || true)
            got=$(tail -n +2 "${stat:-/dev/null}" | tr '\n' ';')
            if [[ $got != "$rows" ]]; then
                echo "${kind}_stat rows of $name: '$got', expected '$rows'" >&2
                exit 1
            fi
        fi
    done
done

for mode in 1 0; do
    mkdir -p "$test_root/traces-move-$mode" "$test_root/work-move-$mode"
    status=0
    timeout 90 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/traces-move-$mode" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/move_output" "$test_root/work-move-$mode" \
        >"$test_root/output-move-$mode" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "the program that moves its output while its threads print failed with status $status (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
done

echo "calls on a locked stream do not block the threads that wait for it"