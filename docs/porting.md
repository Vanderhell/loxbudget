# Porting Guide

## Required HAL

Core requires:

- `now_ms`: monotonic milliseconds since boot (used for audit/rate/calibration time bases).
- Critical section enter/exit: required for correct `enter/leave` on systems with preemption or interrupts.

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

## Guidance

- Keep critical sections short and bounded. `enter/leave` should not be blocked behind unbounded locks.
- Ensure `now_ms` is monotonic (never goes backwards). If your platform can sleep, prefer an uptime counter that continues across sleep or document the behavior.
