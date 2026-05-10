# Design Rationale (V1.0)

This is a short, user-facing summary pulled from `DESIGN.md`. If you're changing core logic or public API, read `DESIGN.md` in full.

## What loxbudget is

`loxbudget` answers one question:

> can this operation safely run right now?

It does that deterministically (bounded time, no hidden state) using:

- operation profiles (actions per pressure state)
- resource limits (reusable, consumable, state)
- optional audit/rate/calibration modules

## Core promises

- No heap allocation
- No floats (integer/fixed-point only)
- No global mutable state (library `.bss = 0`)
- Bounded execution time for every public function
- Feature gates compile out cleanly

## Why pressure is external

Pressure is policy, not mechanism. In V1.0 the application sets pressure (potentially driven by a health monitor). This keeps loxbudget small and avoids "magic heuristics" that hide tradeoffs.

## Why calibration exists

Calibration exists to remove guesswork in configuring limits. It records percentiles (P² estimator) and suggests conservative limits based on observed behavior.

## Why adapters are separate

Adapters integrate loxbudget with other libraries, but must remain optional and isolated:

- no adapter symbols in core build
- opt-in by linking the adapter TU

