# Roadmap

## Current status

`v1.0.0` is released.

The next planned development line is `v1.1.0`. The intent is to keep the `v1.0.x` API stable while collecting stronger real-hardware evidence and deciding whether causality work is ready to graduate from planned work into shipped functionality.

## `v1.1.0` priorities

### 1. Hardware evidence

- Run ESP32 Arduino demo soak on real hardware and record stability/performance results.
- Fill `benchmarks/v1.0_real_hardware.md` with measured data instead of placeholders.
- Re-check calibration outlier behavior from `SPEC.md` section 14 against captured real workloads.

### 2. Validation depth

- Run a longer local fuzz campaign and record outcomes.
- Keep single-header, host, cross-build, and coverage paths green while tightening any flaky checks.
- Expand downstream-consumer validation around installed CMake package usage.

### 3. Feature decision for causality

- Decide whether `v1.1.0` should stay focused on polish/evidence or formally include causality work.
- If causality ships, document exact scope, constraints, and migration impact before tagging a release candidate.

## Release rules for the next cycle

- Keep release notes in `releases/` and the high-level history in `CHANGELOG.md`.
- Tag only from a clean, committed state with green CI.
- Avoid new feature expansion in patch releases unless it is clearly security or correctness related.
