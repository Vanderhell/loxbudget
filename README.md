# loxbudget

Tiny no-heap C99 library for embedded firmware: pre-flight checks for embedded operations.

[![CI](https://github.com/Vanderhell/loxbudget/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/loxbudget/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/Vanderhell/loxbudget?display_name=tag)](https://github.com/Vanderhell/loxbudget/releases)
[![License](https://img.shields.io/github/license/Vanderhell/loxbudget)](LICENSE)
[![Language](https://img.shields.io/badge/language-C99-blue.svg)](include/loxbudget.h)
[![Platform](https://img.shields.io/badge/platform-embedded%20firmware-0a7ea4.svg)](docs/porting.md)
[![Single Header](https://img.shields.io/badge/distribution-single--header-success)](tools/amalgamate.py)
[![CMake](https://img.shields.io/badge/build-CMake-informational)](CMakeLists.txt)
[![No Heap](https://img.shields.io/badge/heap-none-critical)](SPEC.md)
[![No Floats](https://img.shields.io/badge/floats-none-critical)](SPEC.md)
[![API Status](https://img.shields.io/badge/api-v1.0.0%20rc1-orange)](releases/v1.0.0-rc1.md)
[![Security](https://img.shields.io/badge/security-policy-brightgreen)](SECURITY.md)
[![Coverage](https://codecov.io/gh/Vanderhell/loxbudget/graph/badge.svg)](https://codecov.io/gh/Vanderhell/loxbudget)

## Why loxbudget exists

Embedded firmware often fails because operations are allowed to run even when the system is already under resource pressure.

`loxbudget` provides a deterministic pre-flight gate before risky work: MQTT publish, OTA update, flash write, log burst, debug dump, parser run, queue allocation, or any operation that must be degraded, delayed, rejected, or allowed under pressure.

## Features

- Deterministic `check/enter/leave` decisions for operation profiles under pressure.
- No heap, no floats, no global mutable state (all state is user-owned storage).
- Optional audit ring buffer (`LOXBUDGET_ENABLE_AUDIT_TRAIL`) to retrieve recent decisions.
- Optional diagnostic strings (`LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS`).
- Optional rate windows + lifetime limits (`LOXBUDGET_ENABLE_RATE_WINDOWS`).
- Optional calibration (`LOXBUDGET_ENABLE_CALIBRATION`).
- API stability: the public API declared in `include/loxbudget.h` is intended to be stable starting with `v1.0.0` (semver).

## Typical use cases

- Prevent MQTT storms from exhausting queue slots.
- Block OTA when voltage or flash budget is unsafe.
- Degrade logging under memory pressure.
- Reject non-critical work during survival mode.
- Enforce flash-write lifetime budgets.

## What this is not

`loxbudget` is not a scheduler, allocator, watchdog, logger, profiler, or RTOS replacement. It is a deterministic admission-control layer for deciding whether an operation may run now.

## Alternatives (and why not)

Evaluators almost always ask "why not X?". This table is the short, practical answer.

| Alternative | Good for | Where it breaks down | Why `loxbudget` exists anyway |
|---|---|---|---|
| Token bucket / leaky bucket | Smooth-rate limiting a *single* flow (API calls, publishes) | Hard to model **multiple** resources (RAM/queue slots/flash budget), **pressure modes**, and **enter/leave** lifetimes; typically doesn't explain *why* a decision happened | `loxbudget` gates operations against multiple resource types + pressure state, with deterministic decisions and (optional) audit trail |
| Ad-hoc `if (...) return;` | Tiny one-off guardrails | Drifts into inconsistent rules, hidden coupling, and untestable edge cases; hard to enforce "no heap / no globals / bounded work" across a codebase | Centralizes policy in one library + test suite + CI gates (banned symbols, footprint budgets, sanitizers) |
| FreeRTOS resource handling (queues/semaphores/mutexes) | Concurrency control and backpressure within a subsystem | It answers “can I take this resource now?”, not “should I start this expensive operation under global pressure?”; no cross-cutting policy across subsystems | `loxbudget` is an **admission controller** you call *before* starting work, independent of the RTOS primitive you use underneath |

## Release status

Current release: `v1.0.0-rc1`

The public API is intended to be stable for `v1.0.0`. The RC period is for integration feedback, documentation cleanup, and platform validation. No feature expansion is planned before `v1.0.0`.

## Quick start

```c
#include "loxbudget.h"

int main(void) {
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(2, 2, 0) + 3u) / 4u];
  loxbudget_t b;

  loxbudget_op_profile_t p = loxbudget_op_profile_default(0);
  loxbudget_decision_t d;

  if (loxbudget_init_simple(&b, storage, sizeof(storage), 2, 2) != LOXBUDGET_OK) return 2;
  (void)loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE);
  (void)loxbudget_register_op(&b, &p);
  (void)loxbudget_op_set_need(&b, 0, 0, 5);
  (void)loxbudget_check(&b, 0, &d);
  return (d.action == LOXBUDGET_ALLOW_FULL) ? 0 : 1;
}
```

This initializes a budget instance into caller-provided storage, declares one reusable resource, registers an operation profile, and asks the library for a deterministic decision before running the operation.

## Docs

- Main index: `docs/index.md`
- Getting started: `docs/getting_started.md`
- MISRA notes: `docs/misra.md`
- Handoff/release notes: `docs/handoff.md`
- Release notes: `releases/v1.0.0-rc1.md`, `releases/v1.0.0.md`
- Roadmap: `ROADMAP.md`

## Build And Verify

```sh
make test
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Install And Consume With CMake

Install locally:

```sh
cmake -S . -B build
cmake --build build
cmake --install build --prefix ./dist
```

Consume from another CMake project:

```cmake
find_package(loxbudget CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE loxbudget::loxbudget)
```

## Verification status

| Area | Status |
|---|---|
| GCC host build | verified in CI |
| Clang host build | verified in CI |
| ARM Cortex-M0 cross-build | verified in CI |
| clang-format | enforced |
| clang-tidy | enforced |
| ASan/UBSan | verified |
| fuzz smoke | verified |
| single-header build | verified |
| CMake install/export | verified |
| footprint budget | checked |

## Benchmarks (measured numbers)

Embedded claims like "deterministic" need numbers. The library keeps `.bss = 0` (no global mutable state) by design; user storage lives in your `storage[]` buffer.

### At-a-glance table (MCU)

| Platform | Toolchain | `.text` | `.bss` | `loxbudget_check()` WCET | Stack peak (`check()`) |
|---|---|---:|---:|---:|---:|
| Cortex-M0 | `arm-none-eabi-gcc -Os` | 4192 B (standard, causality off) | 0 B | TBD (needs on-target timer; M0 has no `DWT->CYCCNT`) | TBD |
| Cortex-M4 | `arm-none-eabi-gcc -Os` | TBD | 0 B | TBD | TBD |

### Footprint (Cortex-M0, `-Os`, freestanding)

Measured with `arm-none-eabi-size` on a relocatable linked object (`arm-none-eabi-ld -r`). See `benchmarks/v1.1_footprint.md` for the exact methodology and toolchain versions.

| Build | `.text` | `.data` | `.bss` | Notes |
|---|---:|---:|---:|---|
| Standard profile (`LOXBUDGET_ENABLE_CAUSALITY=0`) | 4192 B | 0 B | 0 B | `build/loxbudget_arm_standard.o` |
| Standard profile (`LOXBUDGET_ENABLE_CAUSALITY=1`) | 5340 B | 0 B | 0 B | +1148 B `.text` vs baseline |

### CPU time (host microbenchmark)

Host microbenchmarks are **not** MCU cycle counts, but they are good for catching accidental complexity regressions. See `benchmarks/v1.1_cycles.md`.

| Scenario | Result |
|---|---:|
| `loxbudget_check()` baseline (no causality) | 260.7 ns/check |
| `loxbudget_check()` with causality (2 edges) | 338.8 ns/check |
| Overhead | ~1.30× |

### MCU cycle counts + stack usage

Target-cycle and stack-peak measurement:

- ESP32 (real hardware): `examples/esp32_arduino_loxbudget/` prints min/max/avg latency in a tight-loop stress pass.
- Cortex-M (real hardware): use a cycle counter (`DWT->CYCCNT` on M3/M4/M7) or a hardware timer on M0/M0+. See `benchmarks/cortexm_wcet_stack/`.

## Repository Layout

- `include/` public API
- `src/` library implementation
- `adapters/` optional integrations
- `tests/` host and integration coverage
- `examples/` minimal and scenario-oriented demos
- `tools/` utility scripts for amalgamation, calibration and checks

## Contributing

See `CONTRIBUTING.md` and `CODE_OF_CONDUCT.md`.
