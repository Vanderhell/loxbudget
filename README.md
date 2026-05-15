# loxbudget

Tiny no-heap C99 library for embedded firmware: pre-flight checks for embedded operations.

![CI](https://github.com/Vanderhell/loxbuget/actions/workflows/ci.yml/badge.svg)

## Features

- Deterministic `check/enter/leave` decisions for operation profiles under pressure.
- No heap, no floats, no global mutable state (all state is user-owned storage).
- Optional audit ring buffer (`LOXBUDGET_ENABLE_AUDIT_TRAIL`) to retrieve recent decisions.
- Optional diagnostic strings (`LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS`).
- Optional rate windows + lifetime limits (`LOXBUDGET_ENABLE_RATE_WINDOWS`).
- Optional calibration (`LOXBUDGET_ENABLE_CALIBRATION`).
- API stability: the public API declared in `include/loxbudget.h` is intended to be stable starting with `v1.0.0` (semver).

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

## Repository Layout

- `include/` public API
- `src/` library implementation
- `adapters/` optional integrations
- `tests/` host and integration coverage
- `examples/` minimal and scenario-oriented demos
- `tools/` utility scripts for amalgamation, calibration and checks

## Contributing

See `CONTRIBUTING.md` and `CODE_OF_CONDUCT.md`.
