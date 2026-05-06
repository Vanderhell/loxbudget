# WORKLOG — V0.2 (Audit + Diagnostics)

> **Prerequisite**: V0.1 tagged and released. Do not start V0.2 work until `v0.1.0` is on `main` and CI is green.
> **Goal**: add audit trail, diagnostic strings (optional), and the first adapter (`microlog`).
> **Estimated effort**: ~7 days for a focused implementer.

---

## Why This Phase

V0.1 proved the core works. V0.2 adds the **first tool that makes V0.1's decisions actionable**: when a decision goes wrong in production, the developer needs to know *why*. The audit trail is that record.

Without audit, "the operation was rejected" is a black box. With audit, it's "OTA was rejected at t=184203 because BOOT_PROVEN was false." That's the difference between debuggable and not.

---

## Pre-Work

- [ ] V0.1 is tagged and stable.
- [ ] Read `SPEC.md §16` (Audit Trail) carefully.
- [ ] Re-read `DESIGN.md §14` (boundary with loxguard).
- [ ] Confirm the V0.1 decision engine has the data needed for audit records (op_id, action, pressure, denied_resource, requested, available, reason). If any is missing from `loxbudget_decision_t`, fix in a V0.1.x patch first.

---

## Scope

### In scope (V0.2)

- Audit trail ring buffer.
- `loxbudget_audit_get_recent` and `loxbudget_audit_clear`.
- `LOXBUDGET_OPF_PERSIST_AUDIT` semantics fully implemented.
- LOCKDOWN audit guarantee for `OPF_LOCKDOWN_PASS | OPF_PERSIST_AUDIT` ops.
- Optional diagnostic strings (`LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS`).
- Splitting `src/loxbudget.c` into `loxbudget_core.c` + `loxbudget_audit.c` (now permitted; was banned in V0.1).
- First adapter: `adapters/microlog/`.
- Two new examples: `02_freertos_mqtt_storm`, `03_esp_idf_flash_budget` (the latter without rate windows yet — that's V0.3 — but with audit).
- Footprint check for `STANDARD` profile.

### Out of scope

- Rate windows. (V0.3)
- Calibration. (V1.0)
- Causality. (V1.1)
- Audit filtering by op/action/time. (V0.4)
- Persistent audit to flash via `nvlog`. (V0.4 with adapter)

---

## Phase 1 — Refactor for Optional Modules

**Goal**: split the V0.1 single-file core into core + optional modules without changing behavior.

### P1.1 — Move audit-relevant types into separate sub-header

- [ ] Keep `include/loxbudget.h` as the single user-facing entry point.
- [ ] Internally, create `include/loxbudget/internal/audit.h` (or keep types inline).

### P1.2 — Split `src/loxbudget.c` into modules

- [ ] `src/loxbudget_core.c` — everything from V0.1.
- [ ] `src/loxbudget_audit.c` — audit-only code, gated on `LOXBUDGET_ENABLE_AUDIT_TRAIL`.

### P1.3 — Update build system

- [ ] Makefile picks up new `.c` files.
- [ ] CMake `target_sources` updated.
- [ ] Single-header amalgamation script (defer to V1.0; for now, multi-file is fine).

### P1.4 — Verify no regression

- [ ] All V0.1 tests still pass.
- [ ] Footprint of TINY profile unchanged (since audit is opt-in and disabled).
- [ ] CI green.

### Phase 1 exit criteria

- [ ] Multi-file core builds cleanly.
- [ ] No V0.1 test regressions.
- [ ] Library `.bss` still 0.

---

## Phase 2 — Audit Ring Buffer

### P2.1 — Storage layout extension

- [ ] Update `LOXBUDGET_REQUIRED_SIZE` to account for audit slots.
- [ ] When `audit_size = 0`, audit section is zero bytes (TINY profile must work).
- [ ] When `audit_size > 0`, must be power of 2.

### P2.2 — Ring buffer state

- [ ] Internal head index, count.
- [ ] Place after lease table in storage layout.

### P2.3 — Recording API (internal)

- [ ] Implement `lb__audit_record(loxbudget_t *budget, const loxbudget_decision_t *d, op_id, timestamp)`.
- [ ] Called from `check` and `enter` after decision is finalized.
- [ ] Newest record overwrites oldest when full.
- [ ] All under critical section.

### P2.4 — Public retrieval API

- [ ] `loxbudget_audit_get_recent(budget, out, max, &count)`.
- [ ] Returns newest-first.
- [ ] Non-mutating; safe to call concurrently with decisions if HAL critical section is configured.
- [ ] If audit disabled at compile time → returns `ERR_FEATURE_DISABLED`.

### P2.5 — Public clear API

- [ ] `loxbudget_audit_clear(budget)`.
- [ ] Resets head, count.
- [ ] Useful after exporting records to nvlog or loxguard.

### P2.6 — `OPF_PERSIST_AUDIT` semantics

- [ ] Currently every decision is recorded. The flag is a hint for adapters: "this op's denials matter more, escalate them."
- [ ] Implementation: set a bit in the decision record (or use the existing op_id lookup) so adapters can filter.

### P2.7 — LOCKDOWN audit guarantee

- [ ] When pressure is LOCKDOWN, ops with `OPF_LOCKDOWN_PASS | OPF_PERSIST_AUDIT` flags must:
  - Be allowed to proceed (per V0.1 LOCKDOWN passthrough).
  - Have their audit records written even if the decision engine is otherwise restricted.
- [ ] Add test: LOCKDOWN + PANIC_DUMP → audit record exists for PANIC_DUMP attempt.

### P2.8 — Tests

- [ ] `test_audit_basic`: 5 decisions → 5 records in newest-first order.
- [ ] `test_audit_wrap`: 100 decisions on a 32-slot ring → newest 32 retained.
- [ ] `test_audit_clear`: after clear, get_recent returns 0 records.
- [ ] `test_audit_disabled_returns_error`: with `LOXBUDGET_ENABLE_AUDIT_TRAIL=0`, retrieval returns `ERR_FEATURE_DISABLED`.
- [ ] `test_audit_lockdown_passthrough`: PANIC_DUMP audit record persists during LOCKDOWN.
- [ ] `test_audit_size_zero`: budget with `audit_size=0` skips audit silently (no error, no record).
- [ ] `test_audit_record_size_assert`: `_Static_assert(sizeof(loxbudget_decision_record_t) == 16)`.

### Phase 2 exit criteria

- [ ] All Phase 2 tests pass.
- [ ] TINY profile (with `audit_size = 0`) footprint unchanged from V0.1.
- [ ] STANDARD profile footprint < 8 KiB on Cortex-M3.
- [ ] CI green.

---

## Phase 3 — Diagnostic Strings (Optional)

**Goal**: provide human-readable strings for actions, pressure states, reasons. Compile out completely when disabled.

### P3.1 — String table

- [ ] In `src/loxbudget_strings.c`, define `const char *` arrays for each enum.
- [ ] Wrap entire file content in `#if LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS`.

### P3.2 — Public accessors

- [ ] `const char *loxbudget_action_name(loxbudget_action_t)`
- [ ] `const char *loxbudget_pressure_name(loxbudget_pressure_t)`
- [ ] `const char *loxbudget_reason_name(loxbudget_reason_t)`
- [ ] `const char *loxbudget_status_name(loxbudget_status_t)`
- [ ] When disabled: each accessor returns NULL.

### P3.3 — Tests

- [ ] `test_strings_when_enabled`: each accessor returns non-NULL valid string.
- [ ] `test_strings_when_disabled`: build with feature off; accessors return NULL or are absent.
- [ ] CI builds both configurations.

### P3.4 — Footprint impact

- [ ] Document `.rodata` overhead when enabled (~200-400 bytes).
- [ ] Verify the strings are removed from binary when disabled (banned-symbols-style check on string content).

### Phase 3 exit criteria

- [ ] Strings work when enabled.
- [ ] Zero footprint impact when disabled.
- [ ] CI builds both configs.

---

## Phase 4 — microlog Adapter

**Goal**: first concrete adapter. Demonstrates the adapter pattern and produces a useful integration.

### P4.1 — Adapter directory

- [ ] Create `adapters/microlog/loxbudget_microlog_adapter.h`
- [ ] Create `adapters/microlog/loxbudget_microlog_adapter.c`
- [ ] Adapter is **not** built by default; user must add it explicitly.

### P4.2 — Public API

```c
void loxbudget_microlog_attach(loxbudget_t *budget,
                               microlog_t *log,
                               loxbudget_action_t min_action_to_log);
```

- [ ] Attaches a callback to budget that fires on each decision.
- [ ] Decisions with action ≥ `min_action_to_log` are forwarded to microlog.
- [ ] Uses diagnostic strings if available; else logs numeric codes.

### P4.3 — Callback hook in core

- [ ] Add internal hook `lb__on_decision(budget, decision, op_id)`.
- [ ] Called after each `check` or `enter` completes.
- [ ] Adapter registers via `loxbudget_set_decision_hook(budget, fn, user)`.
- [ ] Default hook is NULL (no overhead).

### P4.4 — Tests (adapter-specific)

- [ ] `test_microlog_adapter_basic`: attach adapter, trigger denial, verify log entry.
- [ ] `test_microlog_adapter_filter`: only above-threshold actions are logged.

### Phase 4 exit criteria

- [ ] Adapter builds when explicitly requested.
- [ ] Core builds without adapter unchanged.
- [ ] Adapter tests pass.
- [ ] Adapter does not appear in TINY profile binary.

---

## Phase 5 — Examples

### P5.1 — Example 02: FreeRTOS MQTT storm (audit-focused)

- [ ] Create `examples/02_freertos_mqtt_storm/`.
- [ ] Simulates network outage and queue saturation.
- [ ] Demonstrates pressure escalation, decision degradation, audit retrieval after the storm.
- [ ] Length: 100-200 lines OK (not subject to 50-line rule; that's only for `01_minimal`).
- [ ] Compile-only target; doesn't need to actually run on FreeRTOS in CI.

### P5.2 — Example 03: ESP-IDF flash budget (audit-focused)

- [ ] Create `examples/03_esp_idf_flash_budget/`.
- [ ] Demonstrates flash write tracking via `CONSUMABLE` resource (no rate windows yet — that's V0.3).
- [ ] Shows how audit records reveal which operations consumed flash.

### Phase 5 exit criteria

- [ ] Both examples compile.
- [ ] Documentation in each example's README explains what's demonstrated.

---

## Phase 6 — Documentation Update

### P6.1 — README

- [ ] Add audit trail to feature list.
- [ ] Update footprint table for STANDARD profile.
- [ ] Show audit retrieval in a code snippet.

### P6.2 — Getting Started

- [ ] Add section on enabling audit.
- [ ] Add section on retrieving and interpreting decision records.

### P6.3 — CHANGELOG

- [ ] Add `## v0.2.0 — YYYY-MM-DD` section.
- [ ] List: audit ring buffer, decision strings, microlog adapter, two new examples, multi-file split.

### Phase 6 exit criteria

- [ ] Documentation reflects V0.2.
- [ ] Changelog accurate.

---

## Phase 7 — Release

### P7.1 — Final test pass

- [ ] All V0.1 tests still pass (regression).
- [ ] All V0.2 tests pass.
- [ ] CI green for 5 consecutive commits.

### P7.2 — Footprint validation

- [ ] TINY profile: unchanged from V0.1.
- [ ] STANDARD profile: under budget.
- [ ] Document actual numbers in `benchmarks/v0.2_footprint.txt`.

### P7.3 — Tag

- [ ] `git tag v0.2.0`
- [ ] Push tag
- [ ] Release notes

---

## V0.2 Done Criteria

- [ ] Audit trail works end to end.
- [ ] LOCKDOWN audit passthrough works.
- [ ] Diagnostic strings opt-in / opt-out builds both pass.
- [ ] microlog adapter works and is opt-in.
- [ ] V0.1 tests still pass (no regression).
- [ ] STANDARD profile footprint within budget.
- [ ] TINY profile footprint unchanged.
- [ ] CI green.
- [ ] `v0.2.0` tagged.

---

## Common Mistakes to Avoid

1. **Audit becomes mandatory.** It must remain opt-in. TINY profile compiles without any audit code.
2. **Adapter pulled into core.** Adapters live in their own directory, never in `src/`.
3. **String table grows uncontrolled.** Keep strings short. They're for diagnostics, not user-facing UI.
4. **Hook system grows generic.** The decision hook is for adapters to observe decisions. Don't generalize it into a full event bus — `microbus` adapter is for that.
5. **Sneaking rate windows in.** Rate windows are V0.3. Don't.

---

*End of V0.2 worklog.*
