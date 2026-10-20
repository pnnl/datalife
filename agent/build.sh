#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATALIFE_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Building DataLife flow-monitor ==="

if ! command -v cmake &>/dev/null; then
    echo "ERROR: cmake not found. Install cmake >= 2.8."
    exit 1
fi

BUILD_DIR="$DATALIFE_ROOT/build"
cmake -S "$DATALIFE_ROOT" -B "$BUILD_DIR" -DENABLE_FlowMonitor=ON -DENABLE_FlowAnalysis=OFF
cmake --build "$BUILD_DIR" -j"$(nproc)"

LIB=$(find "$BUILD_DIR" -name 'libmonitor.so' -type f | head -1)
if [ -z "$LIB" ]; then
    echo "ERROR: libmonitor.so not produced."
    exit 1
fi

echo "Built successfully: $LIB"
