# Public API

This document summarizes the public API declared in `include/loxbudget.h`.

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
