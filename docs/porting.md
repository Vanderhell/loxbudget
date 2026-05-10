# Porting Guide

## Required HAL

Core requires:

- `now_ms`: monotonic milliseconds since boot (needed for audit/rate/calibration time bases).
- critical section enter/exit: required for atomic `enter/leave` on systems with preemption.

See:

- `include/loxbudget.h` (HAL callback struct)
- `src/loxbudget_hal.c` (default permissive callbacks)

## Typical setups

- Bare-metal:
  - `now_ms` from SysTick or hardware timer.
  - critical section from `__disable_irq()` / `__enable_irq()`.
- RTOS:
  - `now_ms` from RTOS tick count.
  - critical section from scheduler lock or mutex (must be bounded).

