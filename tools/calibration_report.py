#!/usr/bin/env python3
import argparse
import json
import struct
import sys
from dataclasses import asdict, dataclass
from typing import Any, List


def die(msg: str) -> None:
    print(msg, file=sys.stderr)
    raise SystemExit(2)


@dataclass
class RecordV1:
    op_id: int
    ram_p50: int
    ram_p95: int
    ram_p99: int
    ram_max: int
    dur_p95_us: int
    dur_p99_us: int
    dur_max_us: int
    suggested_ram_limit: int
    outlier_count: int
    sample_count: int


@dataclass
class RecordV2:
    op_id: int
    active: bool
    ram_p50: int
    ram_p95: int
    ram_p99: int
    ram_max: int
    dur_p95_us: int
    dur_p99_us: int
    dur_max_us: int
    suggested_ram_limit: int
    suggested_time_limit_us: int
    outlier_count: int
    sample_count: int
    target_samples: int


def _read_all(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def _parse(data: bytes) -> tuple[int, list[dict[str, Any]]]:
    if len(data) < 4:
        die("invalid file")

    ver, record_count, _rsv = struct.unpack_from("<BBH", data, 0)
    off = 4

    out: List[dict[str, Any]] = []

    if ver == 1:
        # header: u8 version, u8 record_count, u16 reserved
        # record (32 bytes):
        # u8 op_id, u8 reserved, u16 ram_p50, u16 ram_p95, u16 ram_p99, u16 ram_max,
        # u32 dur_p95_us, u32 dur_p99_us, u32 dur_max_us,
        # u16 suggested_ram_limit, u16 outlier_count, u32 sample_count
        for _ in range(record_count):
            if off + 32 > len(data):
                die("truncated record")
            (
                op_id,
                _z,
                ram_p50,
                ram_p95,
                ram_p99,
                ram_max,
                dur_p95,
                dur_p99,
                dur_max,
                sugg_ram,
                outliers,
                samples,
            ) = struct.unpack_from("<BBHHHHIIIHHI", data, off)
            off += 32
            out.append(
                asdict(
                    RecordV1(
                        op_id=op_id,
                        ram_p50=ram_p50,
                        ram_p95=ram_p95,
                        ram_p99=ram_p99,
                        ram_max=ram_max,
                        dur_p95_us=dur_p95,
                        dur_p99_us=dur_p99,
                        dur_max_us=dur_max,
                        suggested_ram_limit=sugg_ram,
                        outlier_count=outliers,
                        sample_count=samples,
                    )
                )
            )
        return ver, out

    if ver == 2:
        # header: u8 version, u8 record_count, u16 reserved
        # record (40 bytes):
        # u8 op_id, u8 flags(bit0=active), u16 reserved
        # u16 ram_p50, u16 ram_p95, u16 ram_p99, u16 ram_max
        # u32 dur_p95_us, u32 dur_p99_us, u32 dur_max_us
        # u16 suggested_ram_limit, u16 outlier_count, u32 sample_count
        # u32 suggested_time_limit_us, u32 target_samples
        for _ in range(record_count):
            if off + 40 > len(data):
                die("truncated record")
            (
                op_id,
                flags,
                _r0,
                ram_p50,
                ram_p95,
                ram_p99,
                ram_max,
                dur_p95,
                dur_p99,
                dur_max,
                sugg_ram,
                outliers,
                samples,
                sugg_time,
                target_samples,
            ) = struct.unpack_from("<BBHHHHHIIIHHIII", data, off)
            off += 40
            out.append(
                asdict(
                    RecordV2(
                        op_id=op_id,
                        active=bool(flags & 1),
                        ram_p50=ram_p50,
                        ram_p95=ram_p95,
                        ram_p99=ram_p99,
                        ram_max=ram_max,
                        dur_p95_us=dur_p95,
                        dur_p99_us=dur_p99,
                        dur_max_us=dur_max,
                        suggested_ram_limit=sugg_ram,
                        suggested_time_limit_us=sugg_time,
                        outlier_count=outliers,
                        sample_count=samples,
                        target_samples=target_samples,
                    )
                )
            )
        return ver, out

    die(f"unsupported version {ver}")
    raise AssertionError("unreachable")


def _pct(n: int, d: int) -> float:
    if d <= 0:
        return 0.0
    return 100.0 * (float(n) / float(d))


def _confidence(sample_count: int, target_samples: int) -> str:
    if target_samples <= 0:
        return "unknown"
    if sample_count >= target_samples:
        return "high"
    if sample_count >= (target_samples * 7) // 10:
        return "medium"
    if sample_count >= (target_samples * 3) // 10:
        return "low"
    return "very low"


def _which_ram_rule(ram_p99: int, ram_max: int, suggested: int) -> str:
    # Mirrors SPEC.md §14: max(p99+32B, max*1.05). We infer which term dominates.
    p99_term = ram_p99 + 32
    max_term = int((ram_max * 105 + 99) // 100)  # ceil(max*1.05)
    if suggested == p99_term and suggested == max_term:
        return "p99+32B == max*1.05"
    if suggested == p99_term:
        return "p99+32B"
    if suggested == max_term:
        return "max*1.05"
    # Fallback for future format versions / rounding differences.
    if abs(suggested - p99_term) <= 1 and abs(suggested - max_term) <= 1:
        return "p99+32B ~= max*1.05"
    if abs(suggested - p99_term) <= 1:
        return "p99+32B (approx)"
    if abs(suggested - max_term) <= 1:
        return "max*1.05 (approx)"
    return "unknown rule"


def _which_time_rule(dur_p99_us: int, dur_max_us: int, suggested_us: int) -> str:
    # Mirrors SPEC.md §14: max(p99+500us, max*1.10). We infer which term dominates.
    p99_term = dur_p99_us + 500
    max_term = int((dur_max_us * 110 + 99) // 100)  # ceil(max*1.10)
    if suggested_us == p99_term and suggested_us == max_term:
        return "p99+500us == max*1.10"
    if suggested_us == p99_term:
        return "p99+500us"
    if suggested_us == max_term:
        return "max*1.10"
    if abs(suggested_us - p99_term) <= 1 and abs(suggested_us - max_term) <= 1:
        return "p99+500us ~= max*1.10"
    if abs(suggested_us - p99_term) <= 1:
        return "p99+500us (approx)"
    if abs(suggested_us - max_term) <= 1:
        return "max*1.10 (approx)"
    return "unknown rule"


def _recommendation(sample_count: int, target_samples: int, outliers: int) -> str:
    if target_samples > 0 and sample_count < target_samples:
        return "collect more samples"
    if sample_count > 0 and (outliers / float(sample_count)) >= 0.02:
        return "inspect outliers (possible non-typical workload spikes)"
    return "looks OK"


def _print_text(ver: int, records: list[dict[str, Any]]) -> None:
    print(f"loxbudget calibration report v{ver} (records={len(records)})")
    for r in records:
        if ver == 1:
            print(f"Operation: op_{r['op_id']}")
            print(f"  Samples:        {r['sample_count']}")
            print("  RAM:")
            print(f"    p50:          {r['ram_p50']} B")
            print(f"    p95:          {r['ram_p95']} B")
            print(f"    p99:          {r['ram_p99']} B")
            print(f"    max:          {r['ram_max']} B")
            print(f"    suggested:    {r['suggested_ram_limit']} B")
            print("  Duration:")
            print(f"    p95:          {r['dur_p95_us']} us")
            print(f"    p99:          {r['dur_p99_us']} us")
            print(f"    max:          {r['dur_max_us']} us")
            print(f"  Outliers:       {r['outlier_count']}")
        else:
            active = " active" if r["active"] else ""
            print(f"Operation: op_{r['op_id']}{active}")
            print(f"  Samples:        {r['sample_count']} / {r['target_samples']}")
            conf = _confidence(int(r["sample_count"]), int(r["target_samples"]))
            outlier_rate = _pct(int(r["outlier_count"]), int(r["sample_count"]))
            print(f"  Confidence:     {conf}")
            print(f"  Outliers:       {r['outlier_count']} ({outlier_rate:.2f}%)")
            print("  RAM:")
            print(f"    p50:          {r['ram_p50']} B")
            print(f"    p95:          {r['ram_p95']} B")
            print(f"    p99:          {r['ram_p99']} B")
            print(f"    max:          {r['ram_max']} B")
            print(f"    suggested:    {r['suggested_ram_limit']} B")
            print(
                f"    rule:         {_which_ram_rule(int(r['ram_p99']), int(r['ram_max']), int(r['suggested_ram_limit']))}"
            )
            print("  Duration:")
            print(f"    p95:          {r['dur_p95_us']} us")
            print(f"    p99:          {r['dur_p99_us']} us")
            print(f"    max:          {r['dur_max_us']} us")
            print(f"    suggested:    {r['suggested_time_limit_us']} us")
            print(
                f"    rule:         {_which_time_rule(int(r['dur_p99_us']), int(r['dur_max_us']), int(r['suggested_time_limit_us']))}"
            )
            if not r["active"]:
                print("  Note:           inactive (operation did not run during calibration)")
            rec = _recommendation(int(r["sample_count"]), int(r["target_samples"]), int(r["outlier_count"]))
            print(f"  Recommendation: {rec}")


def main() -> int:
    ap = argparse.ArgumentParser(prog="calibration_report.py")
    ap.add_argument("path", help="calibration.bin")
    ap.add_argument("--json", action="store_true", help="emit JSON instead of text")
    args = ap.parse_args()

    data = _read_all(args.path)
    ver, records = _parse(data)

    if args.json:
        payload = {"version": ver, "records": records}
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0

    _print_text(ver, records)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
