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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-entry-points.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# One megabyte read from, or written to, a monitored file through each of the
# calls a program can do it with besides read(), write(), fread() and fwrite():
# the other calls on streams, the calls that take vectors of buffers, the ones
# that copy between two descriptors in the kernel, and descriptors that are
# copies of others. The library must record the megabyte, whatever the call, and
# report the file. The program is built twice, because the same source calls
# different functions when it is optimized and fortified: __fprintf_chk() for
# fprintf(), __getdelim() for getline(), the macros for getc_unlocked().
cat >"$test_root/entry_points.c" <<'EOF_C'
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef RWF_APPEND
#define RWF_APPEND 0x00000010
#endif

#define SIZE (1 << 20) /* what every case reads or writes */
#define LINE 64

static char data[SIZE];

/* Text the cases that read can parse: lines of 64 bytes, each eight numbers of seven digits. */
static void fill(void) {
    for (int i = 0; i < SIZE; i += 8) {
        memcpy(data + i, (i % LINE == LINE - 8) ? "1234567\n" : "1234567 ", 8);
    }
}

/* Creates the file the case reads, with raw system calls: the library does not see it. */
static void prepare(const char *path) {
    int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || syscall(SYS_write, fd, data, SIZE) != SIZE || syscall(SYS_close, fd) != 0) {
        exit(3);
    }
}

static long size_of(const char *path) {
    int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_RDONLY);
    long size = fd < 0 ? -1 : (long)syscall(SYS_lseek, fd, 0, SEEK_END);
    syscall(SYS_close, fd);
    return size;
}

static int print(FILE *fp, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int printed = vfprintf(fp, format, args);
    va_end(args);
    return printed;
}

#define CHECK(ok) do { if (!(ok)) { fprintf(stderr, "probe %s: line %d failed (%s)\n", name, __LINE__, strerror(errno)); return 1; } } while (0)
#define IS(what) (strcmp(name, what) == 0)

int main(int argc, char **argv) {
    if (argc != 3 || chdir(argv[2]) != 0) {
        return 2;
    }
    const char *name = argv[1];
    const char *path = "data-probe";
    static char buf[SIZE];
    static char line[LINE + 1];
    char *volatile text = line; /* volatile: or the compiler picks another call than the one named */
    long n = 0;
    FILE *fp;
    int fd, c;
    fill();
    memcpy(line, data, LINE);

    /* Reads through a stream. */
    if (IS("getc") || IS("getc_unlocked") || IS("fgetc") || IS("fgetc_unlocked") || IS("fgets") || IS("fgets_unlocked") ||
        IS("getline") || IS("getdelim") || IS("fscanf") || IS("fread_unlocked")) {
        prepare(path);
        CHECK(fp = fopen(path, "r"));
        if (IS("getc")) { while ((c = getc(fp)) != EOF) n++; }
        else if (IS("getc_unlocked")) { while ((c = getc_unlocked(fp)) != EOF) n++; }
        else if (IS("fgetc")) { while ((c = fgetc(fp)) != EOF) n++; }
        else if (IS("fgetc_unlocked")) { while ((c = fgetc_unlocked(fp)) != EOF) n++; }
        else if (IS("fgets")) { while (fgets(buf, 4096, fp)) n += (long)strlen(buf); }
        else if (IS("fgets_unlocked")) { while (fgets_unlocked(buf, 4096, fp)) n += (long)strlen(buf); }
        else if (IS("getline") || IS("getdelim")) {
            char *got = NULL; size_t room = 0; ssize_t length;
            while ((length = IS("getline") ? getline(&got, &room, fp) : getdelim(&got, &room, ' ', fp)) > 0) n += length;
            free(got);
        }
        else if (IS("fscanf")) { int value; while (fscanf(fp, "%d", &value) == 1) n += 8; }
        else { size_t got; while ((got = fread_unlocked(buf, 1, 5000, fp)) > 0) n += (long)got; }
        CHECK(n == SIZE && fclose(fp) == 0);
        return 0;
    }

    /* Writes through a stream. */
    if (IS("putc") || IS("putc_unlocked") || IS("fputc") || IS("fputc_unlocked") || IS("fputs") || IS("fputs_unlocked") ||
        IS("fprintf") || IS("vfprintf") || IS("fwrite_unlocked")) {
        CHECK(fp = fopen(path, "w"));
        if (IS("putc")) { for (; n < SIZE; n++) CHECK(putc(data[n], fp) != EOF); }
        else if (IS("putc_unlocked")) { for (; n < SIZE; n++) CHECK(putc_unlocked(data[n], fp) != EOF); }
        else if (IS("fputc")) { for (; n < SIZE; n++) CHECK(fputc(data[n], fp) != EOF); }
        else if (IS("fputc_unlocked")) { for (; n < SIZE; n++) CHECK(fputc_unlocked(data[n], fp) != EOF); }
        else if (IS("fputs")) { for (; n < SIZE; n += LINE) CHECK(fputs(text, fp) != EOF); }
        else if (IS("fputs_unlocked")) { for (; n < SIZE; n += LINE) CHECK(fputs_unlocked(text, fp) != EOF); }
        else if (IS("fprintf")) { for (; n < SIZE; n += LINE) CHECK(fprintf(fp, "%07d %s", 1234567, text + 8) == LINE); }
        else if (IS("vfprintf")) { for (; n < SIZE; n += LINE) CHECK(print(fp, "%07d %s", 1234567, text + 8) == LINE); }
        else { for (; n < SIZE; n += LINE) CHECK(fwrite_unlocked(text, 1, LINE, fp) == LINE); }
        CHECK(fclose(fp) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (IS("fflush_unlocked")) { /* empties the buffer of the characters put there, which no call brought */
        CHECK(fp = fopen(path, "w"));
        for (; n < SIZE; n++) {
            CHECK(putc_unlocked(data[n], fp) != EOF);
            if (n % 1000 == 999) CHECK(fflush_unlocked(fp) == 0);
        }
        CHECK(fclose(fp) == 0 && size_of(path) == SIZE);
        return 0;
    }

    /* A stream reopened on the file: one of the program's, and the standard ones. */
    if (IS("freopen")) {
        CHECK(fp = fopen("elsewhere", "w"));
        CHECK(fp = freopen(path, "w", fp));
        CHECK(fwrite(data, 1, SIZE, fp) == SIZE && fclose(fp) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (IS("freopen_mode")) { /* no name: the same file in another mode */
        CHECK(fp = fopen(path, "w"));
        CHECK(fwrite(data, 1, SIZE / 2, fp) == SIZE / 2);
        CHECK(fp = freopen(NULL, "a", fp));
        CHECK(fwrite(data, 1, SIZE / 2, fp) == SIZE / 2 && fclose(fp) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (IS("freopen_away")) { /* from the file to one that is not monitored */
        CHECK(fp = fopen(path, "w"));
        CHECK(fwrite(data, 1, SIZE, fp) == SIZE);
        CHECK(fp = freopen("elsewhere", "w", fp));
        CHECK(fwrite(data, 1, SIZE, fp) == SIZE && fclose(fp) == 0 && size_of(path) == SIZE && size_of("elsewhere") == SIZE);
        return 0;
    }
    if (IS("freopen_stdout")) { /* printf(), puts() and putchar() write to it */
        CHECK(freopen(path, "w", stdout));
        for (; n < SIZE / 2; n += LINE) CHECK(printf("%07d %s", 1234567, text + 8) == LINE);
        for (; n < SIZE - LINE; n += LINE) { line[LINE - 1] = '\0'; CHECK(puts(text) != EOF); line[LINE - 1] = '\n'; }
        for (; n < SIZE; n++) CHECK(putchar(data[n]) != EOF);
        CHECK(fclose(stdout) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (IS("dup2_stdout")) { /* standard output writes to the file once its descriptor is a copy of the file's */
        CHECK((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0 && dup2(fd, 1) == 1 && close(fd) == 0);
        for (; n < SIZE / 2; n += LINE) CHECK(printf("%07d %s", 1234567, text + 8) == LINE);
        for (; n < SIZE; n += LINE) CHECK(fwrite(text, 1, LINE, stdout) == LINE);
        CHECK(fclose(stdout) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (IS("freopen_stdin")) { /* scanf() and getchar() read from it */
        prepare(path);
        CHECK(freopen(path, "r", stdin));
        int value;
        for (; n < SIZE / 2; n += 8) CHECK(scanf("%d", &value) == 1);
        n--; /* the blank after the last number is still to read */
        while (getchar() != EOF) n++;
        CHECK(n == SIZE && fclose(stdin) == 0);
        return 0;
    }

    /* Descriptors. */
    if (IS("creat")) {
        CHECK((fd = creat(path, 0644)) >= 0 && write(fd, data, SIZE) == SIZE && close(fd) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (IS("mkstemp") || IS("mkstemp64") || IS("mkostemp") || IS("mkstemps") || IS("mkostemps")) { /* a temporary file, renamed to the result */
        char plain[] = "data-probe.XXXXXX", suffixed[] = "data-probe.XXXXXX.tmp";
        char *temporary = (IS("mkstemps") || IS("mkostemps")) ? suffixed : plain;
        fd = IS("mkstemp") ? mkstemp(temporary) : IS("mkstemp64") ? mkstemp64(temporary) : IS("mkostemp") ? mkostemp(temporary, O_CLOEXEC)
           : IS("mkstemps") ? mkstemps(temporary, 4) : mkostemps(temporary, 4, O_CLOEXEC);
        CHECK(fd >= 0 && write(fd, data, SIZE) == SIZE && close(fd) == 0);
        CHECK(rename(temporary, path) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (strncmp(name, "append", 6) == 0) { /* the megabyte goes after the one the file holds, whatever the call and the offset it names */
        prepare(path);
        if (IS("append_stream")) {
            CHECK(fp = fopen(path, "a"));
            CHECK(fwrite(data, 1, SIZE, fp) == SIZE && fclose(fp) == 0);
        } else if (IS("append_fdopen")) { /* a stream that does not know its descriptor appends */
            CHECK((fd = open(path, O_WRONLY | O_APPEND)) >= 0 && (fp = fdopen(fd, "w")));
            for (; n < SIZE / 2; n++) CHECK(putc_unlocked(data[n], fp) != EOF);
            for (; n < SIZE; n += LINE) CHECK(fputs(text, fp) != EOF);
            CHECK(fclose(fp) == 0);
        } else {
            int later = IS("append_fcntl") || IS("append_pwritev2"); /* not opened to append */
            CHECK((fd = open(path, later ? O_WRONLY : O_WRONLY | O_APPEND)) >= 0);
            if (IS("append_fcntl")) CHECK(fcntl(fd, F_SETFL, O_APPEND) == 0);
            for (; n < SIZE; n += 16 * 4096) {
                struct iovec iov = {data + n, 16 * 4096};
                ssize_t done = IS("append_pwrite") ? pwrite(fd, data + n, 16 * 4096, 0) : IS("append_writev") ? writev(fd, &iov, 1)
                             : IS("append_pwritev") ? pwritev(fd, &iov, 1, 0) : IS("append_pwritev2") ? pwritev2(fd, &iov, 1, 0, RWF_APPEND)
                             : write(fd, data + n, 16 * 4096);
                CHECK(done == 16 * 4096);
            }
            CHECK(close(fd) == 0);
        }
        CHECK(size_of(path) == 2 * SIZE);
        return 0;
    }
    if (IS("dup") || IS("dup2") || IS("dup3") || IS("fcntl_dupfd")) { /* written through the copy */
        CHECK((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        int copy = IS("dup") ? dup(fd) : IS("dup2") ? dup2(fd, 100) : IS("dup3") ? dup3(fd, 100, O_CLOEXEC) : fcntl(fd, F_DUPFD_CLOEXEC, 50);
        CHECK(copy >= 0 && close(fd) == 0);
        CHECK(write(copy, data, SIZE / 2) == SIZE / 2 && pwrite(copy, data, SIZE / 2, SIZE / 2) == SIZE / 2);
        CHECK(close(copy) == 0 && size_of(path) == SIZE);
        return 0;
    }
    if (IS("dup2_onto")) { /* a copy made on a monitored descriptor closes its file */
        int other;
        CHECK((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0 && write(fd, data, SIZE / 2) == SIZE / 2);
        CHECK((other = open("data-probe-copy", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        CHECK(dup2(other, fd) == fd && write(fd, data, SIZE / 2) == SIZE / 2);
        CHECK(close(fd) == 0 && close(other) == 0 && size_of(path) == SIZE / 2 && size_of("data-probe-copy") == SIZE / 2);
        return 0;
    }
    if (IS("pwritev") || IS("pwritev2") || IS("preadv") || IS("preadv2")) {
        int writing = name[1] == 'w';
        if (!writing) prepare(path);
        CHECK((fd = open(path, writing ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY, 0644)) >= 0);
        for (; n < SIZE; n += 16 * 4096) {
            struct iovec iov[16];
            for (int i = 0; i < 16; i++) { iov[i].iov_base = (writing ? data : buf) + n + i * 4096; iov[i].iov_len = 4096; }
            ssize_t done = IS("pwritev") ? pwritev(fd, iov, 16, n) : IS("pwritev2") ? pwritev2(fd, iov, 16, n, 0)
                         : IS("preadv") ? preadv(fd, iov, 16, n) : preadv2(fd, iov, 16, -1, 0); /* -1: where the descriptor is */
            CHECK(done == 16 * 4096);
        }
        CHECK(close(fd) == 0 && size_of(path) == SIZE && (writing || memcmp(buf, data, SIZE) == 0));
        return 0;
    }
    if (IS("sendfile") || IS("sendfile_offset") || IS("copy_file_range") || IS("copy_file_range_offset")) { /* read from one file, written to another */
        prepare(path);
        int out;
        CHECK((fd = open(path, O_RDONLY)) >= 0 && (out = open("data-probe-copy", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        off_t from = 0, to = 0;
        while (n < SIZE) {
            ssize_t done = IS("sendfile") ? sendfile(out, fd, NULL, SIZE - n) : IS("sendfile_offset") ? sendfile(out, fd, &from, SIZE - n)
                         : IS("copy_file_range") ? copy_file_range(fd, NULL, out, NULL, SIZE - n, 0) : copy_file_range(fd, &from, out, &to, SIZE - n, 0);
            CHECK(done > 0);
            n += done;
        }
        CHECK(close(fd) == 0 && close(out) == 0 && size_of("data-probe-copy") == SIZE);
        return 0;
    }
    fprintf(stderr, "probe: no case %s\n", name);
    return 2;
}
EOF_C

cc -Wall -Wextra -Werror -O0 -U_FORTIFY_SOURCE "$test_root/entry_points.c" -o "$test_root/entry_points-plain"
cc -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2 "$test_root/entry_points.c" -o "$test_root/entry_points-fortified"

# Prints "read written", the bytes the timer shows for monitored files, from the
# [MONITOR] lines on stdout (builds without TIMER_JSON) or the timer's JSON file.
# The calls that take vectors are counted apart from the others.
timer_bytes() {
    local lines json
    lines=$(grep -a -E '^\[MONITOR\] monitor (read|write|readv|writev) ' "$test_root/output" | head -n 4 || true)
    if [[ -n $lines ]]; then
        echo "$lines" | awk '{bytes[$3] = $6} END {print bytes["read"] + bytes["readv"], bytes["write"] + bytes["writev"]}'
        return
    fi
    json=$(compgen -G "$test_root/traces/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
monitor = next(iter(json.load(open(sys.argv[1])).values()))["monitor"]
print(monitor["read"][2] + monitor["readv"][2], monitor["write"][2] + monitor["writev"][2])' "$json"
    fi
}

# Prints the last block (of 4096 bytes) that the read (r) or write (w) trace of a
# file shows as used, or with a third argument the first and the last: "none"
# if the file has no trace, nothing without python3.
# The trace's data_volume is not compared: it is an estimate, each block's
# accesses times the size of the first, exact only when all are the same size.
traced_blocks() {
    local trace
    trace=$(compgen -G "$test_root/traces/$1.[0-9]*.$2_blk_trace.json" | head -n 1 || true)
    if [[ -z $trace ]]; then
        echo none
    elif command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
blocks = json.load(open(sys.argv[1]))["io_blk_range"]
print(blocks[2] if len(sys.argv) < 3 else "%d-%d" % (blocks[0], blocks[2]))' "$trace" ${3:+"$3"}
    fi
}

# Each case and what it does to the file: r for a megabyte read, w for one written.
cases="
getc r
getc_unlocked r
fgetc r
fgetc_unlocked r
fgets r
fgets_unlocked r
getline r
getdelim r
fscanf r
fread_unlocked r
putc w
putc_unlocked w
fputc w
fputc_unlocked w
fputs w
fputs_unlocked w
fprintf w
vfprintf w
fwrite_unlocked w
fflush_unlocked w
freopen w
freopen_mode w
freopen_away w
freopen_stdout w
dup2_stdout w
freopen_stdin r
creat w
mkstemp w
mkstemp64 w
mkostemp w
mkstemps w
mkostemps w
append_write w
append_pwrite w
append_writev w
append_pwritev w
append_pwritev2 w
append_fcntl w
append_stream w
append_fdopen w
dup w
dup2 w
dup3 w
fcntl_dupfd w
dup2_onto w
pwritev w
pwritev2 w
preadv r
preadv2 r
sendfile rw
sendfile_offset rw
copy_file_range rw
copy_file_range_offset rw
"

megabyte=1048576
while read -r name kinds; do
    [[ -n $name ]] || continue
    for build in plain fortified; do
        rm -rf "$test_root/work" "$test_root/traces"
        mkdir -p "$test_root/work" "$test_root/traces"
        status=0
        timeout 60 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$test_root/traces" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT=1 \
            "$test_root/entry_points-$build" "$name" "$test_root/work" \
            >"$test_root/output" 2>&1 || status=$?
        if [[ $status -ne 0 ]]; then
            echo "$name ($build): the program failed with status $status with the library" >&2
            grep -av '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
            exit 1
        fi
        want_read=0
        want_written=0
        [[ $kinds == *r* ]] && want_read=$megabyte
        [[ $kinds == *w* ]] && want_written=$megabyte
        got=$(timer_bytes)
        if [[ -n $got && $got != "$want_read $want_written" ]]; then
            echo "$name ($build): the library recorded '$got' bytes (read written), expected '$want_read $want_written'" >&2
            exit 1
        fi
        # The file read is data-probe; the file written is too, or its copy. The
        # megabyte must be recorded where it is in the file: through block 255, or
        # block 511 when it was appended to a megabyte. A
        # copy made with dup2() on the descriptor of data-probe closed that file
        # after half of the megabyte, and the rest went to the file copied.
        while read -r file kind want; do
            range=
            [[ $want == *-* ]] && range=range
            got=$(traced_blocks "$file" "$kind" $range)
            if [[ -n $got && $got != "$want" ]]; then
                echo "$name ($build): the ${kind} trace of $file is of block(s) '$got', expected $want" >&2
                exit 1
            fi
        done <<<"$(
            [[ $kinds == *r* ]] && echo "data-probe r 255"
            if [[ $name == dup2_onto ]]; then
                echo "data-probe w 127"
                echo "data-probe-copy w 127"
            elif [[ $name == append* ]]; then
                echo "data-probe w 256-511" # all of it after the megabyte the file held
            elif [[ $kinds == rw ]]; then
                echo "data-probe-copy w 255"
            elif [[ $kinds == w ]]; then
                echo "data-probe w 255"
            fi
        )"
    done
done <<<"$cases"

echo "a megabyte through each of the other calls is recorded"