#!/usr/bin/env python3
"""Run a command under DataLife LD_PRELOAD profiling and collect trace outputs."""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile


def find_libmonitor(explicit_path=None):
    if explicit_path and os.path.isfile(explicit_path):
        return os.path.abspath(explicit_path)
    agent_dir = os.path.dirname(os.path.abspath(__file__))
    datalife_root = os.path.dirname(agent_dir)
    build_lib = os.path.join(datalife_root, "build", "flow-monitor", "src", "libmonitor.so")
    if os.path.isfile(build_lib):
        return build_lib
    print("ERROR: libmonitor.so not found. Run build.sh first, or pass --lib-path.", file=sys.stderr)
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Run a command under DataLife profiling")
    parser.add_argument("--cmd", required=True, help="Command to profile (quoted)")
    parser.add_argument("--output-dir", required=True, help="Directory to collect trace outputs")
    parser.add_argument("--lib-path", default=None, help="Path to libmonitor.so")
    parser.add_argument("--workflow-name", default="default", help="Workflow name for organizing output")
    parser.add_argument("--task-name", default=None, help="Task name tag")
    parser.add_argument("--file-patterns", default=None, help="DATALIFE_FILE_PATTERNS env var value")
    args = parser.parse_args()

    lib_path = find_libmonitor(args.lib_path)

    work_dir = tempfile.mkdtemp(prefix="datalife_run_")

    env = os.environ.copy()
    env["LD_PRELOAD"] = lib_path
    if args.file_patterns:
        env["DATALIFE_FILE_PATTERNS"] = args.file_patterns
    env["DATALIFE_OUTPUT_PATH"] = work_dir

    print(f"Profiling with libmonitor.so: {lib_path}")
    print(f"Working directory: {work_dir}")
    print(f"Command: {args.cmd}")

    result = subprocess.run(args.cmd, shell=True, env=env, cwd=work_dir)

    out_dir = os.path.join(args.output_dir, args.workflow_name)
    os.makedirs(out_dir, exist_ok=True)

    collected = 0
    for pattern in ["*blk_trace.json", "*.datalife.json"]:
        for src in glob.glob(os.path.join(work_dir, "**", pattern), recursive=True):
            dst = os.path.join(out_dir, os.path.basename(src))
            shutil.move(src, dst)
            collected += 1
    for src in glob.glob(os.path.join(os.getcwd(), "**", "*blk_trace.json"), recursive=True):
        dst = os.path.join(out_dir, os.path.basename(src))
        shutil.move(src, dst)
        collected += 1
    for src in glob.glob(os.path.join(os.getcwd(), "**", "*.datalife.json"), recursive=True):
        dst = os.path.join(out_dir, os.path.basename(src))
        shutil.move(src, dst)
        collected += 1

    print(f"Collected {collected} trace files into {out_dir}")
    if result.returncode != 0:
        print(f"WARNING: profiled command exited with code {result.returncode}", file=sys.stderr)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
