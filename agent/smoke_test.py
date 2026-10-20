"""Fixture-based smoke tests for DataLife agent layer."""

import json
import os
import re
import subprocess
import sys

import pytest

AGENT_DIR = os.path.dirname(os.path.abspath(__file__))
DATALIFE_ROOT = os.path.dirname(AGENT_DIR)
MYTEST_DIR = os.path.join(DATALIFE_ROOT, "mytest")
WIDGET_ROOT = os.path.dirname(os.path.dirname(DATALIFE_ROOT))
WORKFLOW_TRACES = os.path.join(WIDGET_ROOT, "workflow_traces")

BLOCK_TRACE_PATTERN = re.compile(r'(.*)\.(\d+)-([\w\.-]+)\.(r|w)_blk_trace\.json')
DATALIFE_TRACE_PATTERN = re.compile(r'monitor_timer\.(\d+)-([\w\.-]+)\.datalife\.json')

# mytest fixtures use simplified naming (no hostname), so match them separately
MYTEST_BLOCK_PATTERN = re.compile(r'(.*)\.(\d+)\.(r|w)_blk_trace\.json')
MYTEST_DATALIFE_PATTERN = re.compile(r'monitor_timer\.(\d+)\.datalife\.json')


def _json_files(directory=None):
    d = directory or MYTEST_DIR
    return [f for f in os.listdir(d) if f.endswith(".json")]


class TestFixturePresence:
    def test_has_read_block_trace(self):
        matches = [f for f in _json_files() if ".r_blk_trace." in f]
        assert len(matches) >= 1

    def test_has_write_block_trace(self):
        matches = [f for f in _json_files() if ".w_blk_trace." in f]
        assert len(matches) >= 1

    def test_has_datalife_trace(self):
        matches = [f for f in _json_files() if ".datalife.json" in f]
        assert len(matches) >= 1


class TestBlockTraceSchema:
    @pytest.fixture(params=[f for f in os.listdir(MYTEST_DIR) if "blk_trace.json" in f])
    def trace_file(self, request):
        return os.path.join(MYTEST_DIR, request.param)

    def test_has_io_blk_range(self, trace_file):
        with open(trace_file) as f:
            data = json.load(f)
        assert "io_blk_range" in data
        assert isinstance(data["io_blk_range"], list)
        assert len(data["io_blk_range"]) >= 2


class TestDatalifeTraceSchema:
    @pytest.fixture(params=[f for f in os.listdir(MYTEST_DIR) if "datalife.json" in f])
    def trace_file(self, request):
        return os.path.join(MYTEST_DIR, request.param)

    def test_has_monitor_section(self, trace_file):
        with open(trace_file) as f:
            data = json.load(f)
        assert isinstance(data, dict) and len(data) > 0
        process_name = list(data.keys())[0]
        assert "monitor" in data[process_name]
        monitor = data[process_name]["monitor"]
        for key in ("read", "write"):
            if key in monitor:
                assert isinstance(monitor[key], list) and len(monitor[key]) == 3


class TestRegexCompatibility:
    """Verify that real widget workflow_traces match the widget regex patterns."""

    @pytest.mark.skipif(
        not os.path.isdir(os.path.join(WORKFLOW_TRACES, "ddmd")),
        reason="No workflow_traces/ddmd directory available"
    )
    def test_real_traces_match_widget_regex(self):
        trace_dir = None
        for d in os.listdir(os.path.join(WORKFLOW_TRACES, "ddmd")):
            full = os.path.join(WORKFLOW_TRACES, "ddmd", d)
            if os.path.isdir(full):
                trace_dir = full
                break
        assert trace_dir is not None
        matched = 0
        for fname in _json_files(trace_dir):
            if "blk_trace" in fname:
                assert BLOCK_TRACE_PATTERN.match(fname), f"{fname} does not match widget block_trace_pattern"
                matched += 1
            elif "datalife.json" in fname:
                assert DATALIFE_TRACE_PATTERN.match(fname), f"{fname} does not match widget datalife_trace_pattern"
                matched += 1
        assert matched >= 1

    def test_mytest_block_traces_valid_structure(self):
        """mytest fixtures have valid JSON structure even though naming is simplified."""
        for fname in _json_files():
            if "blk_trace" in fname:
                with open(os.path.join(MYTEST_DIR, fname)) as f:
                    data = json.load(f)
                assert "io_blk_range" in data

    def test_mytest_datalife_traces_valid_structure(self):
        for fname in _json_files():
            if "datalife.json" in fname:
                with open(os.path.join(MYTEST_DIR, fname)) as f:
                    data = json.load(f)
                process_name = list(data.keys())[0]
                assert "monitor" in data[process_name]


class TestParseTracesCLI:
    def test_parse_traces_on_workflow_traces(self):
        """Run parse_traces.py on real widget workflow traces (which have hostname in filenames)."""
        trace_dir = None
        if os.path.isdir(os.path.join(WORKFLOW_TRACES, "ddmd")):
            for d in os.listdir(os.path.join(WORKFLOW_TRACES, "ddmd")):
                full = os.path.join(WORKFLOW_TRACES, "ddmd", d)
                if os.path.isdir(full):
                    trace_dir = full
                    break
        if trace_dir is None:
            pytest.skip("No workflow traces directory available")

        result = subprocess.run(
            [sys.executable, os.path.join(AGENT_DIR, "parse_traces.py"), trace_dir],
            capture_output=True, text=True
        )
        assert result.returncode == 0, f"parse_traces.py failed:\n{result.stdout}\n{result.stderr}"
        assert "r_blk_trace" in result.stdout
