# Public API

This document summarizes the public API declared in `include/loxbudget.h`.

## Contract (what you can rely on)

- Deterministic decisions: `check/enter/leave` do not consult hidden global state (all state lives in the `loxbudget_t` instance).
- No heap allocation: the library never calls `malloc/new`.
- No floating point: integer / fixed-point only.
- Bounded work per call: all public calls are intended to be bounded-time.

## Concurrency and thread-safety

`loxbudget` is designed for embedded firmware. It is safe to use from multiple contexts only if you provide a bounded critical section via the HAL (see `docs/porting.md`). In other words:

- If `enter/leave` can be called concurrently (preemption/interrupts), you must ensure the critical section callbacks are correct for your platform.
- `check()` is read-mostly, but still operates on shared instance state (do not assume it is lock-free).

## Feature gating (compile-time options)

Optional features compile out when disabled:

- `LOXBUDGET_ENABLE_AUDIT_TRAIL`
- `LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS`
- `LOXBUDGET_ENABLE_RATE_WINDOWS`
- `LOXBUDGET_ENABLE_CALIBRATION`
- `LOXBUDGET_ENABLE_CAUSALITY`

## Typical call flow

1. Initialize into caller-provided storage: `loxbudget_init()` (or `loxbudget_init_simple()`).
2. Declare resources: `loxbudget_set_resource()`.
3. Register operations and their profiles: `loxbudget_register_op()`.
4. Configure needs per operation: `loxbudget_op_set_need()`.
5. Before running an operation:
   - call `loxbudget_check()` to get a decision, then
   - call `loxbudget_enter()` / `loxbudget_leave()` to account a lease when you actually run it.

## Core types

- `loxbudget_status_t`
- `loxbudget_action_t`
- `loxbudget_pressure_t`
- `loxbudget_resource_kind_t`
- `loxbudget_priority_t`
- `loxbudget_reason_t`
- `loxbudget_decision_t`
- `loxbudget_op_profile_t`
- `loxbudget_lease_t`
- `loxbudget_decision_record_t`
- `loxbudget_sample_t` (calibration)
- `loxbudget_suggested_profile_t` (calibration)
- `loxbudget_snapshot_t`
- `loxbudget_burn_rate_t` (rate windows)
- `loxbudget_pressure_hint_t`
- `loxbudget_hal_callbacks_t`
- `loxbudget_config_t`

## Initialization

- `loxbudget_init()`
- `loxbudget_init_simple()`
- `loxbudget_deinit()`

## Configuration

- `loxbudget_config_simple()`
- `loxbudget_op_profile_default()`
- `loxbudget_set_resource()`
- `loxbudget_register_op()`
- `loxbudget_op_set_need()`

## Decisions and leases

- `loxbudget_check()`
- `loxbudget_enter()`
- `loxbudget_leave()`
- `loxbudget_yield_check()`
- `loxbudget_set_decision_hook()`

## Pressure

- `loxbudget_set_pressure()`
- `loxbudget_get_pressure()`

## Snapshot

- `loxbudget_snapshot()`

## Rate windows (optional)

Enabled with `LOXBUDGET_ENABLE_RATE_WINDOWS=1`:

- `loxbudget_set_rate_limit()`
- `loxbudget_set_lifetime_limit()`
- `loxbudget_get_burn_rate()`

## Calibration (optional)

Enabled with `LOXBUDGET_ENABLE_CALIBRATION=1`:

- `loxbudget_calibrate_begin()`
- `loxbudget_calibrate_sample()`
- `loxbudget_calibrate_end()`
- `loxbudget_calibration_export_size()`
- `loxbudget_calibration_export()`

## Causality (optional)

Enabled with `LOXBUDGET_ENABLE_CAUSALITY=1`:

- `loxbudget_op_may_trigger()`
- `loxbudget_causality_edge_count()`

## Audit trail (optional)

Enabled with `LOXBUDGET_ENABLE_AUDIT_TRAIL=1`:

- `loxbudget_audit_get_recent()`
- `loxbudget_audit_clear()`

## Diagnostic strings (optional)

Enabled with `LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS=1`:

- `loxbudget_action_name()`
- `loxbudget_pressure_name()`
- `loxbudget_reason_name()`
- `loxbudget_status_name()`

## HAL functions

Weak/overridable symbols:

- `loxbudget_hal_now_ms()`
- `loxbudget_hal_critical_enter()`
- `loxbudget_hal_critical_exit()`
- `loxbudget_hal_boot_proven()`
- `loxbudget_hal_voltage_ok()`
- `loxbudget_hal_network_up()`

Callback helper:

- `loxbudget_hal_default_permissive()`

Notes:

- If you do not override the weak symbols, the defaults are permissive (intended for host tests / bring-up, not a hardened production platform).
