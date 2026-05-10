#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path


def _die(msg: str) -> None:
    print(msg, file=sys.stderr)
    raise SystemExit(2)


def _default_runner() -> Path:
    # Prefer build_final if present (matches how we run other host tests on Windows).
    here = Path(__file__).resolve().parent.parent
    candidates = [
        here / "build_final" / "Debug" / "scenario_runner.exe",
        here / "build_final" / "scenario_runner",
        here / "build" / "Debug" / "scenario_runner.exe",
        here / "build" / "scenario_runner",
    ]
    for c in candidates:
        if c.exists():
            return c
    return candidates[0]


def main() -> int:
    ap = argparse.ArgumentParser(prog="scenario_replay.py")
    ap.add_argument("path", help="scenario file or directory")
    ap.add_argument(
        "--runner",
        default=str(_default_runner()),
        help="path to scenario_runner executable (default: auto-detect)",
    )
    ap.add_argument("--pattern", default="*.txt", help="when path is a directory, glob pattern")
    args = ap.parse_args()

    runner = Path(args.runner)
    path = Path(args.path)
    if not path.exists():
        _die(f"path not found: {path}")
    if not runner.exists():
        _die(
            f"runner not found: {runner}\n"
            "Build it first with: cmake -S . -B build_final; cmake --build build_final"
        )

    scenarios: list[Path]
    if path.is_dir():
        scenarios = sorted(path.glob(args.pattern))
        if not scenarios:
            _die(f"no scenarios matched {args.pattern} in {path}")
    else:
        scenarios = [path]

    failed = 0
    for scenario in scenarios:
        data = scenario.read_bytes()
        proc = subprocess.run(
            [str(runner)], input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        if proc.returncode != 0:
            sys.stderr.write(f"FAILED: {scenario}\n")
            sys.stderr.write(proc.stderr.decode(errors="replace"))
            failed += 1

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
