# WORKLOG — V0.3 (Time-Windowed Budgets)

> **Prerequisite**: V0.2 tagged and stable.
> **Goal**: time-windowed rate limits, lifetime limits, burn rate query. Plus the cooperative `yield_check` API.
> **Estimated effort**: ~10 days.

---

## Why This Phase

Static limits (V0.1) catch one-shot resource exhaustion. Audit (V0.2) explains denials after the fact. Rate windows fill the gap between: **prevent slow-burn resource exhaustion before it becomes a problem.**

The killer use cases are:
- Flash burnout from log storms.
- MQTT outbox saturation under network outages.
- Queue starvation during sustained load.

Without rate windows, these failure modes are invisible to V0.1's decision engine.

---

## Pre-Work

- [ ] V0.2 tagged.
- [ ] Read `SPEC.md §15` (Time-Windowed Budgets) twice.
- [ ] Decide implementation: fixed-window counter (default) or Q16.16 token bucket. V0.3 ships fixed-window; token bucket is V0.4 once fuzzed.
- [ ] Review HAL `now_ms()` precision on target platforms. Rate windows depend on it.

---

## Scope

### In scope (V0.3)

- Rate limits per resource: second/minute/hour/day windows.
- Lifetime limits per resource.
- `loxbudget_set_rate_limit`, `loxbudget_set_lifetime_limit`.
- `loxbudget_get_burn_rate`.
- `OPF_BYPASS_RATE_LIMIT` flag honored.
- Fixed-window counter implementation (default).
- Optional Q16.16 token bucket implementation (gated behind compile flag, off by default).
- `loxbudget_yield_check` API (cooperative checkpoint inside long ops).
- Three integration tests as flagship demos.
- Two new examples.

### Out of scope

- Calibration. (V1.0)
- Causality. (V1.1)
- Burn-rate-driven auto-pressure. (V1.x; pressure stays externally driven.)
- Adapter for `nvlog` persistence of rate state. (V1.0+)

---

## Phase 1 — Storage Extension

### P1.1 — Rate window storage

- [ ] Define `lb__rate_window_t`:
  ```c
  typedef struct {
      uint32_t window_start_ms;   /* fixed-window: window start */
      uint32_t window_duration_ms;
      uint32_t consumed;
      uint32_t limit;
  } lb__rate_window_t;
  ```
- [ ] Each resource may have up to 4 windows (sec, min, hour, day) plus 1 lifetime counter.
- [ ] Storage in user buffer; gated by `LOXBUDGET_ENABLE_RATE_WINDOWS`.

### P1.2 — Update `LOXBUDGET_REQUIRED_SIZE`

- [ ] Add accounting for rate window slots.
- [ ] When feature disabled, contribute zero bytes.

### P1.3 — Lifetime counter storage

- [ ] Per-resource: `uint32_t consumed_lifetime`, `uint32_t limit_lifetime`.
- [ ] Stored alongside window state.

### P1.4 — Tests

- [ ] `test_required_size_with_rate`: macro returns expected size when feature enabled.
- [ ] `test_required_size_without_rate`: same macro returns smaller size when disabled.

---

## Phase 2 — Configuration API

### P2.1 — `loxbudget_set_rate_limit`

```c
loxbudget_status_t
loxbudget_set_rate_limit(loxbudget_t *budget,
                         loxbudget_resource_id_t res,
                         loxbudget_window_t window,
                         uint32_t limit);
```

- [ ] Validate args.
- [ ] Resource must be `CONSUMABLE` (rate limits don't apply to REUSABLE; document why).
- [ ] Look up or allocate window slot for this resource × window combo.
- [ ] Initialize: `window_start_ms = now_ms()`, `consumed = 0`, `window_duration_ms = window_to_ms(window)`.

### P2.2 — `loxbudget_set_lifetime_limit`

```c
loxbudget_status_t
loxbudget_set_lifetime_limit(loxbudget_t *budget,
                             loxbudget_resource_id_t res,
                             uint32_t lifetime_max);
```

- [ ] Validate.
- [ ] Set per-resource lifetime limit.
- [ ] Lifetime counter starts at zero (or persists if `.noinit` storage and prior magic match).

### P2.3 — Window duration helper

```c
static uint32_t lb__window_duration_ms(loxbudget_window_t w);
```

| Window         | Duration ms  |
|----------------|--------------|
| SECOND         | 1 000        |
| MINUTE         | 60 000       |
| HOUR           | 3 600 000    |
| DAY            | 86 400 000   |
| LIFETIME       | (special)    |

### P2.4 — Tests

- [ ] `test_set_rate_limit_basic`: configure 60/min, verify it's recorded.
- [ ] `test_set_rate_limit_reusable_rejected`: setting rate on REUSABLE resource → ERR_INVALID_ARG.
- [ ] `test_set_lifetime_limit_basic`: configure lifetime cap, verify recorded.
- [ ] `test_multiple_windows_same_resource`: set both /min and /hour limits; both stored independently.

---

## Phase 3 — Rate Check Integration

### P3.1 — Update decision engine

- [ ] In `check` and `enter`, after the V0.1 resource availability check, add:
  - For each CONSUMABLE need, for each configured window: check fixed-window counter.
  - If any window denies → REJECT, reason `RATE_LIMIT`.
  - Check lifetime: if `consumed_lifetime + amount > limit_lifetime` → REJECT, reason `LIFETIME_EXHAUSTED`.

### P3.2 — Window rollover logic

- [ ] On each rate check:
  ```c
  if (now_ms - window_start_ms >= window_duration_ms) {
      window_start_ms = now_ms;
      consumed = 0;
  }
  ```
- [ ] Document: this is fixed-window, not sliding; allows up to 2× burst at boundary. Acceptable for flash-protection use cases. Token bucket option exists for stricter use cases.

### P3.3 — Consumption on enter

- [ ] When `enter` succeeds:
  - For each CONSUMABLE need, for each configured window: `consumed += amount`.
  - Lifetime: `consumed_lifetime += amount`.

### P3.4 — `OPF_BYPASS_RATE_LIMIT`

- [ ] If op profile has this flag, skip all rate checks for this op.
- [ ] Critical for `PANIC_DUMP` and similar evidence ops that must always succeed.
- [ ] Lifetime is **also** bypassed (panic evidence trumps lifetime caps).

### P3.5 — Tests (the meaty ones)

- [ ] `test_rate_limit_basic`: 60/min limit; 60 enter calls succeed, 61st fails with RATE_LIMIT.
- [ ] `test_rate_limit_window_rollover`: after window expires, counter resets and ops succeed again.
- [ ] `test_rate_limit_multi_window_and`: configure 60/min AND 1000/hour. After 60 in current minute, ops fail. After window resets, ops succeed (if hour budget allows).
- [ ] `test_lifetime_limit_basic`: lifetime=100; 100 ops succeed, 101st fails with LIFETIME_EXHAUSTED.
- [ ] `test_lifetime_persists_after_reset`: with `.noinit` storage, lifetime counter survives soft reset (test on host with simulated reset).
- [ ] `test_bypass_rate_limit`: panic op with `OPF_BYPASS_RATE_LIMIT` succeeds even when normal ops are rate-limited.
- [ ] `test_rate_limit_reusable_ignored`: REUSABLE resources don't trigger rate checks (since rate doesn't apply to them).

### Phase 3 exit criteria

- [ ] All Phase 3 tests pass.
- [ ] No regression in V0.1 / V0.2 tests.
- [ ] CI green.

---

## Phase 4 — Burn Rate Query

### P4.1 — Burn rate type

```c
typedef struct {
    uint32_t per_minute;            /* current window consumption */
    uint32_t per_hour;
    uint32_t estimated_exhaustion_ms;  /* 0 = N/A or never */
} loxbudget_burn_rate_t;
```

### P4.2 — `loxbudget_get_burn_rate`

```c
loxbudget_status_t
loxbudget_get_burn_rate(const loxbudget_t *budget,
                        loxbudget_resource_id_t res,
                        loxbudget_burn_rate_t *out);
```

- [ ] Read current consumed values from minute and hour windows.
- [ ] If lifetime configured, estimate exhaustion: `(limit - consumed_lifetime) / current_rate_per_ms`.
- [ ] Be careful with division and saturation to avoid overflow.

### P4.3 — Tests

- [ ] `test_burn_rate_basic`: after 30 ops in a minute, burn rate reads 30/min.
- [ ] `test_burn_rate_estimated_exhaustion`: lifetime=1000, current=500, rate=10/min → exhaustion in 50 minutes.
- [ ] `test_burn_rate_no_lifetime`: estimated_exhaustion_ms = 0 when no lifetime configured.

---

## Phase 5 — `yield_check` API

**Goal**: long-running operations can cooperatively check pressure mid-execution.

### P5.1 — Type

```c
typedef enum {
    LOXBUDGET_PRESSURE_HOLDING = 0,
    LOXBUDGET_PRESSURE_RISING  = 1,
    LOXBUDGET_PRESSURE_FALLING = 2,
    LOXBUDGET_SHOULD_ABORT     = 3
} loxbudget_pressure_hint_t;
```

### P5.2 — API

```c
loxbudget_status_t
loxbudget_yield_check(loxbudget_t *budget,
                      loxbudget_lease_t lease,
                      loxbudget_pressure_hint_t *out);
```

- [ ] Validates lease.
- [ ] Compares current pressure to pressure at lease acquisition (stored in lease metadata? or read fresh).
- [ ] Returns hint: `RISING` if pressure increased, `FALLING` if decreased, `HOLDING` if same, `SHOULD_ABORT` if pressure now exceeds the op's max acceptable level per profile.
- [ ] Does **not** revoke the lease. Caller decides whether to abort.

### P5.3 — Lease metadata extension

- [ ] Store pressure-at-acquisition in lease (we have a spare byte from V0.1 lease layout? if not, add it carefully and update `_Static_assert`).
- [ ] Or: store separately in lease slot table to avoid changing public lease size.

### P5.4 — Tests

- [ ] `test_yield_check_holding`: pressure unchanged → HOLDING.
- [ ] `test_yield_check_rising`: pressure escalated mid-op → RISING.
- [ ] `test_yield_check_should_abort`: pressure now denies the op → SHOULD_ABORT.
- [ ] `test_yield_check_invalid_lease`: stale lease → ERR_BAD_STATE.

### Phase 5 exit criteria

- [ ] yield_check works.
- [ ] Lease layout is stable (still 8 bytes, or documented size change).

---

## Phase 6 — Optional Token Bucket Implementation

**Goal**: provide an opt-in stricter rate limiter using Q16.16 token bucket.

### P6.1 — Compile-time selection

```c
#define LOXBUDGET_RATE_IMPL_FIXED_WINDOW 1
#define LOXBUDGET_RATE_IMPL_TOKEN_BUCKET 2

#ifndef LOXBUDGET_RATE_IMPL
  #define LOXBUDGET_RATE_IMPL LOXBUDGET_RATE_IMPL_FIXED_WINDOW
#endif
```

### P6.2 — Q16.16 implementation

- [ ] Per `SPEC.md §15` token bucket section.
- [ ] All math integer / Q16.16; no floats.
- [ ] Saturating arithmetic to prevent overflow on long elapsed times.

### P6.3 — Selection in code

- [ ] Both implementations live in `src/loxbudget_rate.c`.
- [ ] Switch via `#if LOXBUDGET_RATE_IMPL == ...`.
- [ ] CI builds both configurations.

### P6.4 — Tests

- [ ] `test_token_bucket_basic`: refill rate honored over elapsed time.
- [ ] `test_token_bucket_no_overflow`: stress with large elapsed times; tokens saturate at capacity.
- [ ] `test_token_bucket_burst`: full bucket allows burst consumption.

### Phase 6 exit criteria

- [ ] Both implementations work.
- [ ] CI builds both.
- [ ] Default remains fixed-window (simpler, easier to reason about).

---

## Phase 7 — Integration Tests (the demos)

These are the V0.3 marquee features. Make them shine.

### P7.1 — Demo: flash burnout / log storm

- [ ] `tests/integration/test_flash_burnout.c`.
- [ ] Setup: 100 logs/sec attempted; 60/min rate limit.
- [ ] Assert: 60 succeed, rest are denied with RATE_LIMIT.
- [ ] Assert: critical log (with `OPF_BYPASS_RATE_LIMIT`) always succeeds.
- [ ] Assert: audit trail records the denials.

### P7.2 — Demo: MQTT storm

- [ ] `tests/integration/test_mqtt_storm.c`.
- [ ] Setup: outbox saturated; pressure escalates.
- [ ] Assert: telemetry degrades through profile actions.
- [ ] Assert: fault events still go through.

### P7.3 — Demo: OTA blocked during lockdown

- [ ] `tests/integration/test_ota_blocked.c`.
- [ ] Setup: pressure forced to LOCKDOWN.
- [ ] Assert: OTA op rejected.
- [ ] Assert: PANIC_DUMP op succeeds and audit-records.

### Phase 7 exit criteria

- [ ] All three integration tests pass.
- [ ] These are the demos for the V0.3 release announcement.

---

## Phase 8 — Examples

### P8.1 — Update example 03 (ESP-IDF flash budget)

- [ ] V0.2 had it without rate windows. Add rate-windowed flash protection now.
- [ ] Show flash_writes/min and lifetime cap.

### P8.2 — New example 04: Zephyr basic

- [ ] `examples/04_zephyr_basic/`.
- [ ] Demonstrates Zephyr HAL integration (callback-based).
- [ ] Compile-only target in CI.

---

## Phase 9 — Fuzz Testing

### P9.1 — Set up libFuzzer harness

- [ ] `tests/fuzz/fuzz_decision_inputs.c`.
- [ ] Random op_id, random pressure transitions, random enter/leave sequences.

### P9.2 — Set up rate window fuzzer

- [ ] `tests/fuzz/fuzz_rate_windows.c`.
- [ ] Random rate config + random consumption over simulated time.
- [ ] Goal: no UB, no overflow, no negative counters.

### P9.3 — CI fuzz job (optional, runs nightly)

- [ ] 5-minute fuzz per harness in regular CI.
- [ ] 24-hour fuzz weekly on dedicated runner.

---

## Phase 10 — Documentation and Release

### P10.1 — Documentation

- [ ] README: add rate windows, burn rate, yield_check.
- [ ] Calibration guide: stub it for V1.0 ("coming next release") but mention measured numbers help here.
- [ ] CHANGELOG: full V0.3 entry.

### P10.2 — Footprint validation

- [ ] FULL profile target: < 12 KiB on Cortex-M4F.
- [ ] STANDARD profile: rate-windows-disabled build still under V0.2 budget.

### P10.3 — Release

- [ ] All tests pass.
- [ ] Tag `v0.3.0`.
- [ ] Release notes highlight the three demos.

---

## V0.3 Done Criteria

- [ ] Rate windows work for second/minute/hour/day.
- [ ] Lifetime limits work and persist across reset (`.noinit`).
- [ ] Burn rate query works.
- [ ] `yield_check` works.
- [ ] All three integration demos pass.
- [ ] Both fixed-window (default) and token-bucket implementations build and test.
- [ ] V0.1 and V0.2 tests still pass.
- [ ] FULL profile under footprint budget.
- [ ] Fuzz harnesses run clean.
- [ ] `v0.3.0` tagged.

---

## Common Mistakes to Avoid

1. **Floats sneaking into rate math.** Q16.16 is the answer. No exceptions.
2. **Pressure auto-detection from burn rate.** Tempting; out of scope. Keep pressure externally driven.
3. **Token bucket as default before it's fuzzed.** Default is fixed-window. Token bucket is opt-in until V0.4 confirms it's solid.
4. **Lifetime counter overflow.** Use `uint32_t` and saturating add; check before adding.
5. **`yield_check` mutating lease state.** It's a query, not an action. Caller decides.
6. **Skipping the integration tests.** They're not optional; they're the V0.3 product.

---

*End of V0.3 worklog.*
