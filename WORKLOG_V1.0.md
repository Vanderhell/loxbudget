# WORKLOG — V1.0 (Self-Calibration + Production-Ready)

> **Prerequisite**: V0.3 tagged and stable.
> **Goal**: self-calibration mode with P² estimator, suggested limits, all adapters, single-header amalgamation, stable API commitment.
> **Estimated effort**: ~14 days.

---

## Why This Phase

V0.1–V0.3 give users the tools. V1.0 makes those tools **easy to configure correctly**.

The hardest question for any loxbudget user is: *"what numbers should I put in `set_resource(LOX_RES_RAM, ?)`?"* Without help, they guess. Their guesses are wrong. They get false denials in testing or false grants in production.

Calibration fixes this: run firmware in calibration mode, get measured percentile data, copy suggested limits into your config. **This is the feature that makes loxbudget actually deployable, not just theoretically correct.**

V1.0 is also the **API stability commitment**. After this release, any breaking change is a major bump.

---

## Pre-Work

- [ ] V0.3 tagged.
- [ ] Read `SPEC.md §14` (Self-Calibration) carefully.
- [ ] Study P² algorithm: Jain & Chlamtac, "The P² Algorithm for Dynamic Calculation of Quantiles and Histograms Without Storing Observations" (1985). Implement from the paper, not from a blog post.
- [ ] Verify integer-only P² is feasible (it is; the paper uses floats but the algorithm is straightforward to convert).

---

## Scope

### In scope (V1.0)

- Calibration begin/sample/end API.
- P² percentile estimator (integer-only, no floats).
- Suggested profile output.
- Outlier detection.
- Host-side calibration report tool.
- All adapters: `microhealth`, `microconf`, `microbus`, `nvlog`, `loxguard`.
- Single-header amalgamation script.
- Stable API commitment (semver post-1.0).
- Full documentation: getting-started, porting, calibration guide.

### Out of scope

- Causality. (V1.1)
- Pressure forecasting. (V1.x)
- Borrow/lend. (V1.x)
- Energy budgets. (V1.x)

---

## Phase 1 — P² Estimator (Integer-Only)

**Goal**: implement Jain-Chlamtac P² in C with integer/Q16.16 arithmetic only.

### P1.1 — Algorithm understanding

- [ ] Read the original paper (1985).
- [ ] Understand the five marker invariants and the parabolic interpolation.
- [ ] Note that the paper uses floating-point heights and parabolic adjustments — these are the parts requiring conversion to fixed-point.

### P1.2 — Data structure

```c
typedef struct {
    /* Five markers with positions n_i and heights q_i */
    uint32_t marker_n[5];        /* current position of marker */
    int32_t  marker_n_prime[5];  /* desired position (Q0.0) */
    uint32_t marker_q_q16[5];    /* height of marker, Q16.16 */
    /* Desired marker positions advance by these increments per sample */
    uint32_t dn_q16[5];          /* increment, Q16.16 */
    uint32_t count;
    uint32_t initial_buffer[5];  /* first 5 samples for initialization */
    uint8_t  initialized;
} lb__p2_estimator_t;
```

Sized for tracking a single percentile. We need three estimators per calibrated op (p50, p95, p99).

### P1.3 — Initialization phase (first 5 samples)

- [ ] Buffer first 5 samples.
- [ ] Sort and use as initial marker heights.
- [ ] Initialize `marker_n` to 1..5 and `marker_n_prime` based on target percentile p:
  - n'_1 = 1
  - n'_2 = 1 + 2p
  - n'_3 = 1 + 4p
  - n'_4 = 3 + 2p
  - n'_5 = 5

For p=0.99: n'_2 = 1.02, n'_3 = 1.04, n'_4 = 3.02. Use Q16.16 to represent these.

### P1.4 — Sample processing

- [ ] For each new sample x:
  - Find cell k such that q_k ≤ x < q_{k+1} (linear scan, 5 elements).
  - Increment `marker_n[i]` for i > k.
  - Increment `marker_n_prime` by `dn_q16[i]` for all i.
  - For markers 2, 3, 4: if |n - n'| ≥ 1 in the right direction, attempt parabolic adjustment; if that goes outside [n_{i-1}, n_{i+1}], do linear adjustment.

### P1.5 — Parabolic formula (integer-converted)

The standard formula:
```
q'_i = q_i + d/(n_{i+1} - n_{i-1}) * 
       ((n_i - n_{i-1} + d)(q_{i+1} - q_i)/(n_{i+1} - n_i)
       + (n_{i+1} - n_i - d)(q_i - q_{i-1})/(n_i - n_{i-1}))
```

Where `d = ±1`. All `q` values are Q16.16; all `n` values are integers.

- [ ] Implement step by step using `int64_t` intermediate values to prevent overflow.
- [ ] Document the multiplication order to make overflow checking auditable.

### P1.6 — Linear formula (fallback)

```
q'_i = q_i + d * (q_{i+d} - q_i) / (n_{i+d} - n_i)
```

Simpler; integer arithmetic straightforward.

### P1.7 — Result extraction

- [ ] After processing, percentile estimate = `marker_q_q16[2] >> 16` (truncated to integer).
- [ ] Provide a Q16.16 accessor for finer reporting.

### P1.8 — Tests

- [ ] `test_p2_init`: initialization phase works for known sample sequences.
- [ ] `test_p2_uniform_distribution`: feed 1000 uniform samples [0, 1000]; estimated p99 within 5% of analytical (~990).
- [ ] `test_p2_normal_distribution`: feed 1000 normal samples; verify p50, p95, p99.
- [ ] `test_p2_exponential_distribution`: heavy-tailed; verify estimator handles it.
- [ ] `test_p2_no_overflow`: stress with values near `UINT16_MAX`; intermediate computations don't overflow.
- [ ] `test_p2_idempotent_when_not_called`: with no samples after init, percentile remains stable.

### Phase 1 exit criteria

- [ ] P² estimator works.
- [ ] Accuracy within 5-10% on standard distributions for n ≥ 1000.
- [ ] Integer-only; no float symbols in compiled output.
- [ ] CI green.

---

## Phase 2 — Calibration State and API

### P2.1 — Per-op calibration state

```c
typedef struct {
    lb__p2_estimator_t p50;
    lb__p2_estimator_t p95;
    lb__p2_estimator_t p99;
    uint16_t observed_max_ram;
    uint32_t observed_max_duration_us;
    uint16_t observed_max_at_500;  /* for outlier detection */
    uint32_t sample_count;
    uint32_t target_count;
    uint16_t outlier_count;
    uint8_t  active;
} lb__calibration_state_t;
```

### P2.2 — Storage layout extension

- [ ] Add calibration state slots to user buffer (one per registered op, or a smaller subset?).
- [ ] For V1.0: support calibrating up to MAX_OPS in parallel (one state per op).
- [ ] Update `LOXBUDGET_REQUIRED_SIZE`.
- [ ] Gated by `LOXBUDGET_ENABLE_CALIBRATION`.

### P2.3 — `loxbudget_calibrate_begin`

```c
loxbudget_status_t
loxbudget_calibrate_begin(loxbudget_t *budget,
                          loxbudget_op_id_t op,
                          uint32_t target_samples);
```

- [ ] Validate op is registered.
- [ ] Initialize calibration state for this op.
- [ ] Mark active.
- [ ] If already calibrating this op → ERR_BAD_STATE.

### P2.4 — `loxbudget_calibrate_sample`

```c
loxbudget_status_t
loxbudget_calibrate_sample(loxbudget_t *budget,
                           loxbudget_op_id_t op,
                           const loxbudget_sample_t *sample);
```

- [ ] Validate calibration is active for this op.
- [ ] Feed `ram_used` into p50/p95/p99 estimators.
- [ ] Update `observed_max_ram` and `observed_max_duration_us`.
- [ ] At sample 500, snapshot max into `observed_max_at_500`.
- [ ] Run outlier detection (per `SPEC.md §14`).
- [ ] Increment sample_count.

### P2.5 — `loxbudget_calibrate_end`

```c
loxbudget_status_t
loxbudget_calibrate_end(loxbudget_t *budget,
                        loxbudget_op_id_t op,
                        loxbudget_suggested_profile_t *out);
```

- [ ] Validate calibration was active.
- [ ] Extract percentiles from estimators.
- [ ] Compute suggested limits per spec formula:
  ```c
  suggested_ram_limit = MAX(p99 + RAM_ABS_MARGIN,
                            (observed_max * 105) / 100);
  ```
- [ ] Populate output struct.
- [ ] Mark calibration inactive.

### P2.6 — Outlier detection

- [ ] Per spec: `sample > p99 * 1.5` OR (after sample 500) `sample > observed_max_at_500 * 1.2`.
- [ ] Increment `outlier_count` but don't influence suggested limits.
- [ ] Outliers reported in calibration result for user inspection.

### P2.7 — Tests

- [ ] `test_calibration_basic_workflow`: begin → 100 samples → end → reasonable suggestions.
- [ ] `test_calibration_p99_accuracy`: synthetic sample distribution; verify suggested limit is reasonable.
- [ ] `test_calibration_outlier_count`: inject 5 outliers among 1000 samples; outlier_count = 5.
- [ ] `test_calibration_no_double_begin`: begin twice without end → ERR_BAD_STATE.
- [ ] `test_calibration_disabled_returns_error`: with feature off, returns FEATURE_DISABLED.

### Phase 2 exit criteria

- [ ] Calibration API works.
- [ ] Suggested limits are sensible for synthetic workloads.
- [ ] CI green.

---

## Phase 3 — Host Calibration Report Tool

### P3.1 — Binary export format

- [ ] Define a small binary format for exporting calibration state from device.
- [ ] Include version byte, op count, per-op snapshot.
- [ ] User exports via custom code (e.g., over UART) — library provides the snapshot, not the transport.

### P3.2 — Python report tool

- [ ] `tools/calibration_report.py`.
- [ ] Reads exported binary.
- [ ] Prints human-readable report (per spec §14 example).
- [ ] Optionally outputs JSON for further processing.

### P3.3 — Tests

- [ ] Round-trip test: export from a host-mode budget, parse with Python, verify values match.

---

## Phase 4 — Adapters

### P4.1 — `adapters/microhealth/`

- [ ] Drives pressure state from microhealth metrics.
- [ ] Configurable thresholds.
- [ ] Tests against a mocked microhealth.

### P4.2 — `adapters/microconf/`

- [ ] Loads resource limits and rate limits from microconf.
- [ ] Allows runtime reconfiguration without recompile.
- [ ] Tests with synthetic config.

### P4.3 — `adapters/microbus/`

- [ ] Publishes events: `PRESSURE_CHANGED`, `BUDGET_DENIED`, `LOCKDOWN_ENTERED`.
- [ ] Subscribes to nothing (one-way fan-out).

### P4.4 — `adapters/nvlog/`

- [ ] Persists critical audit records to nvlog.
- [ ] Filterable by severity threshold.

### P4.5 — `adapters/loxguard/`

- [ ] Pushes audit records into loxguard blackbox on critical events.
- [ ] On LOCKDOWN, flushes everything.

### Phase 4 exit criteria

- [ ] All five adapters compile and have basic tests.
- [ ] Each adapter is opt-in, separate translation unit.
- [ ] None of them appear in TINY profile binary.

---

## Phase 5 — Single-Header Amalgamation

### P5.1 — Amalgamation script

- [ ] `tools/amalgamate.py`.
- [ ] Reads `include/loxbudget.h` and recursively inlines includes.
- [ ] Reads all `src/loxbudget_*.c` files (excluding adapters).
- [ ] Wraps implementation in `#ifdef LOXBUDGET_IMPLEMENTATION`.
- [ ] Handles internal `static` symbol naming.
- [ ] Output: `single_header/loxbudget.h`.

### P5.2 — Validation

- [ ] Generated single-header compiles standalone with `-Wall -Wextra -Wpedantic`.
- [ ] All V0.1–V1.0 tests pass when built against single-header instead of multi-file.
- [ ] CI builds both: multi-file and single-header.

### P5.3 — Release artifact

- [ ] On tag, the single-header file is attached to the GitHub release.
- [ ] Users can download just one file.

---

## Phase 6 — Documentation Push

### P6.1 — Getting Started

- [ ] Walks user from `git clone` to working `01_minimal` example in 10 minutes.
- [ ] Covers single-header path AND CMake path.

### P6.2 — Porting Guide

- [ ] Section per platform: bare-metal, FreeRTOS, Zephyr, ESP-IDF, NuttX, host.
- [ ] Each: HAL setup, time source, critical section, example.

### P6.3 — Calibration Guide

- [ ] When to calibrate.
- [ ] How to instrument operations.
- [ ] Sample size recommendations.
- [ ] How to interpret outliers.
- [ ] How to remove calibration code from production.

### P6.4 — API Reference

- [ ] Doxygen on every public function.
- [ ] Auto-generate HTML.
- [ ] Hosted on GitHub Pages or similar.

### P6.5 — FAQ

- [ ] Per spec §29.

### P6.6 — Design Rationale

- [ ] Pull from `DESIGN.md` and expand with V1.0 retrospective notes.

---

## Phase 7 — API Freeze and Stability Commitment

### P7.1 — API audit

- [ ] List every public symbol.
- [ ] For each: confirm name, signature, and semantics are what we want for the long term.
- [ ] Anything questionable: fix now or document as "may change in v2".

### P7.2 — Public commitment

- [ ] In README: "Starting with v1.0.0, public API is stable per semver."
- [ ] In CHANGELOG: explicit commitment.

### P7.3 — Backward compat tests

- [ ] Take an example written against V0.3 API; verify it still compiles and runs against V1.0.
- [ ] Document any unavoidable breaks; provide migration notes.

---

## Phase 8 — Comprehensive Testing

### P8.1 — Coverage target

- [ ] ≥ 95% line coverage on core code.
- [ ] Use `gcov` and `lcov` for measurement.
- [ ] CI publishes coverage report.

### P8.2 — Determinism benchmark

- [ ] Per spec §22: cycles measurement for `check`, `enter`, `leave`.
- [ ] No outlier > 2× median over 1000 calls.

### P8.3 — Long-running fuzz

- [ ] 24-hour fuzz on each fuzz harness.
- [ ] Zero crashes, zero UB-sanitizer reports.

### P8.4 — Integration on real hardware (best effort)

- [ ] If possible, run integration tests on actual STM32, ESP32, RP2040.
- [ ] Document results in `benchmarks/v1.0_real_hardware.md`.

---

## Phase 9 — Release

### P9.1 — Release candidate

- [ ] Tag `v1.0.0-rc1`.
- [ ] Announce for community testing.
- [ ] Address feedback for 1-2 weeks.

### P9.2 — Final release

- [ ] All criteria met.
- [ ] Tag `v1.0.0`.
- [ ] Detailed release notes covering V0.1 → V1.0 journey.
- [ ] Single-header attached.
- [ ] Documentation site live.

---

## V1.0 Done Criteria

- [ ] Calibration works end-to-end with realistic accuracy.
- [ ] Host calibration report tool works.
- [ ] All five adapters work and are opt-in.
- [ ] Single-header amalgamation generates clean output.
- [ ] All V0.1–V0.3 tests still pass.
- [ ] Coverage ≥ 95% on core.
- [ ] Documentation complete: getting-started, porting, calibration, FAQ, API reference.
- [ ] Footprint targets met across all profiles.
- [ ] 24-hour fuzz clean.
- [ ] API committed as stable per semver.
- [ ] `v1.0.0` tagged.

---

## Common Mistakes to Avoid

1. **P² implementation copied from a Stack Overflow answer.** Implement from the paper. Verify against analytical distributions.
2. **Floats in P²** because "it's just for percentages." No. Q16.16 throughout.
3. **Calibration state in core unconditionally.** It's opt-in. TINY profile doesn't compile it.
4. **Adapters depending on each other.** Each adapter depends only on loxbudget core and the partner library.
5. **Single-header amalgamation that doesn't actually compile.** CI must verify the generated file builds standalone.
6. **API stability promised but breaking change snuck in.** Take API audit seriously. Anything wrong now will be locked in for v1.x lifetime.
7. **Documentation that lies about footprint.** Numbers in docs must match measured numbers in `benchmarks/`.

---

*End of V1.0 worklog.*
