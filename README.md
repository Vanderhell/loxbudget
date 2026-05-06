#loxbudget

Tiny no-heap C99 library for embedded firmware: pre-flight checks for embedded operations.

## Features

- Deterministic `check/enter/leave` decisions for operation profiles under pressure.
- No heap, no floats, no global mutable state (all state is user-owned storage).
- Optional audit ring buffer (`LOXBUDGET_ENABLE_AUDIT_TRAIL`) to retrieve recent decisions.
- Optional diagnostic strings (`LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS`).
- Optional rate windows + lifetime limits (`LOXBUDGET_ENABLE_RATE_WINDOWS`).
- Optional calibration (`LOXBUDGET_ENABLE_CALIBRATION`).

## Quick start

```c
#include "loxbudget.h"

int main(void) {
  static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(2, 2, 0)];
  loxbudget_t b;
  loxbudget_config_t cfg = {0};
  cfg.max_resources = 2;
  cfg.max_ops = 2;
  cfg.max_concurrent_leases = 2;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();

  loxbudget_op_profile_t p = {0,
                              LOXBUDGET_PRIO_NORMAL,
                              LOXBUDGET_ALLOW_FULL,
                              LOXBUDGET_ALLOW_FULL,
                              LOXBUDGET_ALLOW_FULL,
                              LOXBUDGET_ALLOW_FULL,
                              LOXBUDGET_ALLOW_FULL,
                              0};
  loxbudget_decision_t d;

  if (loxbudget_init(&b, storage, sizeof(storage), &cfg) != LOXBUDGET_OK) return 2;
  (void)loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE);
  (void)loxbudget_register_op(&b, &p);
  (void)loxbudget_op_set_need(&b, 0, 0, 5);
  (void)loxbudget_check(&b, 0, &d);
  return (d.action == LOXBUDGET_ALLOW_FULL) ? 0 : 1;
}
```

This initializes a budget instance into caller-provided storage, declares one reusable resource, registers an operation profile, and asks the library for a deterministic decision before running the operation.
