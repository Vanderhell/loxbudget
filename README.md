# loxbudget

Tiny no-heap C99 library for embedded firmware: pre-flight checks for embedded operations.

[![CI](https://github.com/Vanderhell/loxbuget/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/loxbuget/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/Vanderhell/loxbuget?display_name=tag)](https://github.com/Vanderhell/loxbuget/releases)
[![License](https://img.shields.io/github/license/Vanderhell/loxbuget)](LICENSE)
[![Language](https://img.shields.io/badge/language-C99-blue.svg)](include/loxbudget.h)
[![Platform](https://img.shields.io/badge/platform-embedded%20firmware-0a7ea4.svg)](docs/porting.md)
[![Single Header](https://img.shields.io/badge/distribution-single--header-success)](tools/amalgamate.py)
[![CMake](https://img.shields.io/badge/build-CMake-informational)](CMakeLists.txt)
[![No Heap](https://img.shields.io/badge/heap-none-critical)](SPEC.md)
[![No Floats](https://img.shields.io/badge/floats-none-critical)](SPEC.md)
[![API Status](https://img.shields.io/badge/api-v1.0.0%20rc1-orange)](releases/v1.0.0-rc1.md)
[![Security](https://img.shields.io/badge/security-policy-brightgreen)](SECURITY.md)
[![Coverage](https://img.shields.io/badge/coverage-CI%20tracked-blueviolet)](.github/workflows/ci.yml)

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

## Repository Layout

- `include/` public API
- `src/` library implementation
- `adapters/` optional integrations
- `tests/` host and integration coverage
- `examples/` minimal and scenario-oriented demos
- `tools/` utility scripts for amalgamation, calibration and checks

## Contributing

See `CONTRIBUTING.md` and `CODE_OF_CONDUCT.md`.
