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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-descriptor-records.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# What the library knows of a descriptor must stay true to what the descriptor
# is, through everything a program may do to it:
#  - a log that one thread rotates, with freopen() or with dup2(), while other
#    threads write to it;
#  - a stream that comes by a monitored file because dup2() put the file on its
#    descriptor, and the numbers of its descriptors given to other files later;
#  - a dup3() that fails;
#  - a vfork() child that redirects its output before it calls exec: it runs in
#    the parent's memory, and the parent's descriptors are not the child's;
#  - a child made with clone(), which has the records of its parent to itself;
#  - threads that take characters from one stream, or put them, at once;
#  - a thread in fflush(NULL), which goes through all the streams, while another
#    closes streams;
#  - calls that fail, and a write() made by a signal handler while the thread is
#    in the library;
#  - calls made before the library's constructor has run.
cat >"$test_root/records.c" <<'EOF_C'
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SIZE (1 << 20)
#define THREADS 4
#define LINE 21

static char data[SIZE];
static const char *volatile line = "a line of log output\n"; /* volatile: fputs() stays fputs() */
static FILE *log_fp;
static int log_fd;
static volatile int stop;
static long counts[THREADS];

#define CHECK(ok) do { if (!(ok)) { fprintf(stderr, "%s: line %d failed (%s)\n", name, __LINE__, strerror(errno)); return 1; } } while (0)
#define IS(what) (strcmp(name, what) == 0)

/* No thread uses fprintf() here: in glibc 2.39 a program that calls fprintf() and
   freopen() on one stream at once does not always end, whether the library is
   loaded or not. */
static void *stream_writer(void *arg) {
    long id = (long)arg, n = 0;
    while (!stop) {
        if (id == 0) n += fputs(line, log_fp) == EOF ? 0 : LINE;
        else if (id == 1) n += (long)fwrite(line, 1, LINE, log_fp);
        else if (id == 2) { for (int i = 0; i < LINE; i++) n += putc(line[i], log_fp) != EOF; }
        else { flockfile(log_fp); for (int i = 0; i < LINE; i++) n += putc_unlocked(line[i], log_fp) != EOF; funlockfile(log_fp); }
    }
    counts[id] = n;
    return NULL;
}

static void *descriptor_writer(void *arg) {
    long n = 0;
    while (!stop) {
        if (write(log_fd, line, LINE) != LINE) exit(4);
        n += LINE;
    }
    counts[(long)arg] = n;
    return NULL;
}

static void *taker(void *arg) {
    long n = 0;
    while (getc(log_fp) != EOF) n++;
    counts[(long)arg] = n;
    return NULL;
}

static void *putter(void *arg) {
    for (long n = 0; n < SIZE / THREADS; n++) {
        if (putc('a' + (int)(long)arg, log_fp) == EOF) exit(4);
    }
    counts[(long)arg] = SIZE / THREADS;
    return NULL;
}

static void *flusher(void *arg) {
    (void)arg;
    while (!stop) fflush(NULL);
    return NULL;
}

static int wake[2];
static void on_alarm(int sig) {
    (void)sig;
    int saved_errno = errno;
    if (write(wake[1], "x", 1) < 0) { /* the pipe is full: nobody reads it */
    }
    errno = saved_errno;
}

/* Leaves the bytes the threads wrote where the test finds them. */
static int report(long wrote) {
    FILE *fp = fopen("wrote", "w");
    return fp && fprintf(fp, "%ld\n", wrote) > 0 && fclose(fp) == 0;
}

/* Runs the function on four threads and returns the sum of what they counted. */
static long run_threads(void *(*function)(void *), int rounds, const char *name) {
    pthread_t threads[THREADS];
    long total = 0;
    for (long i = 0; i < THREADS; i++) {
        if (pthread_create(&threads[i], NULL, function, (void *)i) != 0) exit(4);
    }
    for (int i = 0; i < rounds; i++) { /* the rotation, for the cases that have one */
        usleep(200);
        const char *next = i % 2 ? "data-log" : "data-log.1";
        if (IS("rotate_freopen")) {
            if (!freopen(next, "w", log_fp)) exit(4);
        } else {
            int fd = open(next, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
            if (fd < 0 || dup2(fd, log_fd) != log_fd || close(fd) != 0) exit(4);
        }
    }
    stop = 1;
    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
        total += counts[i];
    }
    return total;
}

int main(int argc, char **argv) {
    if (argc != 4 || chdir(argv[2]) != 0) {
        return 2;
    }
    const char *name = argv[1];
    int rounds = atoi(argv[3]);
    memset(data, 'x', sizeof data);
    for (int i = 99; i < SIZE; i += 100) data[i] = '\n';

    if (IS("nothing")) {
        return 0;
    }
    if (IS("rotate_freopen")) {
        CHECK(log_fp = fopen("data-log", "w"));
        long wrote = run_threads(stream_writer, rounds, name);
        CHECK(fclose(log_fp) == 0 && report(wrote));
        return 0;
    }
    if (IS("rotate_dup2")) {
        CHECK((log_fd = open("data-log", O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644)) >= 0);
        long wrote = run_threads(descriptor_writer, rounds, name);
        CHECK(close(log_fd) == 0 && report(wrote));
        return 0;
    }
    if (IS("borrowed_stream")) { /* a line and a megabyte to data-a through the stream of another file, then four megabytes to files that are not monitored */
        int a, plain1, plain2;
        FILE *fp;
        CHECK((a = open("data-a", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        CHECK(fp = fopen("plain-log", "w"));
        CHECK(fputs(line, fp) != EOF); /* waits in the stream's buffer, and goes to data-a with the rest */
        int borrowed = fileno(fp);
        CHECK(dup2(a, borrowed) == borrowed);
        CHECK(fwrite(data, 1, SIZE, fp) == SIZE && fclose(fp) == 0 && close(a) == 0);
        /* The numbers go to other files. */
        CHECK((plain1 = open("plain-1", O_WRONLY | O_CREAT | O_TRUNC, 0644)) == a);
        CHECK((plain2 = open("plain-2", O_WRONLY | O_CREAT | O_TRUNC, 0644)) == borrowed);
        for (int i = 0; i < 2; i++) CHECK(write(plain1, data, SIZE) == SIZE && write(plain2, data, SIZE) == SIZE);
        CHECK(close(plain1) == 0 && close(plain2) == 0);
        return 0;
    }
    if (IS("stdout_twice")) { /* standard output on data-a with dup2(), then on data-b with freopen(): half a megabyte each */
        int a;
        CHECK((a = open("data-a", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0 && dup2(a, 1) == 1 && close(a) == 0);
        CHECK(write(1, data, SIZE / 4) == SIZE / 4);
        CHECK(fwrite(data, 1, SIZE / 4, stdout) == SIZE / 4);
        CHECK(freopen("data-b", "w", stdout));
        CHECK(fwrite(data, 1, SIZE / 2, stdout) == SIZE / 2 && fclose(stdout) == 0);
        return 0;
    }
    if (IS("dup3_flags")) { /* a dup3() that fails leaves the descriptor it names as it was */
        int a, plain;
        CHECK((a = open("data-a", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        CHECK((plain = open("plain-1", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        CHECK(dup3(plain, a, 0x1234) == -1 && errno == EINVAL);
        CHECK(write(a, data, SIZE) == SIZE && close(a) == 0 && close(plain) == 0);
        return 0;
    }
    if (IS("vfork_child")) { /* the child's output goes to data-a; the parent's own stays where it was */
        int a, status;
        CHECK((a = open("data-a", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        pid_t child = vfork();
        if (child == 0) {
            dup2(a, 1);
            close(a);
            int extra = open("data-child", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            close(extra);
            execl("/bin/echo", "echo", "from the child", (char *)NULL);
            _exit(127);
        }
        CHECK(child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(write(1, data, SIZE) == SIZE);          /* not monitored */
        CHECK(write(a, data, SIZE / 2) == SIZE / 2);  /* monitored */
        /* The number the child gave to its own file goes to a file that is not monitored. */
        int plain;
        CHECK((plain = open("plain-1", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        CHECK(write(plain, data, SIZE) == SIZE && close(plain) == 0 && close(a) == 0);
        return 0;
    }
    if (IS("clone_child")) { /* the child closes data-a, its number goes to a plain file, and it writes data-b; then the parent writes data-a */
        int a, status;
        if (syscall(SYS_kcmp, getpid(), getpid(), 1 /* KCMP_VM */, 0, 0) != 0) {
            return 77; /* the kernel does not let a process be told from a vfork() child */
        }
        CHECK((a = open("data-a", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
        pid_t child = (pid_t)syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0);
        if (child == 0) {
            int plain, b;
            CHECK(close(a) == 0 && (plain = open("plain-1", O_WRONLY | O_CREAT | O_TRUNC, 0644)) == a);
            CHECK(write(plain, data, SIZE) == SIZE && close(plain) == 0);
            CHECK((b = open("data-b", O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0);
            CHECK(write(b, data, SIZE / 2) == SIZE / 2 && close(b) == 0);
            return 0;
        }
        CHECK(child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(write(a, data, SIZE / 4) == SIZE / 4 && close(a) == 0);
        return 0;
    }
    if (IS("flush_all")) { /* streams opened, written and closed while another thread flushes them all */
        pthread_t thread;
        CHECK(pthread_create(&thread, NULL, flusher, NULL) == 0);
        for (int i = 0; i < SIZE / 4096; i++) {
            FILE *fp;
            CHECK(fp = fopen("data-a", i ? "a" : "w"));
            CHECK(fwrite(data, 1, 4096, fp) == 4096 && fclose(fp) == 0);
        }
        stop = 1;
        pthread_join(thread, NULL);
        return 0;
    }
    if (IS("failures")) { /* calls that fail change neither what the program sees nor what is counted */
        FILE *fp;
        int fd;
        CHECK(symlink("/dev/full", "data-full") == 0);
        CHECK(fp = fopen("data-full", "w"));
        CHECK(fputs(line, fp) != EOF);
        errno = 0;
        CHECK(fclose(fp) == EOF && errno == ENOSPC); /* the line could not be written */
        CHECK((fd = open("data-full", O_WRONLY)) >= 0);
        errno = 0;
        CHECK(write(fd, data, 4096) == -1 && errno == ENOSPC && close(fd) == 0);
        CHECK(fp = fopen("data-a", "w"));
        CHECK(fwrite(data, 1, SIZE, fp) == SIZE);
        CHECK(fgetc(fp) == EOF && ferror(fp)); /* the stream is not open for reading */
        rewind(fp);
        CHECK(!ferror(fp) && fclose(fp) == 0);
        return 0;
    }
    if (IS("signal_handler")) { /* a handler that writes to a pipe, as programs do to wake their main loop, 10000 times a second */
        struct sigaction action;
        struct itimerval interval = {{0, 100}, {0, 100}};
        int fd;
        CHECK(pipe2(wake, O_NONBLOCK) == 0 && (fd = open("plain-1", O_RDWR | O_CREAT | O_TRUNC, 0644)) >= 0);
        memset(&action, 0, sizeof action);
        action.sa_handler = on_alarm;
        action.sa_flags = SA_RESTART;
        CHECK(sigaction(SIGALRM, &action, NULL) == 0 && setitimer(ITIMER_REAL, &interval, NULL) == 0);
        for (time_t end = time(NULL) + 2; time(NULL) < end;) {
            CHECK(pwrite(fd, data, 512, 0) == 512 && pread(fd, data, 512, 0) == 512);
        }
        memset(&interval, 0, sizeof interval);
        CHECK(setitimer(ITIMER_REAL, &interval, NULL) == 0 && close(fd) == 0);
        return 0;
    }
    if (IS("getc_threads") || IS("putc_threads")) {
        int reading = IS("getc_threads");
        if (reading) {
            int fd = (int)syscall(SYS_openat, AT_FDCWD, "data-a", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            CHECK(fd >= 0 && syscall(SYS_write, fd, data, SIZE) == SIZE && syscall(SYS_close, fd) == 0);
        }
        CHECK(log_fp = fopen("data-a", reading ? "r" : "w"));
        stop = 1;
        CHECK(run_threads(reading ? taker : putter, 0, name) == SIZE && fclose(log_fp) == 0);
        return 0;
    }
    fprintf(stderr, "no case %s\n", name);
    return 2;
}
EOF_C

# A library whose constructor runs before the monitor's when it is preloaded
# after it, and uses the calls the monitor wraps.
cat >"$test_root/early.c" <<'EOF_C'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>

__attribute__((constructor)) static void early(void) {
    const char *directory = getenv("EARLY_DIRECTORY");
    char *line = NULL;
    size_t room = 0;
    struct iovec iov = {(void *)"vector\n", 7};
    if (!directory || chdir(directory) != 0) abort();
    int fd = creat("data-early", 0644);
    int copy = dup(fd);
    if (fd < 0 || copy < 0 || dup2(fd, copy) != copy || fcntl(fd, F_GETFD) < 0) abort();
    if (write(fd, "early\n", 6) != 6 || pwritev(fd, &iov, 1, 6) != 7 || close(copy) != 0 || close(fd) != 0) abort();
    FILE *fp = fopen("data-early", "r+");
    if (!fp || getc(fp) != 'e' || getline(&line, &room, fp) != 5 || fseek(fp, 0, SEEK_END) != 0) abort();
    if (fprintf(fp, "%d\n", 1) != 2 || putc('x', fp) != 'x' || fputs("y\n", fp) == EOF || fflush(fp) != 0) abort();
    if (!(fp = freopen("data-early", "r", fp)) || fclose(fp) != 0) abort();
    free(line);
    fd = open("data-early", O_RDONLY);
    if (fd < 0 || lseek(fd, 0, SEEK_END) != 18 || close(fd) != 0) abort();
}
EOF_C

cc -Wall -Wextra -Werror -O2 -pthread "$test_root/records.c" -o "$test_root/records"
cc -Wall -Wextra -Werror -O2 -shared -fPIC "$test_root/early.c" -o "$test_root/libearly.so"

# Prints "read written", the bytes the timer shows for monitored files, from the
# [MONITOR] lines on stdout (builds without TIMER_JSON) or the timer's JSON file
# of the program itself (the child of the vfork() case writes one too).
timer_bytes() {
    local lines json
    lines=$(grep -a -E '^\[MONITOR\] monitor (read|write|readv|writev) ' "$test_root/output" | head -n 4 || true)
    if [[ -n $lines ]]; then
        echo "$lines" | awk '{bytes[$3] = $6} END {print bytes["read"] + bytes["readv"], bytes["write"] + bytes["writev"]}'
        return
    fi
    if command -v python3 >/dev/null 2>&1; then
        python3 -I -c '
import glob, json, sys
for path in glob.glob(sys.argv[1] + "/monitor_timer.*.json"):
    for program, timer in json.load(open(path)).items():
        if program.startswith("records"):
            monitor = timer["monitor"]
            print(monitor["read"][2] + monitor["readv"][2], monitor["write"][2] + monitor["writev"][2])' "$test_root/traces"
    fi
}

# Prints the last block (of 4096 bytes) that the write trace of a file shows as
# written: "none" if the file has no trace, nothing without python3.
traced_blocks() {
    local trace
    trace=$(compgen -G "$test_root/traces/$1.[0-9]*.w_blk_trace.json" | head -n 1 || true)
    if [[ -z $trace ]]; then
        echo none
    elif command -v python3 >/dev/null 2>&1; then
        python3 -I -c 'import json, sys; print(json.load(open(sys.argv[1]))["io_blk_range"][2])' "$trace"
    fi
}

# Runs one case of the program with the library and leaves its output in
# $test_root/output and its exit status in case_status. Several cases make
# threads wait for one another: a deadlock shows as a timeout.
run_case() {
    local name=$1 rounds=${2:-0} status=0
    rm -rf "$test_root/work" "$test_root/traces"
    mkdir -p "$test_root/work" "$test_root/traces"
    timeout 120 env \
        LD_PRELOAD="$monitor_lib${PRELOAD_AFTER:+ $PRELOAD_AFTER}" \
        EARLY_DIRECTORY="$test_root/work" \
        DATALIFE_OUTPUT_PATH="$test_root/traces" \
        DATALIFE_FILE_PATTERNS='data-*' \
        DATALIFE_JSON_OUTPUT=1 \
        "$test_root/records" "$name" "$test_root/work" "$rounds" \
        >"$test_root/output" 2>&1 || status=$?
    case_status=$status
    if [[ $status -ne 0 && ! (${ALLOW_SKIP:-0} -eq 1 && $status -eq 77) ]]; then
        echo "$name: the program failed with status $status with the library" >&2
        grep -av '^\[MONITOR\]' "$test_root/output" | cut -c 1-200 | tail -n 20 >&2
        exit 1
    fi
}

megabyte=1048576

# The rotations, several times each: the threads meet differently every time.
# With dup2() the record of the descriptor goes from the one file to the other
# in one step, so every byte the threads write is recorded, for one file or the
# other. With freopen() a thread can lose the one call it began while the C
# library had the stream between its two descriptors.
threads=4
line=21
for attempt in 1 2 3 4 5; do
    for name in rotate_freopen rotate_dup2; do
        run_case "$name" 200
        wrote=$(cat "$test_root/work/wrote")
        got=$(timer_bytes)
        recorded=${got#* }
        lost=0
        [[ $name == rotate_freopen ]] && lost=$((threads * 200 * line))
        if [[ $wrote -eq 0 || (-n $got && ($got != "0 $recorded" || $recorded -gt $wrote || $recorded -lt $((wrote - lost)))) ]]; then
            echo "$name: the threads wrote $wrote bytes and the library recorded '$got' (read written)" >&2
            exit 1
        fi
    done
done

# Each case, the bytes the timer must show (read written), and the last block
# of 4096 bytes in the write trace of data-a and of data-b ("-" for no check).
cases="
borrowed_stream 0 $((megabyte + 21)) 256 -
stdout_twice 0 $megabyte 127 127
dup3_flags 0 $megabyte 255 -
vfork_child 0 $((megabyte / 2)) 127 -
flush_all 0 $megabyte 255 -
failures 0 $((megabyte + 21)) 255 -
signal_handler 0 0 - -
getc_threads $megabyte 0 - -
putc_threads 0 $megabyte - -
"
while read -r name want_read want_written want_a want_b; do
    [[ -n $name ]] || continue
    run_case "$name"
    got=$(timer_bytes)
    if [[ -n $got && $got != "$want_read $want_written" ]]; then
        echo "$name: the library recorded '$got' bytes (read written), expected '$want_read $want_written'" >&2
        exit 1
    fi
    for file in a b; do
        want=$want_a
        [[ $file == b ]] && want=$want_b
        [[ $want != - ]] || continue
        got=$(traced_blocks "data-$file")
        if [[ -n $got && $got != "$want" ]]; then
            echo "$name: the write trace of data-$file ends at block '$got', expected $want" >&2
            exit 1
        fi
    done
    # The file the vfork() child opened and closed was the child's.
    if [[ $name == vfork_child ]] && compgen -G "$test_root/traces/data-child.*" >/dev/null; then
        echo "vfork_child: the parent reports a file that only its child opened" >&2
        exit 1
    fi
done <<<"$cases"

# A child made with clone() has no handler run for it as fork() has, and is
# another process than its parent all the same: it wrote half a megabyte to
# data-b and one to a file that is not monitored, the parent a quarter to
# data-a. Each reports its own (builds without TIMER_JSON show the child's).
# Skipped where the kernel does not offer kcmp(): the library cannot tell such
# a child from a vfork() child there, and leaves its records alone.
ALLOW_SKIP=1 run_case clone_child
if [[ $case_status -eq 77 ]]; then
    echo "clone_child: skipped, kcmp() is not available here" >&2
else
    got=$(timer_bytes | sort | tr '\n' ',')
    if [[ -n $got && $got != "0 $((megabyte / 4)),0 $((megabyte / 2))," && $got != "0 $((megabyte / 2))," ]]; then
        echo "clone_child: parent and child recorded '$got' bytes (read written), expected a quarter and half a megabyte written" >&2
        exit 1
    fi
    got=$(traced_blocks data-b)
    if [[ -n $got && $got != 127 ]]; then
        echo "clone_child: the write trace of the child's data-b ends at block '$got', expected 127" >&2
        exit 1
    fi
fi

# Calls made before the library's constructor go to the C library and are not
# recorded; the program must run as it does without the library.
PRELOAD_AFTER="$test_root/libearly.so" run_case nothing
if [[ $(cat "$test_root/work/data-early") != $'early\nvector\n1\nxy' ]]; then
    echo "the calls made before the library's constructor did not write what they write without it" >&2
    exit 1
fi

echo "descriptors that are replaced, borrowed, copied by a child or shared by threads are recorded as what they are"