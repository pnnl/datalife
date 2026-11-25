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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-libc-entry-points.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# The C library has many ways to open, read and write a file besides open(),
# read(), write() and the stream calls a program is first written with: creat(),
# duplicated descriptors, preadv() and pwritev(), copies made in the kernel,
# freopen() of stdin and stdout with the calls that use them, getc() and putc()
# as real calls, lines, and formatted input and output. A program built another
# way calls other functions for the same source: the 64-bit variants with
# _FILE_OFFSET_BITS=64, the checked ones with _FORTIFY_SOURCE, the plain ones
# without optimization. Whichever it is, I/O on a monitored file must be
# recorded, once, at its offsets, and the program must see what it would without
# the library.
cat >"$test_root/entry_points.c" <<'EOF'
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#define BLOCK 4096

static char data[5 * BLOCK];
static char buf[5 * BLOCK];
static char stream_buffer[BLOCK]; /* one block: each refill or flush is one block */
static int failures;

static void expect(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "FAILED: %s (errno %d)\n", what, errno);
        failures++;
    }
}

/* Creates a file with raw system calls: the library does not see it. */
static void prepare(const char *path, const char *content, size_t size) {
    int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || syscall(SYS_write, fd, content, size) != (long)size || syscall(SYS_close, fd) != 0) {
        fprintf(stderr, "FAILED: setup of %s\n", path);
        failures++;
    }
}

/* Whether the file holds exactly these bytes, read with raw system calls. */
static int holds(const char *path, const char *content, size_t size) {
    static char found[5 * BLOCK + 1];
    int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_RDONLY);
    long got = fd < 0 ? -1 : syscall(SYS_read, fd, found, sizeof found);
    syscall(SYS_close, fd);
    return got == (long)size && memcmp(found, content, size) == 0;
}

/* A C library without these functions (before glibc 2.26 and 2.27): the same
   reads and writes with the calls it has, so that the rest is tested there. */
#if !__GLIBC_PREREQ(2, 26)
static ssize_t preadv2(int fd, const struct iovec *iov, int count, off_t offset, int flags) {
    (void)flags;
    return offset == -1 ? readv(fd, iov, count) : preadv(fd, iov, count, offset);
}

static ssize_t pwritev2(int fd, const struct iovec *iov, int count, off_t offset, int flags) {
    (void)flags;
    return offset == -1 ? writev(fd, iov, count) : pwritev(fd, iov, count, offset);
}
#endif
#if !__GLIBC_PREREQ(2, 27)
static ssize_t copy_file_range(int in, off64_t *off_in, int out, off64_t *off_out, size_t count, unsigned flags) {
    static char copied[BLOCK];
    (void)flags;
    ssize_t got = off_in ? pread(in, copied, count, *off_in) : read(in, copied, count);
    if (got <= 0) {
        return got;
    }
    ssize_t put = off_out ? pwrite(out, copied, got, *off_out) : write(out, copied, got);
    if (put > 0) {
        if (off_in) *off_in += put;
        if (off_out) *off_out += put;
    }
    return put;
}
#endif

#ifdef OLD_GETC
/* What getc() is to a program built with a glibc before 2.28. */
extern int old_getc(FILE *fp);
__asm__(".symver old_getc,_IO_getc@" OLD_GETC);
#endif

/* What fscanf() is to a program built with an older C library, or for an older
   C standard: this one's is __isoc23_fscanf since glibc 2.38. */
extern int c89_fscanf(FILE *fp, const char *format, ...) __asm__("fscanf");
extern int c99_fscanf(FILE *fp, const char *format, ...) __asm__("__isoc99_fscanf");

/* What a child does to have a file as its standard output. */
static __attribute__((noinline)) int open_as_output(const char *path) {
    int out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    return out >= 0 && dup2(out, STDOUT_FILENO) == STDOUT_FILENO && close(out) == 0;
}

/* A second thread that prints a line to a stream the first has locked. */
static FILE *shared;
static atomic_int shared_stage;

static void *print_line(void *arg) {
    (void)arg;
    while (shared_stage != 1) {
        usleep(1000);
    }
    shared_stage = 2;
    return fprintf(shared, "%049d\n", 2) == 50 ? NULL : (void *)1; /* waits for the first thread's funlockfile() */
}

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    alarm(30);
    memset(data, 'd', sizeof data);
    int fd, other;
    FILE *fp;

    /* creat(), then two blocks' worth of write(). */
    fd = creat("data-creat", 0644);
    expect(fd >= 0 && write(fd, data, 5000) == 5000 && close(fd) == 0, "creat() and write() of data-creat");
    expect(holds("data-creat", data, 5000), "data-creat holds the 5000 bytes written");

    /* Files created under a name the C library chooses, written and renamed to
       their real name when they are complete. */
    char temporary[] = "data-temp-XXXXXX";
    fd = mkstemp(temporary);
    expect(fd >= 0 && strncmp(temporary, "data-temp-", 10) == 0 && strcmp(temporary, "data-temp-XXXXXX") != 0, "mkstemp()");
    expect(write(fd, data, 300) == 300 && close(fd) == 0, "write() of 300 bytes to the temporary file");
    expect(rename(temporary, "data-temp-result") == 0 && holds("data-temp-result", data, 300), "rename of the temporary file");
    char suffixed[] = "data-temp-XXXXXX.out";
    fd = mkostemps(suffixed, 4, O_CLOEXEC);
    expect(fd >= 0 && strcmp(suffixed + 16, ".out") == 0 && strcmp(suffixed, "data-temp-XXXXXX.out") != 0, "mkostemps()");
    expect(write(fd, data, 100) == 100 && rename(suffixed, "data-temp-other") == 0 && write(fd, data, 50) == 50,
           "write() before and after the rename");
    expect(close(fd) == 0 && holds("data-temp-other", data, 150), "close of the renamed file");
    char plain_temporary[] = "plain-temp-XXXXXX";
    fd = mkstemp(plain_temporary);
    expect(fd >= 0 && write(fd, data, 10) == 10 && close(fd) == 0 && unlink(plain_temporary) == 0,
           "mkstemp() of a file that is not monitored");

    /* A mapping counts as read when it is made, whole. Programs built with
       _FILE_OFFSET_BITS=64 make it with mmap64(). */
    prepare("data-mmap", data, 2 * BLOCK);
    fd = open("data-mmap", O_RDONLY);
    char *mapped = mmap(NULL, 2 * BLOCK, PROT_READ, MAP_PRIVATE, fd, 0);
    expect(fd >= 0 && mapped != MAP_FAILED && memcmp(mapped, data, 2 * BLOCK) == 0, "mmap() of data-mmap");
    expect(munmap(mapped, 2 * BLOCK) == 0 && close(fd) == 0, "munmap() and close of data-mmap");

    /* A descriptor duplicated three ways, each used after the one before it is
       closed. All share one position: the second read is of block 1. */
    prepare("data-dup", data, 2 * BLOCK);
    fd = open("data-dup", O_RDONLY);
    other = dup(fd);
    expect(fd >= 0 && other >= 0 && close(fd) == 0, "dup() of data-dup");
    expect(read(other, buf, BLOCK) == BLOCK, "read of block 0 through dup()");
    fd = fcntl(other, F_DUPFD_CLOEXEC, 100);
    expect(fd >= 100 && close(other) == 0, "fcntl(F_DUPFD_CLOEXEC) of data-dup");
    expect(read(fd, buf, BLOCK) == BLOCK, "read of block 1 through fcntl(F_DUPFD_CLOEXEC)");
    other = dup3(fd, 200, O_CLOEXEC);
    expect(other == 200 && close(fd) == 0, "dup3() of data-dup");
    expect(lseek(other, 0, SEEK_SET) == 0 && read(other, buf, 100) == 100 && close(other) == 0,
           "read of 100 bytes through dup3()");

    /* zlib is given a file this way: a stream on a duplicate. */
    prepare("data-dupstream", data, BLOCK);
    fd = open("data-dupstream", O_RDONLY);
    fp = fdopen(dup(fd), "r");
    expect(fp && close(fd) == 0 && fread(buf, 1, 1000, fp) == 1000 && fclose(fp) == 0, "stream on a dup() of data-dupstream");

    /* A program that starts another with its output in a file makes the file
       the child's standard output, in a child from vfork(), which runs in the
       parent's memory. The parent's own standard output is not the file. */
    fd = open("data-vfork", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    pid_t child = vfork();
    if (child == 0) {
        _exit(dup2(fd, STDOUT_FILENO) == STDOUT_FILENO ? 0 : 1);
    }
    int child_status = -1;
    expect(fd >= 0 && child > 0 && waitpid(child, &child_status, 0) == child && child_status == 0, "dup2() in a child from vfork()");
    expect(write(STDOUT_FILENO, "from the parent\n", 16) == 16, "write() to the parent's standard output");
    expect(write(fd, data, 100) == 100 && close(fd) == 0 && holds("data-vfork", data, 100), "write() of 100 bytes to data-vfork");

    /* A child that opens its output itself and closes the descriptor once the
       file is its standard output leaves nothing of it with the parent: the
       next file the parent opens gets that descriptor and is not the child's. */
    child = vfork();
    if (child == 0) {
        _exit(open_as_output("data-vfork-child") ? 0 : 1);
    }
    child_status = -1;
    expect(child > 0 && waitpid(child, &child_status, 0) == child && child_status == 0, "open(), dup2() and close() in a child from vfork()");
    fd = open("plain-after-vfork", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(fd >= 0 && write(fd, data, 50) == 50 && close(fd) == 0, "write() to a file that is not monitored, after the child");

    /* dup2() onto a monitored descriptor closes that file, and the descriptor is
       the other file's from then on. One that fails closes nothing. */
    prepare("data-dup2-a", data, BLOCK);
    prepare("data-dup2-b", data, BLOCK);
    fd = open("data-dup2-a", O_RDONLY);
    other = open("data-dup2-b", O_RDONLY);
    expect(fd >= 0 && other >= 0 && read(other, buf, 100) == 100, "100 bytes of data-dup2-b");
    errno = 0;
    expect(dup2(-1, other) == -1 && errno == EBADF, "dup2() of a descriptor that is not open");
    expect(dup2(other, other) == other, "dup2() of a descriptor onto itself");
    expect(dup2(fd, other) == other && read(other, buf, 200) == 200, "200 bytes of data-dup2-a through dup2()");
    expect(close(fd) == 0 && read(other, buf, 300) == 300 && close(other) == 0, "300 more after the first descriptor is closed");

    /* A program that puts a file on its own standard output with dup2(), and
       goes on with printf(). Afterwards the output goes where it went. */
    int saved_output = dup(STDOUT_FILENO);
    fd = open("data-dup2-stdout", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(saved_output >= 0 && fd >= 0 && fflush(stdout) == 0 && dup2(fd, STDOUT_FILENO) == STDOUT_FILENO && close(fd) == 0,
           "dup2() of data-dup2-stdout onto the standard output");
    expect(printf("%d bytes\n", 8) == 8 && fflush(stdout) == 0, "printf() to the file");
    expect(dup2(saved_output, STDOUT_FILENO) == STDOUT_FILENO && close(saved_output) == 0, "the standard output back");
    expect(holds("data-dup2-stdout", "8 bytes\n", 8), "data-dup2-stdout holds what was printed");

    /* The same with close() and open(), which gives the file the descriptor
       that was just closed. */
    saved_output = dup(STDOUT_FILENO);
    expect(saved_output >= 0 && fflush(stdout) == 0 && close(STDOUT_FILENO) == 0, "close() of the standard output");
    fd = open("data-opened-stdout", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    expect(fd == STDOUT_FILENO && printf("%d bytes\n", 8) == 8 && fflush(stdout) == 0, "printf() to a file opened as descriptor 1");
    expect(dup2(saved_output, STDOUT_FILENO) == STDOUT_FILENO && close(saved_output) == 0, "the standard output back again");

    /* A program's log and its standard output put on the log with dup2(): two
       streams on one file, each with a buffer of its own. */
    fp = fopen("data-two-streams", "w");
    saved_output = dup(STDOUT_FILENO);
    expect(fp && saved_output >= 0 && fflush(stdout) == 0 && dup2(fileno(fp), STDOUT_FILENO) == STDOUT_FILENO,
           "dup2() of the log onto the standard output");
    for (int i = 0; fp && i < 3; i++) {
        expect(fprintf(fp, "%s %d\n", "log line", i) == 11 && fflush(fp) == 0, "a line to the log");
        expect(printf("%s %d\n", "output line", i) == 14 && fflush(stdout) == 0, "a line to the standard output");
    }
    expect(dup2(saved_output, STDOUT_FILENO) == STDOUT_FILENO && close(saved_output) == 0 && fp && fclose(fp) == 0,
           "the standard output back, and fclose() of the log");
    expect(holds("data-two-streams", "log line 0\noutput line 0\nlog line 1\noutput line 1\nlog line 2\noutput line 2\n", 75),
           "data-two-streams holds the six lines");

    /* A stream whose descriptor dup2() made a monitored file's: closing the
       stream closes the file, and the next file with that descriptor is not it. */
    prepare("data-under-stream", data, BLOCK);
    prepare("plain-under-stream", data, BLOCK);
    other = open("plain-under-stream", O_RDONLY);
    fp = fdopen(other, "r");
    fd = open("data-under-stream", O_RDONLY);
    expect(fp && fd >= 0 && dup2(fd, other) == other && close(fd) == 0, "dup2() onto the descriptor of a stream");
    expect(read(other, buf, 100) == 100 && fclose(fp) == 0, "100 bytes of data-under-stream, and fclose() of the stream");
    fd = open("plain-under-stream", O_RDONLY);
    expect(fd == other && read(fd, buf, 50) == 50 && close(fd) == 0, "50 bytes of another file on the same descriptor");

    /* Vectors at an offset leave the position alone; preadv2() and pwritev2()
       with offset -1 use it. */
    struct iovec two[2] = {{data, BLOCK}, {data, 100}};
    struct iovec halves[2] = {{buf, 50}, {buf + 50, 50}};
    struct iovec one = {data, 20};
    struct iovec back = {buf, 30};
    fd = open("data-vecat", O_RDWR | O_CREAT | O_TRUNC, 0644);
    expect(fd >= 0 && pwritev(fd, two, 2, BLOCK) == BLOCK + 100, "pwritev() of 4196 bytes at block 1");
    expect(preadv(fd, halves, 2, BLOCK) == 100 && memcmp(buf, data, 100) == 0, "preadv() of 100 bytes at block 1");
    expect(write(fd, data, 10) == 10, "write() at the position, still 0");
    expect(pwritev2(fd, &one, 1, -1, 0) == 20, "pwritev2() at the position");
    expect(preadv2(fd, &back, 1, 0, 0) == 30 && memcmp(buf, data, 30) == 0, "preadv2() of 30 bytes at offset 0");
    expect(lseek(fd, 0, SEEK_CUR) == 30 && close(fd) == 0, "the position after the vectors is 30");

    /* Copies made in the kernel, from and to the position and an offset, and
       with a file that is not monitored at either end. */
    prepare("data-copy-src", data, 3 * BLOCK);
    prepare("plain-source", data, BLOCK);
    int src = open("data-copy-src", O_RDONLY);
    int out1 = open("data-copy-out1", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int out2 = open("data-copy-out2", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int plain_out = open("plain-copy", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int plain_in = open("plain-source", O_RDONLY);
    expect(src >= 0 && out1 >= 0 && out2 >= 0 && plain_out >= 0 && plain_in >= 0, "open of the files to copy");
    expect(sendfile(out1, src, NULL, 5000) == 5000, "sendfile() of 5000 bytes from the position");
    off_t offset = 2 * BLOCK;
    expect(sendfile(out1, src, &offset, 1000) == 1000 && offset == 2 * BLOCK + 1000, "sendfile() of 1000 bytes from block 2");
    expect(lseek(src, 0, SEEK_CUR) == 5000, "sendfile() from an offset leaves the position");
    expect(copy_file_range(src, NULL, out2, NULL, 100, 0) == 100, "copy_file_range() of 100 bytes from the position");
    off64_t from = 0, to = BLOCK;
    expect(copy_file_range(src, &from, out2, &to, 200, 0) == 200 && from == 200 && to == BLOCK + 200,
           "copy_file_range() of 200 bytes between offsets");
    expect(sendfile(plain_out, src, NULL, 10) == 10, "sendfile() to a file that is not monitored");
    from = 0;
    expect(copy_file_range(plain_in, &from, out2, NULL, 10, 0) == 10, "copy_file_range() from a file that is not monitored");
    expect(close(src) == 0 && close(out1) == 0 && close(out2) == 0 && close(plain_out) == 0 && close(plain_in) == 0,
           "close of the copied files");
    expect(holds("data-copy-out1", data, 6000) && holds("plain-copy", data, 10), "the copies hold what was copied");

    /* stdin on a file, read with every call that takes its stream for granted. */
    static const char input[] = "12 34\nhello\nthird line\nab";
    prepare("data-stdin", input, sizeof input - 1);
    int a = 0, b = 0;
    char *line = NULL;
    size_t size = 0;
    expect(freopen("data-stdin", "r", stdin) == stdin, "freopen() of stdin");
    expect(scanf("%d %d", &a, &b) == 2 && a == 12 && b == 34, "scanf() of two numbers");
    expect(getchar() == '\n', "getchar() of the newline");
    expect(getline(&line, &size, stdin) == 6 && strcmp(line, "hello\n") == 0, "getline() from stdin");
    expect(fgets(buf, 100, stdin) && strcmp(buf, "third line\n") == 0, "fgets() from stdin");
    expect(getchar() == 'a' && getchar() == 'b' && getchar() == EOF, "getchar() up to the end");
    expect(fclose(stdin) == 0, "fclose() of stdin");

    /* stdout on a file, likewise. The line before goes where stdout went. */
    expect(printf("%s stdout line\n", "plain") == 18 && fflush(stdout) == 0, "printf() before freopen()");
    expect(freopen("data-stdout", "w", stdout) == stdout, "freopen() of stdout");
    expect(printf("%d items\n", 7) == 8, "printf() of 8 bytes");
    expect(puts("done") >= 0, "puts() of 5 bytes");
    expect(putchar('!') == '!' && putchar('\n') == '\n', "putchar() of 2 bytes");
    expect(fputs("end\n", stdout) >= 0, "fputs() of 4 bytes to stdout");
    expect(fclose(stdout) == 0, "fclose() of stdout");
    expect(holds("data-stdout", "7 items\ndone\n!\nend\n", 19), "data-stdout holds what was printed");

    /* A stream moved to another file, then to the same file in another mode. */
    fp = fopen("data-reopen-a", "w");
    expect(fp && fprintf(fp, "%s\n", "first") == 6, "fprintf() to data-reopen-a");
    fp = freopen("data-reopen-b", "w", fp);
    expect(fp && fprintf(fp, "%s\n", "second!") == 8, "fprintf() to data-reopen-b");
    fp = freopen(NULL, "r", fp);
    expect(fp && fgets(buf, 100, fp) && strcmp(buf, "second!\n") == 0, "fgets() after freopen() for reading");
    expect(fp && fclose(fp) == 0, "fclose() of the reopened stream");
    expect(holds("data-reopen-a", "first\n", 6), "data-reopen-a holds its line");

    /* getc() and putc() as the calls they are: (getc) is not the macro. */
    prepare("data-getc", data, 5 * BLOCK);
    fp = fopen("data-getc", "r");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fopen of data-getc");
    long got = 0;
    while (fp && (getc)(fp) == 'd') {
        got++;
    }
    expect(got == 5 * BLOCK && fclose(fp) == 0, "getc() of the 5 blocks of data-getc");
    fp = fopen("data-putc", "w");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fopen of data-putc");
    for (int i = 0; fp && i < 6000; i++) {
        if ((putc)('d', fp) != 'd') {
            failures++;
        }
    }
    expect(fp && fclose(fp) == 0 && holds("data-putc", data, 6000), "putc() of 6000 bytes to data-putc");
#ifdef OLD_GETC
    prepare("data-oldgetc", data, BLOCK);
    fp = fopen("data-oldgetc", "r");
    got = 0;
    while (fp && old_getc(fp) == 'd') {
        got++;
    }
    expect(got == BLOCK && fclose(fp) == 0, "_IO_getc() of the block of data-oldgetc");
#endif

    /* Lines of 100 bytes. */
    static char lines[300];
    memset(lines, 'x', sizeof lines);
    lines[99] = lines[199] = lines[299] = '\n';
    prepare("data-lines", lines, sizeof lines);
    fp = fopen("data-lines", "r");
    expect(fp && getline(&line, &size, fp) == 100 && getline(&line, &size, fp) == 100, "getline() of two lines");
    expect(fp && getdelim(&line, &size, '\n', fp) == 100 && getline(&line, &size, fp) == -1, "getdelim() of the third");
    expect(fp && fclose(fp) == 0, "fclose of data-lines");

    /* Formatted input: 8 bytes, then the newline and a word. */
    static const char numbers[] = "10 20 30\nrest";
    int c = 0;
    prepare("data-scan", numbers, sizeof numbers - 1);
    fp = fopen("data-scan", "r");
    expect(fp && fscanf(fp, "%d %d %d", &a, &b, &c) == 3 && a + b + c == 60, "fscanf() of three numbers");
    expect(fp && fscanf(fp, "%99s", buf) == 1 && strcmp(buf, "rest") == 0, "fscanf() of a word");
    expect(fp && fscanf(fp, "%99s", buf) == EOF && fclose(fp) == 0, "fscanf() at the end of data-scan");

    prepare("data-scan-old", "1 2\n3 4\n", 8);
    fp = fopen("data-scan-old", "r");
    expect(fp && c89_fscanf(fp, "%d %d", &a, &b) == 2 && a + b == 3, "fscanf() as older programs call it");
    expect(fp && c99_fscanf(fp, "%d %d", &a, &b) == 2 && a + b == 7, "__isoc99_fscanf()");
    expect(fp && fclose(fp) == 0, "fclose of data-scan-old");

    /* Formatted output: 1000 lines of 8 bytes through more than one buffer. */
    fp = fopen("data-print", "w");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0, "fopen of data-print");
    for (int i = 0; fp && i < 1000; i++) {
        if (fprintf(fp, "%07d\n", i) != 8) {
            failures++;
        }
    }
    expect(fp && fclose(fp) == 0, "fclose of data-print");

    /* What the GNU tools write with. */
    fp = fopen("data-unlocked", "w");
    const char *volatile tail = "tail\n"; /* volatile: or the compiler calls fwrite_unlocked() */
    expect(fp && fwrite_unlocked(data, 1, 300, fp) == 300 && fputs_unlocked(tail, fp) >= 0, "fwrite_unlocked() and fputs_unlocked()");
    expect(fp && fclose(fp) == 0 && holds("data-unlocked", "dddd", 0) == 0, "fclose of data-unlocked");

    /* A position kept with fgetpos() and returned to: the bytes skipped on the
       way are not read. */
    fpos_t start;
    prepare("data-setpos", data, 3 * BLOCK);
    fp = fopen("data-setpos", "r");
    expect(fp && setvbuf(fp, stream_buffer, _IOFBF, BLOCK) == 0 && fgetpos(fp, &start) == 0, "fgetpos() at the start");
    expect(fp && fread(buf, 1, 100, fp) == 100 && fseek(fp, 2 * BLOCK, SEEK_SET) == 0 && fread(buf, 1, 100, fp) == 100,
           "100 bytes of blocks 0 and 2");
    expect(fp && fsetpos(fp, &start) == 0, "fsetpos() to the start");
    got = 0;
    while (fp && got < 50 && (getc)(fp) == 'd') {
        got++;
    }
    expect(got == 50 && fclose(fp) == 0, "50 bytes with getc() after fsetpos()");

    /* Lines printed with the stream locked, while another thread waits to
       print one. */
    shared = fopen("data-locked", "w");
    pthread_t printer;
    expect(shared && pthread_create(&printer, NULL, print_line, NULL) == 0, "start of the second thread");
    flockfile(shared);
    expect(fprintf(shared, "%049d\n", 1) == 50, "the first line, with the stream locked");
    shared_stage = 1;
    while (shared_stage != 2) {
        usleep(1000);
    }
    usleep(100000); /* the second thread is now waiting in fprintf() */
    expect(fprintf(shared, "%049d\n", 3) == 50, "another line, with the stream still locked");
    funlockfile(shared);
    void *line_failed = NULL;
    expect(pthread_join(printer, &line_failed) == 0 && line_failed == NULL, "the second thread's fprintf()");
    expect(fclose(shared) == 0, "fclose of data-locked");

    return failures != 0;
}
EOF

# A program seldom closes its standard streams. What it read and wrote through
# them a character at a time is counted all the same, when it exits.
cat >"$test_root/exit_open.c" <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 2 || chdir(argv[1]) != 0) {
        return 2;
    }
    static char input[6000];
    memset(input, 'i', sizeof input);
    int fd = (int)syscall(SYS_openat, AT_FDCWD, "data-exit-in", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || syscall(SYS_write, fd, input, sizeof input) != (long)sizeof input || syscall(SYS_close, fd) != 0) {
        return 1;
    }
    if (!freopen("data-exit-in", "r", stdin) || !freopen("data-exit-out", "w", stdout)) {
        return 1;
    }
    for (int i = 0; i < 3000; i++) {
        if (getchar() != 'i' || putchar('o') != 'o') {
            return 1;
        }
    }
    return 0; /* with both streams open */
}
EOF
cc -Wall -Wextra -Werror -O2 "$test_root/exit_open.c" -o "$test_root/exit_open"

# _IO_getc is no longer there to link with, but is still in the C library for
# older programs, under the version it had.
old_getc=()
libc=$(ldd "$(command -v cc)" 2>/dev/null | awk '/libc\.so/ {print $3; exit}' || true)
if [[ -n $libc ]] && command -v objdump >/dev/null 2>&1; then
    version=$(objdump -T "$libc" | awk '$NF == "_IO_getc" {gsub(/[()]/, "", $(NF-1)); print $(NF-1); exit}' || true)
    if [[ -n $version ]]; then
        old_getc=(-DOLD_GETC="\"$version\"")
    fi
fi

# name: flags, and functions the program must then call for the test to prove
# anything about them.
flavors=(plain fortified large)
declare -A flavor_flags=(
    [plain]="-O0"
    [fortified]="-O2 -D_FORTIFY_SOURCE=2"
    [large]="-O2 -U_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64"
)
declare -A flavor_symbols=(
    [plain]="creat mkstemp mkostemps mmap dup dup2 dup3 fcntl preadv pwritev preadv2 pwritev2 sendfile copy_file_range freopen getc putc getchar putchar getline getdelim fprintf printf puts fwrite_unlocked fputs_unlocked fsetpos"
    [fortified]="__printf_chk __fprintf_chk __getdelim getc putc"
    [large]="creat64 mkstemp64 mkostemps64 mmap64 fcntl64 preadv64 pwritev64 preadv64v2 pwritev64v2 sendfile64 freopen64 fsetpos64"
)
for flavor in "${flavors[@]}"; do
    # shellcheck disable=SC2086
    if ! cc -Wall -Wextra -Werror ${flavor_flags[$flavor]} -pthread "${old_getc[@]}" "$test_root/entry_points.c" \
        -o "$test_root/entry_points-$flavor" 2>"$test_root/compile-$flavor"; then
        if [[ ${#old_getc[@]} -gt 0 ]]; then
            # shellcheck disable=SC2086
            cc -Wall -Wextra -Werror ${flavor_flags[$flavor]} -pthread "$test_root/entry_points.c" -o "$test_root/entry_points-$flavor"
            old_getc=()
            echo "cannot link with _IO_getc; not testing it" >&2
        else
            cat "$test_root/compile-$flavor" >&2
            exit 1
        fi
    fi
    imports=$(nm -D --undefined-only "$test_root/entry_points-$flavor" | awk '{print $2}' | sed 's/@.*//')
    for symbol in ${flavor_symbols[$flavor]}; do
        if ! grep -qx -- "$symbol" <<<"$imports"; then
            echo "note: the $flavor program does not call $symbol with this compiler and C library" >&2
        fi
    done
done

# Prints the bytes of one timer entry from the [MONITOR] lines on stdout (builds
# without TIMER_JSON) or the timer's JSON file.
timer_bytes() {
    local run=$1 type=$2 call=$3
    local line json
    line=$(grep "^\[MONITOR\] $type $call " "$test_root/output-$run" | head -n 1 || true)
    if [[ -n $line ]]; then
        echo "$line" | awk '{print $6}'
        return
    fi
    json=$(compgen -G "$test_root/traces-$run/monitor_timer.*.json" | head -n 1 || true)
    if [[ -n $json ]] && command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import json, sys
timer = next(iter(json.load(open(sys.argv[1])).values()))
print(timer[sys.argv[2]][sys.argv[3]][2])' "$json" "$type" "$call"
    fi
}

# For each file and direction, the file's size when it was closed and its block
# touches: block, accesses, bytes of the block's first access.
#   data-creat      5000 bytes in one write: blocks 0 and 1
#   data-temp-result      300 bytes written under the name mkstemp() chose
#   data-temp-other       100 bytes under the name mkostemps() chose, 50 after the rename
#   data-dup        block 0, block 1 and 100 bytes of block 0 through three duplicates
#   data-vfork      100 bytes; the 16 the parent wrote to its standard output are not its
#   data-dup2-stdout   the 8 bytes printf() wrote after dup2() onto the standard output
#   data-two-streams   three lines of 11 bytes through the log's stream, three of 14 through stdout
#   data-under-stream  100 bytes; the 50 read from the next file on its descriptor are not its
#   data-dup2-a, b  100 bytes of b, then 200 and 300 of a through b's descriptor
#   data-vecat      written: 4196 bytes at block 1, then 10 and 20 at the position;
#                   read: 100 bytes at block 1, 30 at offset 0
#   data-copy-src   0-4999, 100 and 10 bytes from the position (5000, 5100), 1000
#                   from block 2 and 200 from offset 0
#   data-copy-out1  0-4999 and 5000-5999
#   data-copy-out2  100 and 10 bytes from the position, 200 at block 1
#   data-stdin      scanf (5), the getchar with getline (1, 6), fgets (11), the last
#                   two characters when the stream finds the end
#   data-stdout     printf (8), puts (5), the two characters with fputs (2, 4)
#   data-getc, data-putc  a buffer at a time, like the unlocked macros
#   data-scan       "10 20 30", then the newline and "rest"
#   data-scan-old   "1 2", then the newline and "3 4"
#   data-print      512 lines in block 0, 488 in block 1
#   data-setpos     100 bytes of blocks 0 and 2, then 50 of block 0 again
#   data-locked     three lines of 50 bytes
expected=(
    "data-creat w 5000 0 1 4096;1 1 904;"
    "data-temp-result w 300 0 1 300;"
    "data-temp-other w 150 0 2 100;"
    "data-dup r 8192 0 2 4096;1 1 4096;"
    "data-dupstream r 4096 0 1 1000;"
    "data-vfork w 100 0 1 100;"
    "data-dup2-stdout w 8 0 1 8;"
    "data-opened-stdout w 8 0 1 8;"
    "data-two-streams w 75 0 6 11;"
    "data-under-stream r 4096 0 1 100;"
    "data-dup2-a r 4096 0 2 200;"
    "data-dup2-b r 4096 0 1 100;"
    "data-vecat w 8292 0 2 10;1 1 4096;2 1 100;"
    "data-vecat r 8292 0 1 30;1 1 100;"
    "data-copy-src r 12288 0 2 4096;1 3 904;2 1 1000;"
    "data-copy-out1 w 6000 0 1 4096;1 2 904;"
    "data-copy-out2 w 4296 0 2 100;1 1 200;"
    "data-stdin r 25 0 5 5;"
    "data-stdout w 19 0 4 8;"
    "data-reopen-a w 6 0 1 6;"
    "data-reopen-b w 8 0 1 8;"
    "data-reopen-b r 8 0 1 8;"
    "data-getc r 20480 0 1 4096;1 1 4096;2 1 4096;3 1 4096;4 1 4096;"
    "data-putc w 6000 0 1 4096;1 1 1904;"
    "data-lines r 300 0 3 100;"
    "data-scan r 13 0 2 8;"
    "data-scan-old r 8 0 2 3;"
    "data-print w 8000 0 512 8;1 488 8;"
    "data-unlocked w 305 0 2 300;"
    "data-setpos r 12288 0 2 100;2 1 100;"
    "data-locked w 150 0 3 50;"
)
# The timer's bytes: reads 8192 (the mapping) + 8292 + 1000 + 100 + 600 + 6310 +
# 25 + 8 + 20480 + 300 + 13 + 7 + 250, writes 5000 + 300 + 150 + 100 + 8 + 8 +
# 75 + 10 + 6310 + 19 + 14 + 6000 + 8000 + 305 + 150, and the vectors apart.
bytes_read=45577
if [[ ${#old_getc[@]} -gt 0 ]]; then
    expected+=("data-oldgetc r 4096 0 1 4096;")
    bytes_read=$((bytes_read + 4096))
fi
expected_bytes=("read $bytes_read" "write 26449" "readv 130" "writev 4216")

for flavor in "${flavors[@]}"; do
    for mode in 1 0; do
        run="$flavor-$mode"
        mkdir -p "$test_root/traces-$run" "$test_root/work-$run"
        status=0
        timeout 60 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$test_root/traces-$run" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT="$mode" \
            "$test_root/entry_points-$flavor" "$test_root/work-$run" \
            >"$test_root/output-$run" 2>&1 || status=$?
        where="$flavor program, DATALIFE_JSON_OUTPUT=$mode"
        if [[ $status -eq 124 || $status -eq 142 ]]; then
            echo "the program did not finish: two threads printing to one locked stream block each other ($where)" >&2
            exit 1
        fi
        if [[ $status -ne 0 ]]; then
            echo "the program failed with status $status ($where):" >&2
            grep -v '^\[MONITOR\]' "$test_root/output-$run" | tail -n 20 >&2
            exit 1
        fi
        if ! grep -q 'plain stdout line$' "$test_root/output-$run"; then
            echo "what the program printed before it reopened stdout is lost ($where)" >&2
            exit 1
        fi

        for want in "${expected_bytes[@]}"; do
            got=$(timer_bytes "$run" monitor "${want% *}")
            if [[ -n $got && $got != "${want#* }" ]]; then
                echo "monitored ${want% *} bytes: '$got', expected '${want#* }' ($where)" >&2
                exit 1
            fi
        done

        for entry in "${expected[@]}"; do
            read -r name kind size rows <<<"$entry"
            if [[ $mode -eq 1 ]]; then
                trace=$(compgen -G "$test_root/traces-$run/$name.*.${kind}_blk_trace.json" | head -n 1 || true)
                if [[ -z $trace ]]; then
                    echo "no $kind trace was written for $name ($where)" >&2
                    exit 1
                fi
                if command -v python3 >/dev/null 2>&1; then
                    # access_frequency and data_volume: each block's accesses, and
                    # those times the bytes of its first one.
                    want=$(awk -v size="$size" 'BEGIN {RS = ";"} NF == 3 {accesses += $2; volume += $2 * $3} END {print accesses, volume, size}' <<<"$rows")
                    got=$(python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["access_frequency"], d["data_volume"], d["file_size_bytes"])' "$trace")
                    if [[ $got != "$want" ]]; then
                        echo "access_frequency, data_volume and file_size_bytes of $name ($kind): '$got', expected '$want' ($where)" >&2
                        exit 1
                    fi
                fi
            else
                stat=$(compgen -G "$test_root/traces-$run/${name}_*_${kind}_stat" | head -n 1 || true)
                got=$(tail -n +2 "${stat:-/dev/null}" | tr '\n' ';')
                if [[ $got != "$rows" ]]; then
                    echo "${kind}_stat rows of $name: '$got', expected '$rows' ($where)" >&2
                    exit 1
                fi
            fi
        done

        # The two files that match no pattern are not reported.
        if compgen -G "$test_root/traces-$run/plain-*" >/dev/null; then
            echo "a file that matches no pattern is reported ($where)" >&2
            exit 1
        fi
    done
done

for mode in 1 0; do
    run="exit-$mode"
    mkdir -p "$test_root/traces-$run" "$test_root/work-$run"
    status=0
    timeout 20 env \
        LD_PRELOAD="$monitor_lib" \
        DATALIFE_OUTPUT_PATH="$test_root/traces-$run" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT="$mode" \
        "$test_root/exit_open" "$test_root/work-$run" \
        >"$test_root/output-$run" 2>&1 || status=$?
    if [[ $status -ne 0 ]]; then
        echo "the program that leaves its standard streams open failed with status $status (DATALIFE_JSON_OUTPUT=$mode)" >&2
        exit 1
    fi
    for want in "read 3000" "write 3000"; do
        got=$(timer_bytes "$run" monitor "${want% *}")
        if [[ -n $got && $got != "${want#* }" ]]; then
            echo "monitored ${want% *} bytes of standard streams left open: '$got', expected '${want#* }' (DATALIFE_JSON_OUTPUT=$mode)" >&2
            exit 1
        fi
    done
done

echo "I/O through the C library's other entry points is monitored"