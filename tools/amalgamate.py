#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]


def read_text(p: pathlib.Path) -> str:
    return p.read_text(encoding="utf-8", errors="replace")


INC_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"\s*$', re.M)


def inline_includes(text: str, base_dir: pathlib.Path, seen: set[pathlib.Path]) -> str:
    def repl(m: re.Match[str]) -> str:
        rel = m.group(1)
        inc = (base_dir / rel).resolve()
        if inc in seen:
            return f"/* skipped duplicate include: {rel} */"
        if not inc.exists():
            return m.group(0)
        seen.add(inc)
        body = read_text(inc)
        return f"/* begin include: {rel} */\n{inline_includes(body, inc.parent, seen)}\n/* end include: {rel} */"

    return INC_RE.sub(repl, text)


def main() -> int:
    out_dir = ROOT / "single_header"
    out_dir.mkdir(exist_ok=True)
    out_path = out_dir / "loxbudget.h"

    header = read_text(ROOT / "include" / "loxbudget.h")
    header = inline_includes(header, (ROOT / "include"), seen=set())

    impl_parts: list[str] = []
    for p in sorted((ROOT / "src").glob("loxbudget_*.c")):
        if p.name.endswith("_adapter.c"):
            continue
        impl_parts.append(read_text(p))

    impl = "\n\n".join(impl_parts)
    impl = impl.replace('#include "loxbudget.h"', "/* include removed by amalgamate */")

    combined = header + "\n\n#ifdef LOXBUDGET_IMPLEMENTATION\n\n" + impl + "\n\n#endif\n"
    out_path.write_text(combined, encoding="utf-8")
    print(str(out_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

