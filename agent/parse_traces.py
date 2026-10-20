#!/usr/bin/env python3
"""Validate a directory of DataLife trace files against widget-expected schemas."""

import json
import os
import re
import sys


BLOCK_TRACE_PATTERN = re.compile(r'(.*)\.(\d+)-([\w\.-]+)\.(r|w)_blk_trace\.json')
DATALIFE_TRACE_PATTERN = re.compile(r'monitor_timer\.(\d+)-([\w\.-]+)\.datalife\.json')


def validate_block_trace(filepath):
    with open(filepath) as f:
        data = json.load(f)
    if "io_blk_range" not in data:
        return False, "missing 'io_blk_range' key"
    blk = data["io_blk_range"]
    if not isinstance(blk, list) or len(blk) < 2:
        return False, f"io_blk_range should be a list with >= 2 elements, got {type(blk).__name__} len={len(blk) if isinstance(blk, list) else 'N/A'}"
    return True, "valid"


def validate_datalife_trace(filepath):
    with open(filepath) as f:
        data = json.load(f)
    if not isinstance(data, dict) or len(data) == 0:
        return False, "expected non-empty dict"
    process_name = list(data.keys())[0]
    proc = data[process_name]
    if "monitor" not in proc:
        return False, f"missing 'monitor' key under '{process_name}'"
    monitor = proc["monitor"]
    for key in ("read", "write"):
        if key in monitor:
            val = monitor[key]
            if not isinstance(val, list) or len(val) != 3:
                return False, f"monitor.{key} should be [time, count, bytes], got {val}"
    return True, "valid"


def scan_directory(dir_path):
    results = {"r_blk_trace": [], "w_blk_trace": [], "datalife": [], "unknown": []}

    for fname in sorted(os.listdir(dir_path)):
        if not fname.endswith(".json"):
            continue
        fpath = os.path.join(dir_path, fname)

        blk_match = BLOCK_TRACE_PATTERN.match(fname)
        if blk_match:
            op = "r_blk_trace" if blk_match.group(4) == "r" else "w_blk_trace"
            ok, msg = validate_block_trace(fpath)
            results[op].append({"file": fname, "valid": ok, "message": msg})
            continue

        dl_match = DATALIFE_TRACE_PATTERN.match(fname)
        if dl_match:
            ok, msg = validate_datalife_trace(fpath)
            results["datalife"].append({"file": fname, "valid": ok, "message": msg})
            continue

        results["unknown"].append(fname)

    return results


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <trace_directory>", file=sys.stderr)
        sys.exit(1)

    dir_path = sys.argv[1]
    if not os.path.isdir(dir_path):
        print(f"ERROR: {dir_path} is not a directory", file=sys.stderr)
        sys.exit(1)

    results = scan_directory(dir_path)

    total = 0
    valid = 0
    for category in ("r_blk_trace", "w_blk_trace", "datalife"):
        entries = results[category]
        print(f"\n{category}: {len(entries)} file(s)")
        for e in entries:
            status = "OK" if e["valid"] else "FAIL"
            print(f"  [{status}] {e['file']}: {e['message']}")
            total += 1
            if e["valid"]:
                valid += 1

    print(f"\nSummary: {valid}/{total} valid")
    print(f"  r_blk_trace: {len(results['r_blk_trace'])}")
    print(f"  w_blk_trace: {len(results['w_blk_trace'])}")
    print(f"  datalife:    {len(results['datalife'])}")

    if valid < total:
        sys.exit(1)
    if len(results["r_blk_trace"]) == 0 and len(results["w_blk_trace"]) == 0:
        print("WARNING: no block trace files found", file=sys.stderr)
        sys.exit(1)
    if len(results["datalife"]) == 0:
        print("WARNING: no datalife timer files found", file=sys.stderr)
        sys.exit(1)

    sys.exit(0)


if __name__ == "__main__":
    main()
