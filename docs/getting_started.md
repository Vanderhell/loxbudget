# Getting Started

## Build (multi-file)

- CMake:
  - Configure: `cmake -S . -B build`
  - Build: `cmake --build build`
- Make:
  - Build library: `make all`
  - Run core tests: `make test`
  - Run adapter tests: `make test-adapters`

## Build (single-header)

Generate `single_header/loxbudget.h`:

- `python3 tools/amalgamate.py`

Then compile with `-I./single_header` and define `LOXBUDGET_IMPLEMENTATION` in **exactly one** translation unit.

Example:

```c
/* single_header_impl.c */
#define LOXBUDGET_IMPLEMENTATION
#include "loxbudget.h"
```

## Tests

With CMake, the repo builds these test executables:

- `test_v0_1`
- `test_microlog_adapter`
- `test_microhealth_adapter`
- `test_microconf_adapter`
- `test_microbus_adapter`
- `test_nvlog_adapter`
- `test_loxguard_adapter`

## Convenience init

For simple setups you can use:

- `loxbudget_config_simple(max_resources, max_ops)`
- `loxbudget_init_simple(&budget, storage, storage_size, max_resources, max_ops)`

## Convenience op profile

If you just want a “normal priority, always allow full” baseline profile:

- `loxbudget_op_profile_default(op_id)`
