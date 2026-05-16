#!/usr/bin/env python3
from __future__ import annotations

import argparse
import pathlib
import re
import sys


LINE_RE = re.compile(r"^(?P<file>.*?):(?P<line>\d+):(?P<func>[^:]+):(?P<bytes>\d+):(?P<kind>.*)$")


def iter_su_files(paths: list[pathlib.Path]) -> list[pathlib.Path]:
    out: list[pathlib.Path] = []
    for p in paths:
        if p.is_dir():
            out.extend(sorted(p.rglob("*.su")))
        elif p.is_file() and p.suffix == ".su":
            out.append(p)
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Summarize GCC -fstack-usage .su files")
    ap.add_argument("path", nargs="+", help="One or more .su files or directories to scan")
    args = ap.parse_args(argv)

    su_files = iter_su_files([pathlib.Path(p) for p in args.path])
    if not su_files:
        print("No .su files found", file=sys.stderr)
        return 2

    per_func: dict[str, int] = {}
    for su in su_files:
        for line in su.read_text(encoding="utf-8", errors="replace").splitlines():
            m = LINE_RE.match(line.strip())
            if not m:
                continue
            func = m.group("func")
            b = int(m.group("bytes"))
            prev = per_func.get(func)
            if prev is None or b > prev:
                per_func[func] = b

    top = sorted(per_func.items(), key=lambda kv: kv[1], reverse=True)[:50]
    print("Top stack users (bytes):")
    for func, b in top:
        print(f"{b:5d}  {func}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

