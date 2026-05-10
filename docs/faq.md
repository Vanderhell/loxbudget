# FAQ

## Does this allocate memory?

No. All state lives in caller-provided storage passed to `loxbudget_init()`.

## Does this use floating point?

No. Core logic and calibration use integer / fixed-point only.

## Is the library thread-safe?

The library is designed for embedded firmware. `enter/leave` must be protected by the HAL critical section (callbacks) if you have preemption or interrupts that can call into loxbudget concurrently.

## How do I remove optional features in production?

Build with the feature macros set to `0` (for example `LOXBUDGET_ENABLE_CALIBRATION=0`). Optional modules compile out when disabled.

## How do I generate the single-header?

- `python3 tools/amalgamate.py`
- Compile with `-I./single_header` and define `LOXBUDGET_IMPLEMENTATION` in exactly one translation unit.

