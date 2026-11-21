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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-stream-inline-io.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# bowtie2 reads its FASTQ input with open(), fdopen() and getc_unlocked(), which
# glibc inlines: the program takes characters from the stream's buffer itself
# and calls libc (__uflow) only to refill it. The stream must be monitored, the
# characters it reads or writes this way recorded once each, at their offsets,
# and the trace written when the stream is closed.
cat >"$test_root/stream_inline_io.c" <<'EOF'
#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BLOCK 4096

static char data[5 * BLOCK];
static char stream_buffer[BLOCK]; /* one block: each refill or flush is one block */
static int failures;

static void expect(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAILED: %s\n", what);
        failures++;
    }
}

/* Creates a file of `size` bytes with raw system calls: the library does not see it. */
static void prepare(const char *path, size_t size) {
    int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || syscall(SYS_write, fd, data, size) != (long)size || syscall(SYS_close, fd) != 0) {
        fprintf(stderr, "FAILED: setup of %s\n", path);
        failures++;
    }
}

/* Takes up to `count` characters with the inlined getc_unlocked(); returns how many it got. */
static long take(FILE *fp, long count) {
    long got = 0;
    while (got < count && getc_unlocked(fp) != EOF) {
        got++;
    }
    return got;
}

static void put(FILE *fp, long count) {
    while (count-- > 0) {
        putc_unlocked('d', fp);
    }
}

static int print(FILE *fp, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int printed = vfprintf(fp, format, args);
    va_end(args);
    return printed;
}

/* A second thread that writes a line of 100 bytes to a stream the first has locked. */
static FILE *shared;
static atomic_int shared_stage;

static void *write_line(void *arg) {
    static char line[101];
    (void)arg;
    memset(line, 'd', 100);
    while (shared_stage != 1) {
        usleep(1000);
    }
    shared_stage = 2;
    return fputs(line, shared) == EOF ? (void *)1 : NULL; /* waits for the first thread's funlockfile() */
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    memset(data, 'd', sizeof data);
    char buf[256];

    /* The whole file, a character at a time, as bowtie2 reads its input. */
    prepare("data-getc", 5 * BLOCK);
    int fd = open("data-getc", O_RDONLY);
    FILE *fp = fdopen(fd, "r");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fdopen of data-getc");
    expect(take(fp, 1 << 20) == 5 * BLOCK, "data-getc holds 5 blocks");
    expect(fclose(fp) == 0, "fclose of data-getc");

    /* Closed before the end: 6000 characters taken, 8192 read into the buffer. */
    prepare("data-partial", 5 * BLOCK);
    fd = open("data-partial", O_RDONLY);
    fp = fdopen(fd, "r");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fdopen of data-partial");
    expect(take(fp, 6000) == 6000, "6000 characters of data-partial");
    expect(fclose(fp) == 0, "fclose of data-partial");

    /* Characters taken between calls the library sees, on a stream from fopen(). */
    prepare("data-mixed", 5 * BLOCK);
    fp = fopen("data-mixed", "r");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fopen of data-mixed");
    expect(take(fp, 100) == 100, "bytes 0-99 of data-mixed");
    expect(fread(buf, 1, 200, fp) == 200, "fread of bytes 100-299");
    expect(take(fp, 50) == 50, "bytes 300-349");
    expect(fgets(buf, 51, fp) && strlen(buf) == 50, "fgets of bytes 350-399");
    expect(take(fp, 24) == 24, "bytes 400-423");
    expect(fseeko(fp, 2 * BLOCK, SEEK_SET) == 0, "fseeko to block 2");
    expect(take(fp, 10) == 10, "10 bytes of block 2");
    expect(fseeko64(fp, 4 * BLOCK, SEEK_SET) == 0, "fseeko64 to block 4");
    expect(take(fp, 1 << 20) == BLOCK, "block 4 to the end of the file");
    expect(fclose(fp) == 0, "fclose of data-mixed");

    /* Written a character at a time, with a flush and an fwrite() on the way. */
    fd = open("data-putc", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    fp = fdopen(fd, "w");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fdopen of data-putc");
    put(fp, 6000);
    expect(fflush(fp) == 0, "fflush after 6000 bytes");
    put(fp, 1000);
    expect(fwrite(data, 1, 500, fp) == 500, "fwrite of bytes 7000-7499");
    put(fp, 692);
    expect(fclose(fp) == 0, "fclose of data-putc");
    fd = (int)syscall(SYS_openat, AT_FDCWD, "data-putc", O_RDONLY);
    static char written[2 * BLOCK + 1];
    expect(syscall(SYS_read, fd, written, sizeof written) == 2 * BLOCK && memcmp(written, data, 2 * BLOCK) == 0,
           "data-putc holds the 8192 bytes written");
    syscall(SYS_close, fd);

    /* Streams that write at once what they are given: every character of an
       unbuffered one, every line of a line-buffered one. */
    fp = fopen("data-unbuffered", "w");
    expect(fp && setvbuf(fp, NULL, _IONBF, 0) == 0, "fopen of data-unbuffered");
    put(fp, 100);
    expect(fclose(fp) == 0, "fclose of data-unbuffered");
    fp = fopen("data-lines", "w");
    expect(fp && setvbuf(fp, stream_buffer, _IOLBF, BLOCK) == 0, "fopen of data-lines");
    for (int line = 0; line < 3; line++) {
        put(fp, 10);
        putc_unlocked('\n', fp);
    }
    expect(fclose(fp) == 0, "fclose of data-lines");

    /* The unlocked calls as they are meant to be used: with the stream locked,
       while another thread waits to write to it. */
    shared = fopen("data-shared", "w");
    expect(shared && setvbuf(shared, stream_buffer, _IOFBF, BLOCK) == 0, "fopen of data-shared");
    pthread_t writer;
    expect(pthread_create(&writer, NULL, write_line, NULL) == 0, "start of the second thread");
    flockfile(shared);
    shared_stage = 1;
    while (shared_stage != 2) {
        usleep(1000);
    }
    usleep(100000); /* the second thread is now waiting in fputs() */
    put(shared, 6000);
    expect(fflush(shared) == 0, "fflush with the stream locked");
    funlockfile(shared);
    void *line_failed = NULL;
    expect(pthread_join(writer, &line_failed) == 0 && line_failed == NULL, "the second thread's fputs");
    expect(fclose(shared) == 0, "fclose of data-shared");

    /* A buffer that a seek filled: the stream never asks libc for more. */
    prepare("data-seek", 5 * BLOCK);
    fp = fopen("data-seek", "r");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fopen of data-seek");
    expect(fseeko(fp, 2 * BLOCK + 10, SEEK_SET) == 0, "fseeko into block 2 of data-seek");
    expect(take(fp, 100) == 100, "100 bytes of block 2 of data-seek");
    expect(fclose(fp) == 0, "fclose of data-seek");

    /* A character put back that is not the one read: the stream is 99 bytes into
       the file, and that is what it read. */
    prepare("data-unget", BLOCK);
    fp = fopen("data-unget", "r");
    expect(fp && take(fp, 100) == 100, "100 bytes of data-unget");
    expect(ungetc('Z', fp) == 'Z', "ungetc of another character");
    expect(fclose(fp) == 0, "fclose of data-unget");

    /* Two streams that append to one file. What the first has in its buffer when
       the second makes the file longer is recorded once, though its position
       has moved with the end of the file. */
    prepare("data-append", BLOCK);
    FILE *first = fopen("data-append", "a");
    FILE *second = fopen("data-append", "a");
    expect(first && second, "fopen of data-append, twice");
    expect(fwrite(data, 1, 100, first) == 100, "100 bytes in the first stream's buffer");
    expect(fwrite(data, 1, 1000, second) == 1000 && fflush(second) == 0, "1000 bytes appended through the second stream");
    expect(fwrite(data, 1, 100, first) == 100, "100 more bytes in the first stream's buffer");
    expect(fclose(first) == 0 && fclose(second) == 0, "fclose of both streams on data-append");

    /* Lines that hold a NUL: each is 100 bytes, whatever strlen() says. */
    static char lines[300];
    memset(lines, 'd', sizeof lines);
    for (int line = 0; line < 3; line++) {
        lines[line * 100 + 10] = '\0';
        lines[line * 100 + 99] = '\n';
    }
    fd = (int)syscall(SYS_openat, AT_FDCWD, "data-nul", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(fd >= 0 && syscall(SYS_write, fd, lines, sizeof lines) == (long)sizeof lines && syscall(SYS_close, fd) == 0,
           "setup of data-nul");
    fp = fopen("data-nul", "r");
    int read_lines = 0;
    while (fp && fgets(buf, sizeof buf, fp)) {
        read_lines++;
    }
    expect(read_lines == 3 && fclose(fp) == 0, "3 lines of data-nul");

    /* vfprintf() prints its arguments, and its 11 bytes are recorded. */
    fp = fopen("data-printf", "w");
    expect(fp && print(fp, "%d %s %d\n", 42, "hello", 7) == 11, "vfprintf to data-printf");
    expect(fclose(fp) == 0, "fclose of data-printf");
    fd = (int)syscall(SYS_openat, AT_FDCWD, "data-printf", O_RDONLY);
    memset(buf, 0, sizeof buf);
    expect(syscall(SYS_read, fd, buf, sizeof buf - 1) == 11 && strcmp(buf, "42 hello 7\n") == 0,
           "data-printf holds what was printed");
    syscall(SYS_close, fd);

    /* A stream used only with calls the library does not see, here fprintf(), is
       not reported at all, rather than with the little that is left in its
       buffer when it is closed. */
    fp = fopen("data-unseen", "w");
    for (int i = 0; fp && i < 2000; i++) {
        fprintf(fp, "%d\n", i);
    }
    expect(fp && fclose(fp) == 0, "fclose of data-unseen");

    /* A program that closes the descriptor behind the stream's back keeps running. */
    prepare("data-closed", BLOCK);
    fp = fopen("data-closed", "r");
    expect(fp && close(fileno(fp)) == 0, "close of the descriptor of data-closed");
    expect(getc_unlocked(fp) == EOF, "a stream whose descriptor is closed reads nothing");
    fclose(fp);

    return failures != 0;
}
EOF

# Optimized, for the macros; not fortified, so that vfprintf() is not __vfprintf_chk().
cc -Wall -Wextra -Werror -O2 -U_FORTIFY_SOURCE -pthread "$test_root/stream_inline_io.c" -o "$test_root/stream_inline_io"

# The program must really make only the slow-path calls, or the test proves nothing.
imports=$(nm -D --undefined-only "$test_root/stream_inline_io" | awk '{print $2}' | sed 's/@.*//')
for symbol in __uflow __overflow fdopen vfprintf; do
    if ! grep -qx "$symbol" <<<"$imports"; then
        echo "this C library does not inline getc_unlocked and putc_unlocked around $symbol; nothing to test" >&2
        exit 0
    fi
done
if grep -qxE 'getc_unlocked|putc_unlocked' <<<"$imports"; then
    echo "this compiler does not inline getc_unlocked and putc_unlocked; nothing to test" >&2
    exit 0
fi

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

# Block touches and the bytes of each block's first access, per file:
#   data-getc     one refill per block
#   data-partial  block 0, and at the close bytes 4096-5999 of block 1
#   data-mixed    bytes 0-99, 100-299 (fread), 300-399 (taken, then fgets) and
#                 400-423 of block 0, 10 bytes of block 2 before the second seek,
#                 block 4
#   data-putc     block 0 when the buffer fills, then bytes 4096-5999 (fflush),
#                 6000-6999, 7000-7499 (fwrite) and 7500-8191 (fclose) of block 1
#   data-unbuffered, data-lines  a character at a time
#   data-shared   block 0 when the buffer fills, then bytes 4096-5999 (fflush) and
#                 the other thread's 6000-6099 of block 1
#   data-append   three writes, all in block 1: 100 bytes, 1000 bytes, 100 bytes
expected_rows=(
    "data-getc r 0 1 4096;1 1 4096;2 1 4096;3 1 4096;4 1 4096;"
    "data-partial r 0 1 4096;1 1 1904;"
    "data-mixed r 0 4 100;2 1 10;4 1 4096;"
    "data-putc w 0 1 4096;1 4 1904;"
    "data-unbuffered w 0 100 1;"
    "data-lines w 0 33 1;"
    "data-shared w 0 1 4096;1 2 1904;"
    "data-seek r 2 1 100;"
    "data-unget r 0 1 99;"
    "data-append w 1 3 100;"
    "data-nul r 0 3 100;"
    "data-printf w 0 1 11;"
)
# The same as access_frequency and data_volume (each block's frequency times the
# bytes of its first access), with the file's size at the close.
expected_json=(
    "data-getc r 5 20480 20480"
    "data-partial r 2 6000 20480"
    "data-mixed r 6 4506 20480"
    "data-putc w 5 11712 8192"
    "data-unbuffered w 100 100 100"
    "data-lines w 33 33 33"
    "data-shared w 3 7904 6100"
    "data-seek r 1 100 20480"
    "data-unget r 1 99 4096"
    "data-append w 3 300 5296"
    "data-nul r 3 300 300"
    "data-printf w 1 11 11"
)

for mode in 1 0; do
    mkdir -p "$test_root/traces-$mode" "$test_root/work-$mode"
    status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/traces-$mode" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/stream_inline_io" "$test_root/work-$mode" \
        >"$test_root/output-$mode" 2>&1 || status=$?
    if [[ $status -eq 124 ]]; then
        echo "the program reading and writing a character at a time did not finish: two threads using one stream block each other (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    if [[ $status -ne 0 ]]; then
        echo "the program reading and writing a character at a time failed with status $status (DATALIFE_JSON_OUTPUT=$mode):" >&2
        grep -v '^\[MONITOR\]' "$test_root/output-$mode" | tail -n 20 >&2
        exit 1
    fi

    # Bytes read: 20480 + 6000 + 4430 + 100 + 99 (the 100 bytes of data-mixed
    # recorded by fgets() are in the block statistics only, like the lines of
    # data-nul).
    # Bytes written: 8192 + 100 + 33 + 6000 + 1200 (the 100 bytes of data-shared
    # written with fputs() and the 11 printed to data-printf likewise).
    for expected in "read 31109" "write 15525"; do
        got=$(timer_bytes "$mode" monitor "${expected% *}")
        if [[ -n $got && $got != "${expected#* }" ]]; then
            echo "monitored ${expected% *} bytes: '$got', expected '${expected#* }' (DATALIFE_JSON_OUTPUT=$mode)" >&2
            exit 1
        fi
    done

    # No trace of data-unseen, or with the legacy output, which is written for
    # every file closed, a histogram without rows.
    unseen=$(compgen -G "$test_root/traces-$mode/data-unseen.[0-9]*_blk_trace.json" || true)
    stat=$(compgen -G "$test_root/traces-$mode/data-unseen_[0-9]*_w_stat" | head -n 1 || true)
    if [[ -n $unseen || -n $(tail -n +2 "${stat:-/dev/null}") ]]; then
        echo "a stream written only with fprintf() is reported with part of its bytes (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi

    if [[ $mode -eq 1 ]]; then
        for expected in "${expected_json[@]}"; do
            read -r name kind want <<<"$expected"
            trace=$(compgen -G "$test_root/traces-$mode/$name.*.${kind}_blk_trace.json" | head -n 1 || true)
            if [[ -z $trace ]]; then
                echo "no $kind trace was written for $name" >&2
                exit 1
            fi
            if command -v python3 >/dev/null 2>&1; then
                got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"], d["file_size_bytes"])' "$trace")
                if [[ $got != "$want" ]]; then
                    echo "access_frequency, data_volume and file_size_bytes of $name: '$got', expected '$want'" >&2
                    exit 1
                fi
            fi
        done
    else
        for expected in "${expected_rows[@]}"; do
            read -r name kind want <<<"$expected"
            stat=$(compgen -G "$test_root/traces-$mode/${name}_*_${kind}_stat" | head -n 1 || true)
            got=$(tail -n +2 "${stat:-/dev/null}" | tr '\n' ';')
            if [[ $got != "$want" ]]; then
                echo "${kind}_stat rows of $name: '$got', expected '$want'" >&2
                exit 1
            fi
        done
    fi
done

echo "characters read and written in a stream's buffer are monitored"