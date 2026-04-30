# DataLife Agent Layer

AI-agent-friendly interface to the DataLife flow-monitor profiler.

## Components

- **build.sh** — Builds `libmonitor.so` from the DataLife C++ source. Requires CMake.
- **run_profiling.py** — Wraps LD_PRELOAD-based profiling for arbitrary commands.
- **parse_traces.py** — Validates a trace directory against the widget-expected JSON schemas.
- **smoke_test.py** — pytest-based fixture validation using `mytest/` sample data.
- **schema/** — JSON Schema definitions for each trace file type.

## Output Format

DataLife produces three file types per profiled process:
1. `<filename>.<pid>-<host>.r_blk_trace.json` — read block ranges
2. `<filename>.<pid>-<host>.w_blk_trace.json` — write block ranges
3. `monitor_timer.<pid>-<host>.datalife.json` — timing/byte statistics

These are consumed directly by `widget-v1/src/dfl_mcp/data_parser.py::TraceParser`.

## Usage

```bash
# Build
bash agent/build.sh

# Profile a workload
python agent/run_profiling.py --cmd "python my_app.py" --output-dir /tmp/traces

# Validate traces
python agent/parse_traces.py /tmp/traces
```
