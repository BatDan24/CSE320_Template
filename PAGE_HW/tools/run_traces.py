#!/usr/bin/env python3
"""Run every NAME.trace in the given directories through bin/sf_vm and compare
standard output with NAME.expected.

    python3 tools/run_traces.py DIR [DIR ...]
    python3 tools/run_traces.py --update DIR   # rewrite .expected from current output

A trace passes when its output matches exactly and the driver exits with 0
(or with 1 when the expected output contains a deliberate MISMATCH line).
"""

import difflib
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SF_VM = PROJECT_ROOT / "bin" / "sf_vm"


def run_trace(trace: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(SF_VM), str(trace)],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )


def check(trace: Path, update: bool) -> bool:
    expected_path = trace.with_suffix(".expected")
    try:
        completed = run_trace(trace)
    except subprocess.TimeoutExpired:
        print(f"FAIL {trace}: timed out")
        return False

    if completed.returncode == 2 or completed.stderr:
        print(f"FAIL {trace}: driver error (exit {completed.returncode})")
        print(completed.stderr, end="")
        return False

    if update:
        expected_path.write_text(completed.stdout)
        print(f"updated {expected_path}")
        return True

    if not expected_path.exists():
        print(f"FAIL {trace}: missing {expected_path.name}")
        return False

    expected = expected_path.read_text()
    expected_exit = 1 if "MISMATCH" in expected else 0
    if completed.stdout != expected or completed.returncode != expected_exit:
        print(f"FAIL {trace} (exit {completed.returncode}, expected {expected_exit})")
        sys.stdout.writelines(difflib.unified_diff(
            expected.splitlines(keepends=True),
            completed.stdout.splitlines(keepends=True),
            fromfile=str(expected_path.relative_to(PROJECT_ROOT)),
            tofile="actual",
        ))
        return False
    return True


def main(argv: list[str]) -> int:
    update = "--update" in argv
    directories = [Path(arg) for arg in argv if arg != "--update"]
    if not directories:
        print(__doc__)
        return 2
    if not SF_VM.exists():
        print(f"{SF_VM} not found; run make first")
        return 2

    traces = sorted(t for d in directories for t in (PROJECT_ROOT / d).glob("*.trace"))
    if not traces:
        print("no traces found")
        return 2
    failures = sum(not check(trace, update) for trace in traces)
    print(f"traces: {len(traces) - failures}/{len(traces)} passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
