# Causality (V1.1, optional)

Enable compilation:

- `-DLOXBUDGET_ENABLE_CAUSALITY=1`

## Purpose

Causality helps with the cascading-cost problem: an operation looks safe alone, but its downstream triggers (writes, logs, retries, queue flushes, ...) make the system overload.

In causality mode, operations declare which other operations they may trigger. During `loxbudget_check(parent)`, the library walks this graph up to a depth limit and adds the child needs scaled by a Q8 weight.

## API

- `loxbudget_op_may_trigger(budget, parent, child, kind)`
- `loxbudget_causality_edge_count(budget)`

## Trigger kinds (Q8 weights)

- `LOXBUDGET_TRIGGER_NEVER` (0)
- `LOXBUDGET_TRIGGER_RARE` (32)
- `LOXBUDGET_TRIGGER_MAYBE` (128)
- `LOXBUDGET_TRIGGER_ALWAYS` (255)

Scaled need uses round-to-nearest integer math:

`scaled = (need * weight_q8 + 128) >> 8`

## Notes

- `RARE` edges only contribute when pressure is `CRITICAL` or `SURVIVAL` (`SPEC.md` §17).
- Cycles are prevented by a visited bitmap; nodes already visited during the cascade walk are skipped.

## Scenario replay (host)

Build the host runner:

- `cmake -S . -B build_final`
- `cmake --build build_final`

Run a single scenario:

- `python3 tools/scenario_replay.py tests/scenarios/01_simple_cascade.txt`

Run the full suite:

- `python3 tools/scenario_replay.py tests/scenarios`
