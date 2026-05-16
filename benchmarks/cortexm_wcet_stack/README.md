# Cortex-M WCET + stack measurement harness (template)

This folder is a **portable template** for measuring:

- worst-case execution time (WCET) of `loxbudget_check()` in **cycles**
- per-function **stack peak** (static `-fstack-usage` report, plus optional runtime painting)

It is intentionally not tied to a specific board SDK so you can drop it into STM32CubeIDE, bare-metal Makefiles, etc.

## Cycle measurement

### Cortex-M3/M4/M7 (recommended): `DWT->CYCCNT`

Use `bench_cycles_dwt.c` and call `loxbench_cycles_init()` at boot.

### Cortex-M0/M0+

M0 typically **does not have** `DWT->CYCCNT`. Use a hardware timer (TIMx) or GPIO toggle + logic analyzer.

Provide a `loxbench_cycles_now()` implementation that returns a monotonically increasing tick count at CPU clock rate (or convert to cycles).

## Stack usage

### Static (toolchain): `-fstack-usage`

GCC can emit `.su` files with per-function stack usage.

- Build with `-fstack-usage`
- Run `python3 tools/stack_usage_report.py <path-to-.su-files>` to summarize

### Runtime (optional): stack painting

If you want measured stack peak under realistic call patterns, paint a stack region with a pattern and check the high-water mark after a run.

## Files

- `bench_wcet_check.c`: creates a "busy" `loxbudget_t` config and runs `check()` in a loop while recording max cycles
- `bench_cycles_dwt.c/.h`: drop-in DWT cycle counter helpers (M3/M4/M7)

