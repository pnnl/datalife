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

test_root=$(mktemp -d "${TMPDIR:-/tmp}/datalife-immediate-binding.XXXXXX")
trap 'rm -rf "$test_root"' EXIT

# With immediate binding every symbol the library references must resolve when
# it is loaded, so one that nothing defines stops every program from starting.
mkdir -p "$test_root/traces"
status=0
output=$(env \
    LD_BIND_NOW=1 \
    LD_PRELOAD="$monitor_lib" \
    DATALIFE_OUTPUT_PATH="$test_root/traces" \
    DATALIFE_FILE_PATTERNS='data-*' \
    /bin/echo started 2>&1) || status=$?
if [[ $status -ne 0 || $output != started* ]]; then
    echo "a program did not start with the library preloaded and LD_BIND_NOW=1 (status $status):" >&2
    echo "$output" | grep -v '^\[MONITOR\]' | tail -n 20 >&2
    exit 1
fi

cat >"$test_root/load_now.c" <<'EOF'
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    if (!dlopen(argv[1], RTLD_NOW | RTLD_LOCAL)) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    return 0;
}
EOF
cc -Wall -Wextra -Werror "$test_root/load_now.c" -o "$test_root/load_now" -ldl

# From a directory of its own: at the exit of a program that loaded the library
# with dlopen(), the library's strings are destroyed before it writes the timer's
# file, which must still go to the output directory and not, under a name made
# of what was left of the path, to the current one.
mkdir -p "$test_root/work"
status=0
(cd "$test_root/work" && DATALIFE_OUTPUT_PATH="$test_root/traces" "$test_root/load_now" "$monitor_lib") \
    >"$test_root/output" 2>&1 || status=$?
if [[ $status -ne 0 ]]; then
    echo "dlopen(RTLD_NOW) of the library failed:" >&2
    grep -v '^\[MONITOR\]' "$test_root/output" | tail -n 20 >&2
    exit 1
fi
if [[ -n $(ls -A "$test_root/work") ]]; then
    echo "a program that loaded the library with dlopen() left a file in its directory:" >&2
    ls -Ab "$test_root/work" >&2
    exit 1
fi
# The timer's totals are on stdout, or with TIMER_JSON in a file of the output
# directory.
if ! grep -q '^\[MONITOR\] monitor read ' "$test_root/output" &&
    ! compgen -G "$test_root/traces/monitor_timer.*.json" >/dev/null; then
    echo "a program that loaded the library with dlopen() wrote no timer output" >&2
    exit 1
fi

echo "every symbol the library references resolves at load time"