# Progress log (agent handoff)

**Last updated:** 2026-05-06

## Where we stopped

**Last commit:** `89b01a1` — `[V1.0 P6-P7] Update README for V1.0 features`

Completed worklogs (no tags were created):
- `WORKLOG_V0.1.md` complete
- `WORKLOG_V0.2.md` complete
- `WORKLOG_V0.3.md` complete

V1.0 started (partial):
- `860ab58` — integer P² estimator + basic calibration API (gated `LOXBUDGET_ENABLE_CALIBRATION`)
- `84507f0` — per-op calibration storage + tools: `tools/calibration_report.py`, `tools/amalgamate.py`
- `a8fd6dc` — CI job `single-header` generates + compiles `single_header/loxbudget.h`
- `1fca391` — adapter stubs (opt-in): `adapters/microhealth`, `adapters/microconf`, `adapters/microbus`, `adapters/nvlog`, `adapters/loxguard`
- `89b01a1` — README feature bullets include rate windows + calibration

## Next steps (tomorrow)

1. Continue `WORKLOG_V1.0.md` Phase 3:
   - Define/export real calibration binary snapshot format from device/host
   - Align `tools/calibration_report.py` with that format (currently scaffold)
2. `WORKLOG_V1.0.md` Phase 4:
   - Implement real adapter behavior (currently mostly stubs) + add adapter tests
3. `WORKLOG_V1.0.md` Phase 5:
   - Extend CI to run tests against generated `single_header/loxbudget.h` (currently compile-only)
4. Docs/coverage/bench/fuzz phases are not done.

## Code map

- Core: `src/loxbudget_core.c`
- Audit: `src/loxbudget_audit.c` (`LOXBUDGET_ENABLE_AUDIT_TRAIL`)
- Rate windows + lifetime + burn + `yield_check`: `src/loxbudget_core.c` (`LOXBUDGET_ENABLE_RATE_WINDOWS`)
- Calibration: `src/loxbudget_calibration.c` (`LOXBUDGET_ENABLE_CALIBRATION`)
- Diagnostic strings: `src/loxbudget_strings.c` (`LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS`)
- Single-header generator: `tools/amalgamate.py`
- CI: `.github/workflows/ci.yml`

