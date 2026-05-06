#!/usr/bin/env python3
import struct
import sys


def die(msg: str) -> None:
    print(msg, file=sys.stderr)
    raise SystemExit(2)


def main() -> int:
    if len(sys.argv) != 2:
        die("usage: calibration_report.py <calibration.bin>")

    data = open(sys.argv[1], "rb").read()
    if len(data) < 4:
        die("invalid file")

    # Format (v1):
    # u8 version, u8 op_count, u16 reserved
    # then op_count records:
    # u8 op_id, u8 reserved, u16 ram_p50, u16 ram_p95, u16 ram_p99, u16 ram_max,
    # u32 dur_p95, u32 dur_p99, u32 dur_max, u16 sugg_ram, u16 outliers, u32 samples
    ver, op_count, _rsv = struct.unpack_from("<BBH", data, 0)
    if ver != 1:
        die(f"unsupported version {ver}")

    off = 4
    print(f"loxbudget calibration report v{ver} (ops={op_count})")
    for _ in range(op_count):
        if off + 32 > len(data):
            die("truncated record")
        (op_id, _z, ram_p50, ram_p95, ram_p99, ram_max, dur_p95, dur_p99, dur_max, sugg_ram,
         outliers, samples) = struct.unpack_from("<BBHHHHIIIHHI", data, off)
        off += 32
        print(
            f"- op {op_id}: ram p50={ram_p50} p95={ram_p95} p99={ram_p99} max={ram_max} "
            f"suggested={sugg_ram}, dur p95={dur_p95}us p99={dur_p99}us max={dur_max}us "
            f"samples={samples} outliers={outliers}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

