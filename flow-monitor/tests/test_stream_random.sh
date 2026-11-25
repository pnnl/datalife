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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-stream-random.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# Random sequences of stream operations on one file: streams opened in every
# mode, with fopen() or open() and fdopen(), buffered in every way; characters
# taken and put with getc_unlocked() and putc_unlocked() between fread(),
# fwrite(), fgetc(), fputc(), fgets(), getline(), fputs(), fprintf(), seeks
# (fsetpos() among them), flushes, ungetc() and another stream appending to the
# file. The program is
# built twice: optimized, where the two are macros that work in the stream's
# buffer, and not, where they are calls. Whatever the sequence, the program must do the same with the library
# as without it, and the library must record exactly the bytes the program read
# and wrote: no arrangement of buffers and positions may lose or repeat any.
cat >"$test_root/stream_random.c" <<'EOF_C'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned long long state;

static unsigned pick(unsigned n) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(state >> 33) % n;
}

static char buf[70000];
static char stream_buffer[65536];

int main(int argc, char **argv) {
    if (argc != 4) {
        return 2;
    }
    state = strtoull(argv[1], NULL, 10) * 2654435761ULL + 12345;
    const char *path = argv[2];

    /* What the timer must show: every byte read and written. */
    long bytes_read = 0, bytes_written = 0;

    FILE *fp = NULL;
    int can_read = 0, can_write = 0;
    int last = 0; /* 1 after a read, 2 after a write: a seek must come between the two */
    /* A character put back and not read again yet: 1 if the macro took it, 2 if
       a call with a wrapper did. The library records where the stream is in the
       file, so a character the macro took and ungetc() put back was not read; a
       call with a wrapper is recorded for what it returns, as it always was. */
    int put_back = 0, last_char = EOF, macro_read = 0, just_read = 0;
    /* A position of the stream kept with fgetpos(), for fsetpos() to go back to. */
    fpos_t saved_position;
    int position_saved = 0;

    int operations = 20 + pick(150);
    for (int i = 0; i < operations; i++) {
        if (!fp) {
            static const char *modes[] = {"r", "r+", "w", "w+", "a", "a+"};
            static const int flags[] = {O_RDONLY, O_RDWR, O_WRONLY | O_CREAT | O_TRUNC, O_RDWR | O_CREAT | O_TRUNC,
                                        O_WRONLY | O_CREAT | O_APPEND, O_RDWR | O_CREAT | O_APPEND};
            int mode = pick(6);
            if (mode < 2 && access(path, F_OK) != 0) {
                mode = 3;
            }
            if (pick(3) == 0) {
                int fd = open(path, flags[mode], 0644);
                fp = fd >= 0 ? fdopen(fd, modes[mode]) : NULL;
            } else {
                fp = fopen(path, modes[mode]);
            }
            if (!fp) {
                return 3;
            }
            can_read = mode != 2 && mode != 4;
            can_write = mode != 0;
            last = put_back = macro_read = just_read = 0;
            static const int sizes[] = {2, 16, 100, 4096, 65536};
            int buffering = pick(8);
            if (buffering < 5) {
                setvbuf(fp, pick(2) ? stream_buffer : NULL, _IOFBF, sizes[buffering]);
            } else if (buffering == 5) {
                setvbuf(fp, NULL, _IOLBF, 4096);
            } else if (buffering == 6) {
                setvbuf(fp, NULL, _IONBF, 0);
            }
            continue;
        }

        int may_put_back = just_read;
        just_read = 0;
        int what = pick(128);
        if (what < 45 && can_read) {
            if (last == 2) {
                fseek(fp, 0, SEEK_CUR);
                put_back = 0;
            }
            if (what < 30) { /* characters, with the macro */
                int count = 1 + pick(pick(4) ? 300 : 9000), got = 0;
                for (; got < count; got++) {
                    int c = getc_unlocked(fp);
                    if (c == EOF) {
                        break;
                    }
                    last_char = c;
                }
                if (got > 0) {
                    bytes_read += got - (put_back == 2);
                    put_back = 0;
                    /* Characters taken up to the end of the file are recorded there
                       and then: one put back after that is not taken off again. */
                    macro_read = got == count;
                    just_read = 1;
                }
            } else {
                size_t got = fread(buf, 1, 1 + pick(pick(4) ? 500 : 60000), fp);
                if (got > 0) {
                    bytes_read += got;
                    last_char = (unsigned char)buf[got - 1];
                    put_back = macro_read = 0;
                    just_read = 1;
                }
            }
            last = 1;
        } else if (what < 80 && can_write) {
            if (last == 1) {
                fseek(fp, 0, SEEK_CUR);
                put_back = 0;
            }
            if (what < 70) { /* characters, with the macro */
                int count = 1 + pick(pick(4) ? 300 : 9000), put = 0;
                for (; put < count; put++) {
                    if (putc_unlocked(pick(40) ? 'a' + pick(26) : '\n', fp) == EOF) {
                        break;
                    }
                }
                bytes_written += put;
            } else {
                size_t count = 1 + pick(pick(4) ? 500 : 60000);
                memset(buf, 'A' + pick(26), count);
                bytes_written += fwrite(buf, 1, count, fp);
            }
            last = 2;
        } else if (what < 90) { /* a seek */
            long offset = pick(pick(3) ? 20000 : 200000);
            switch (pick(7)) {
            case 0: rewind(fp); break;
            case 1: fseek(fp, offset, SEEK_SET); break;
            case 2: fseeko(fp, offset, SEEK_SET); break;
            case 3: fseek(fp, -(long)pick(3000), SEEK_END); break;
            case 4: /* fsetpos() to a position kept before, or to this one: a seek all the same */
            case 5:
                if (!position_saved || pick(3) == 0) {
                    position_saved = fgetpos(fp, &saved_position) == 0;
                }
                if (position_saved) {
                    fsetpos(fp, &saved_position);
                }
                break;
            default: fseeko(fp, (long)pick(2000) - 1000, SEEK_CUR); break;
            }
            last = put_back = 0;
        } else if (what < 95) {
            fflush(fp);
            if (last == 2) {
                last = 0;
            }
            put_back = 0;
        } else if (what < 97) {
            (void)ftell(fp);
        } else if (what < 100) {
            fclose(fp);
            fp = NULL;
            position_saved = 0;
        } else if (what < 104 && can_read) { /* fgetc() */
            if (last == 2) {
                fseek(fp, 0, SEEK_CUR);
                put_back = 0;
            }
            int c = fgetc(fp);
            if (c != EOF) {
                bytes_read++;
                last_char = c;
                put_back = macro_read = 0;
                just_read = 1;
            }
            last = 1;
        } else if (what < 108 && can_read && may_put_back && !put_back) { /* what was read, or something else */
            if (ungetc(pick(4) ? last_char : 'Z', fp) != EOF) {
                put_back = macro_read ? 1 : 2;
                bytes_read -= macro_read;
            }
        } else if (what < 112 && can_write) { /* fputc(), fputs() */
            if (last == 1) {
                fseek(fp, 0, SEEK_CUR);
                put_back = 0;
            }
            if (pick(2)) {
                bytes_written += fputc('q', fp) != EOF;
            } else {
                const char *volatile text = "some text\n"; /* volatile: or the compiler calls fwrite() */
                if (fputs(text, fp) != EOF) {
                    bytes_written += 10;
                }
            }
            last = 2;
        } else if (what >= 116 && what < 124 && can_read && !put_back) { /* a line */
            if (last == 2) {
                fseek(fp, 0, SEEK_CUR);
            }
            if (what < 120) { /* fgets() does not tell its length if the line holds a NUL: the position does */
                long before = ftell(fp);
                if (fgets(buf, 2 + pick(5000), fp)) {
                    bytes_read += ftell(fp) - before;
                }
            } else {
                static char *line;
                static size_t room;
                ssize_t got = getline(&line, &room, fp);
                if (got > 0) {
                    bytes_read += got;
                }
            }
            macro_read = 0;
            last = 1;
        } else if (what >= 124 && can_write) { /* fprintf() */
            if (last == 1) {
                fseek(fp, 0, SEEK_CUR);
                put_back = 0;
            }
            int printed = fprintf(fp, "%u %s\n", pick(100000), "words");
            if (printed > 0) {
                bytes_written += printed;
            }
            last = 2;
        } else if (what >= 112 && what < 116) { /* another stream makes the file longer */
            FILE *other = fopen(path, "a");
            size_t count = 1 + pick(3000);
            memset(buf, '#', count);
            if (other) {
                bytes_written += fwrite(buf, 1, count, other);
                fclose(other);
            }
        }
    }
    if (fp) {
        fclose(fp);
    }

    FILE *totals = fopen(argv[3], "w");
    if (!totals) {
        return 3;
    }
    fprintf(totals, "%ld %ld\n", bytes_read, bytes_written);
    return fclose(totals) != 0;
}
EOF_C

# Optimized, for the macros, and not, for the calls; not fortified, as in
# test_stream_inline_io.sh.
cc -Wall -Wextra -Werror -O2 -U_FORTIFY_SOURCE "$test_root/stream_random.c" -o "$test_root/stream_random-macros"
cc -Wall -Wextra -Werror -O0 -U_FORTIFY_SOURCE "$test_root/stream_random.c" -o "$test_root/stream_random-calls"

imports=$(nm -D --undefined-only "$test_root/stream_random-macros" | awk '{print $2}' | sed 's/@.*//')
if ! grep -qx __uflow <<<"$imports" || ! grep -qx __overflow <<<"$imports" || grep -qxE 'getc_unlocked|putc_unlocked' <<<"$imports"; then
    echo "this compiler does not inline getc_unlocked and putc_unlocked; nothing to test" >&2
    exit 0
fi
imports=$(nm -D --undefined-only "$test_root/stream_random-calls" | awk '{print $2}' | sed 's/@.*//')
if ! grep -qx getc_unlocked <<<"$imports" || ! grep -qx putc_unlocked <<<"$imports"; then
    echo "this compiler inlines getc_unlocked and putc_unlocked even without optimization; nothing to test" >&2
    exit 0
fi

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

skipped=0
for seed in $(seq 1 100); do
    for build in macros calls; do
        program="$test_root/stream_random-$build"
        rm -rf "$test_root/plain" "$test_root/work" "$test_root/traces"
        mkdir -p "$test_root/plain" "$test_root/work" "$test_root/traces"

        # Without the library first. A sequence the C library itself fails on (a few
        # do, in its handling of ungetc()) says nothing about the library.
        if ! { "$program" "$seed" "$test_root/plain/data-random" "$test_root/plain/totals"; } >/dev/null 2>&1; then
            skipped=$((skipped + 1))
            continue
        fi
        status=0
        timeout 20 env \
            LD_PRELOAD="$monitor_lib" \
            DATALIFE_OUTPUT_PATH="$test_root/traces" \
            DATALIFE_FILE_PATTERNS='data-*' \
            DATALIFE_JSON_OUTPUT=1 \
            "$program" "$seed" "$test_root/work/data-random" "$test_root/work/totals" \
            >"$test_root/output" 2>&1 || status=$?
        if [[ $status -ne 0 ]]; then
            echo "sequence $seed ($build) failed with status $status with the library" >&2
            grep -av '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
            exit 1
        fi
        if ! cmp -s "$test_root/plain/totals" "$test_root/work/totals" ||
            ! cmp -s "$test_root/plain/data-random" "$test_root/work/data-random"; then
            echo "sequence $seed ($build): the program read or wrote something else with the library than without it" >&2
            exit 1
        fi
        want=$(cat "$test_root/work/totals")
        got=$(timer_bytes)
        if [[ -n $got && $got != "$want" ]]; then
            echo "sequence $seed ($build): the library recorded '$got' bytes (read written), the program read and wrote '$want'" >&2
            exit 1
        fi
    done
done

if ((skipped > 20)); then
    echo "$skipped of the 200 sequences fail without the library: too few are left to say anything" >&2
    exit 1
fi

echo "random sequences of stream operations are recorded byte for byte ($((200 - skipped)) sequences)"