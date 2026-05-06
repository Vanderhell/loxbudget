# WORKLOG — V0.1 (Tiny Core)

> **Purpose**: step-by-step implementation plan for V0.1. Designed to be followed by an agent (human or AI) without further direction beyond this document, DESIGN.md, V0.1_SCOPE.md, and SPEC.md.
> **Style**: each task is small, self-contained, and verifiable. Complete in order. Check the box. Commit. Move on.
> **Rule**: if a task seems to require something not in scope (V0.1_SCOPE.md), stop and re-read the scope. Do not improvise scope expansion.

---

## How to Use This Document

1. Read `DESIGN.md`, then `SPEC.md` (skim), then `V0.1_SCOPE.md` (carefully).
2. Work through Phase 1 → 2 → 3 → ... in order.
3. Each task is a single logical commit. Commit message format: `[V0.1 P{phase}.{task}] short description`.
4. At the end of each phase, verify the **phase exit criteria**. If they don't hold, do not proceed.
5. Don't start the next phase to "warm up" while debugging the current one. Finish, then move.

---

## Pre-Work — Read First

Before writing any code:

- [ ] Read `DESIGN.md` end to end. The "why" matters.
- [ ] Skim `SPEC.md` table of contents. Don't memorize; just know where things are.
- [ ] Read `V0.1_SCOPE.md` slowly. Note what's *out* of scope.
- [ ] Read this file (WORKLOG_V0.1.md) end to end. Don't start mid-document.
- [ ] Check that toolchain is available: `gcc --version`, `clang --version`, `arm-none-eabi-gcc --version`.

---

## Phase 1 — Repo Skeleton

**Goal**: a repository that builds nothing meaningful but passes CI, so every subsequent commit has a safety net.

### P1.1 — Repository initialization

- [ ] `git init`
- [ ] Create directory structure:
  ```
  loxbudget/
  ├── include/
  ├── src/
  ├── tests/
  ├── examples/01_bare_metal_minimal/
  ├── tools/
  ├── ci/
  └── docs/
  ```
- [ ] Add `.gitignore` for: `build/`, `*.o`, `*.a`, `*.exe`, IDE files.
- [ ] Add `LICENSE` (MIT recommended).
- [ ] Add stub `README.md` (will be filled in Phase 8).

### P1.2 — Place specification documents

- [ ] Copy `SPEC.md` to repo root.
- [ ] Copy `DESIGN.md` to repo root.
- [ ] Copy `V0.1_SCOPE.md` to repo root.
- [ ] Add `CHANGELOG.md` with `## v0.1.0 — unreleased` heading.

### P1.3 — Empty source files

- [ ] Create `include/loxbudget.h` containing only the include guard and a `#error "not implemented"` line below it. This guarantees nothing accidentally compiles before P3.
- [ ] Create `src/loxbudget.c` containing `#include "loxbudget.h"` only.
- [ ] Create `src/loxbudget_hal.c` containing `#include "loxbudget.h"` only.

### P1.4 — Build system stubs

- [ ] Create root `Makefile` with targets: `all`, `test`, `clean`, `tiny`, `format`, `lint`. They can be empty rules for now.
- [ ] Create root `CMakeLists.txt` declaring an empty static library `loxbudget`.
- [ ] Create `tests/Makefile` (or integrated rule) for host test build.

### P1.5 — CI pipeline

- [ ] Create `.github/workflows/ci.yml` (or equivalent for chosen CI).
- [ ] CI must run on push and pull request.
- [ ] CI jobs:
  - `gcc-host`: `make all && make test`.
  - `clang-host`: `make CC=clang all && make CC=clang test`.
  - `arm-none-eabi-cross`: compile-only build for Cortex-M0.
  - `format-check`: `clang-format --dry-run -Werror`.
  - `lint`: `clang-tidy` on all .c/.h files.

### P1.6 — Banned symbols script

- [ ] Create `tools/check_banned_symbols.sh`:
  ```bash
  #!/bin/bash
  # Fails if any banned symbol appears in the linked object/binary.
  set -e
  OBJ=${1:?usage: $0 <object-or-binary>}
  BANNED='malloc|free|calloc|realloc|printf|fprintf|sprintf|fopen|exit|abort'
  BANNED_FLOAT='__floatsi|__floatdi|__divdf|__muldf|__adddf|__subdf'
  if nm --undefined-only "$OBJ" 2>/dev/null \
       | grep -E "$BANNED|$BANNED_FLOAT"; then
      echo "FAIL: banned symbols referenced in $OBJ"
      exit 1
  fi
  echo "PASS: no banned symbols in $OBJ"
  ```
- [ ] Make it executable. Wire it into CI.

### P1.7 — Footprint check stub

- [ ] Create `ci/footprint_budget.yaml`:
  ```yaml
  tiny:
    text_max: 4096
    bss_max:  0
  ```
- [ ] Create `tools/footprint_check.sh` that reads the yaml and asserts `arm-none-eabi-size` output is within budget. Stub for now (always pass on empty library); make real in Phase 7.

### Phase 1 exit criteria

- [ ] Repo exists with all directories.
- [ ] CI runs and passes (because there is nothing to fail).
- [ ] All required documents (`SPEC.md`, `DESIGN.md`, `V0.1_SCOPE.md`, `CHANGELOG.md`, `README.md`, `LICENSE`) present.
- [ ] Banned-symbols script wired and executable.
- [ ] One green commit on `main`.

**If exit criteria don't hold, do not proceed to Phase 2.**

---

## Phase 2 — Public API Header

**Goal**: complete `include/loxbudget.h` with all types, enums, prototypes, and `_Static_assert`s. The header compiles standalone but no implementation exists yet.

### P2.1 — Remove the `#error` from header

- [ ] Replace the placeholder with a real header skeleton.

### P2.2 — Include guards and language level

- [ ] `#ifndef LOXBUDGET_H` / `#define LOXBUDGET_H` / `#endif`.
- [ ] `#ifdef __cplusplus / extern "C" { ... } / #endif` for C++ compatibility.
- [ ] Comment block at top: project name, version, license, one-paragraph description.

### P2.3 — Allowed includes only

- [ ] `#include <stdint.h>`
- [ ] `#include <stddef.h>`
- [ ] **Do not** `#include <stdbool.h>`, `<stdio.h>`, `<math.h>`, `<assert.h>`, anything else.

### P2.4 — Boolean type

- [ ] Define `typedef uint8_t loxbudget_bool_t;`
- [ ] `#define LOXBUDGET_TRUE  1u`
- [ ] `#define LOXBUDGET_FALSE 0u`

### P2.5 — Identifier typedefs

- [ ] `typedef uint8_t  loxbudget_resource_id_t;`
- [ ] `typedef uint8_t  loxbudget_op_id_t;`
- [ ] `typedef uint8_t  loxbudget_lease_id_t;`

### P2.6 — Status enum

- [ ] Per `SPEC.md §6`. All 12 values. Negative for errors.

### P2.7 — Action and pressure enums

- [ ] `loxbudget_action_t` with 5 values.
- [ ] `loxbudget_pressure_t` with 5 values.
- [ ] `loxbudget_resource_kind_t` with 3 values.
- [ ] `loxbudget_priority_t` with 4 values.
- [ ] `loxbudget_reason_t` with 10 values.

### P2.8 — Operation flags

- [ ] All 7 `LOXBUDGET_OPF_*` macros per spec.

### P2.9 — Public structs (with `_Static_assert`)

- [ ] `loxbudget_decision_t`
- [ ] `loxbudget_resource_view_t`
- [ ] `loxbudget_op_profile_t` — assert size 8.
- [ ] `loxbudget_lease_t` — assert size 8.
- [ ] `loxbudget_decision_record_t` — assert size 16 (even though audit is V0.2; the type can exist now).
- [ ] `loxbudget_snapshot_t`
- [ ] `loxbudget_config_t` (with `hal_strict`, `hal_callbacks` pointer).
- [ ] `loxbudget_hal_callbacks_t`

`_Static_assert` placement: directly after each struct definition.

### P2.10 — Internal types (forward declarations)

- [ ] `typedef struct loxbudget_t loxbudget_t;` — opaque to user.
  Definition lives in `loxbudget.c` or in a private internal header.

### P2.11 — `LOXBUDGET_REQUIRED_SIZE` macro

- [ ] Define as a compile-time macro (not function). See spec §13.
- [ ] Verify it produces a valid constant expression by:
  ```c
  static uint8_t test_buf[LOXBUDGET_REQUIRED_SIZE(8, 16, 0)];
  ```
  in a test file. If this fails to compile, fix the macro.

### P2.12 — Public function prototypes

- [ ] All 11 functions per `V0.1_SCOPE.md`. Include Doxygen-style comments for each.

### P2.13 — HAL prototypes

- [ ] `uint32_t loxbudget_hal_now_ms(void);`
- [ ] `void loxbudget_hal_critical_enter(void);`
- [ ] `void loxbudget_hal_critical_exit(void);`
- [ ] `loxbudget_bool_t loxbudget_hal_boot_proven(void);`
- [ ] `loxbudget_bool_t loxbudget_hal_voltage_ok(void);`
- [ ] `loxbudget_bool_t loxbudget_hal_network_up(void);`
- [ ] `const loxbudget_hal_callbacks_t *loxbudget_hal_default_permissive(void);`

### P2.14 — Header self-test

- [ ] Create `tests/test_header_compiles.c`:
  ```c
  #include "loxbudget.h"
  int main(void) { return 0; }
  ```
- [ ] Add to CI build. This catches header-only errors fast.

### Phase 2 exit criteria

- [ ] `loxbudget.h` compiles standalone with `-Wall -Wextra -Wpedantic -Werror`.
- [ ] All `_Static_assert`s pass.
- [ ] No banned `#include`s.
- [ ] CI green.

---

## Phase 3 — Init, Deinit, Snapshot

**Goal**: minimal lifecycle works. You can init a budget, snapshot it (empty state), and deinit it. No decisions yet.

### P3.1 — Internal struct definition

- [ ] In `src/loxbudget.c`, define the actual `struct loxbudget_t`.
- [ ] Include: header magic, pointer-into-storage offsets for sub-tables, pressure state, counters, `lease_magic_base`.
- [ ] Compute table offsets at init from `LOXBUDGET_REQUIRED_SIZE` parameters.

### P3.2 — Implement `loxbudget_init`

- [ ] Validate args: NULL checks, storage size, alignment (must be `uint32_t`-aligned).
- [ ] Validate config: `max_resources`, `max_ops`, `audit_size` (must be 0 or power of 2).
- [ ] Compute internal table offsets within the storage buffer.
- [ ] Write magic value to header.
- [ ] Initialize pressure to `NORMAL`.
- [ ] Initialize lease_magic_base from instance address XOR HAL time.
- [ ] Zero counters.
- [ ] Mark resource and op tables as empty.

### P3.3 — Implement `loxbudget_deinit`

- [ ] Validate the budget pointer + magic.
- [ ] Zero the magic to prevent reuse.
- [ ] Optionally `memset` the storage to a known pattern for debug builds.

### P3.4 — Implement `loxbudget_snapshot`

- [ ] Validate args.
- [ ] Copy public-visible counters and state into output struct.
- [ ] Read-only; no mutation.

### P3.5 — Implement `loxbudget_set_pressure` / `loxbudget_get_pressure`

- [ ] Validate args.
- [ ] `set_pressure`: assert valid enum value; write under critical section.
- [ ] `get_pressure`: simple read.

### P3.6 — Tests

- [ ] `test_init_invalid_args`: NULL budget, NULL storage, zero size, misaligned, NULL config, bad config values. Each must return correct error.
- [ ] `test_init_valid`: succeeds; snapshot reflects empty state with pressure NORMAL, zero counters.
- [ ] `test_deinit_after_init`: succeeds, second deinit on same instance fails cleanly.
- [ ] `test_pressure_set_get`: round-trip all 5 values.

### Phase 3 exit criteria

- [ ] Tests P3.6 pass on GCC, Clang, MSVC.
- [ ] No banned symbols.
- [ ] Library `.bss` still 0.
- [ ] CI green.

---

## Phase 4 — Resource and Operation Tables

**Goal**: configure resources and operations. No decision logic yet; just table management.

### P4.1 — Internal layout for resource table

- [ ] `loxbudget_resource_t` array within user storage at known offset.
- [ ] Mark each entry as configured/unconfigured via `flags` field.

### P4.2 — Implement `loxbudget_set_resource`

- [ ] Validate args: budget, id < max_resources, kind in enum range, limit ≤ UINT16_MAX.
- [ ] Write entry; mark configured.
- [ ] Reset `used`, `reserved`, `high_watermark` to 0.

### P4.3 — Implement `loxbudget_get_resource`

- [ ] Validate args.
- [ ] If id not configured → `LOXBUDGET_ERR_NOT_FOUND`.
- [ ] Compute `available` from limit/used/reserved per `SPEC.md §9`.

### P4.4 — Internal op profile table

- [ ] `loxbudget_op_profile_t` array within user storage.
- [ ] Separate needs table: `loxbudget_need_t[MAX_OPS][MAX_NEEDS_PER_OP]`.

### P4.5 — Implement `loxbudget_register_op`

- [ ] Validate args: budget, profile, op_id < max_ops, action values in enum range.
- [ ] Reject duplicate op_id with `ERR_DUPLICATE`.
- [ ] **Fail-closed HAL check**: if profile flags require boot/voltage/network and `cfg->hal_strict == 1` and HAL not provided → `ERR_HAL_NOT_CONFIGURED`. Detection mechanism: check if `cfg->hal_callbacks` provides the relevant callback. (Weak symbol detection is unreliable; explicit callback presence is the source of truth in strict mode.)
- [ ] Copy profile to internal table.
- [ ] Initialize empty needs list for this op.

### P4.6 — Implement `loxbudget_op_set_need`

- [ ] Validate args: op registered, resource configured, amount > 0.
- [ ] Find or insert need into op's needs list.
- [ ] If needs list full → `ERR_NO_SPACE`.

### P4.7 — Tests

- [ ] `test_set_resource_basic`: configure 3 resources, get them back.
- [ ] `test_register_op_basic`: register 2 ops, get profiles back via internal helper or via check (later).
- [ ] `test_register_op_duplicate`: returns ERR_DUPLICATE.
- [ ] `test_op_set_need_basic`: 3 needs added; 4th hits limit.
- [ ] `test_register_op_unconfigured_resource`: setting a need on a non-existent resource fails.
- [ ] `test_hal_strict_fail_closed`: register_op with `OPF_REQUIRES_BOOT_PROVEN` and no HAL callbacks → `ERR_HAL_NOT_CONFIGURED`.
- [ ] `test_hal_strict_disabled`: same op registers fine with `hal_strict = 0`.

### Phase 4 exit criteria

- [ ] All Phase 4 tests pass.
- [ ] Resource and op tables behave correctly.
- [ ] Fail-closed HAL works as designed.
- [ ] CI green.

---

## Phase 5 — Decision Engine and Atomic Reservation

**Goal**: the heart of the library. `check` and `enter`/`leave` work correctly under all valid combinations of profile × pressure × resource state.

### P5.1 — Implement `loxbudget_check`

- [ ] Validate args.
- [ ] Look up op profile by id; if not found → action REJECT, reason UNKNOWN_OP.
- [ ] Read current pressure.
- [ ] Map pressure to action via profile's `action_*` fields.
- [ ] If mapped action is WAIT/REJECT/LOCKDOWN, return immediately with reason `PRESSURE_BLOCK` or `LOCKDOWN_ACTIVE`.
- [ ] **LOCKDOWN special case**: if pressure is LOCKDOWN, only ops with `OPF_LOCKDOWN_PASS` flag may pass through.
- [ ] Check HAL preconditions (`OPF_REQUIRES_BOOT_PROVEN`, etc.) by calling appropriate callback or weak symbol. On failure → REJECT, reason PRECONDITION_FAIL.
- [ ] Iterate op's needs:
  - For each need, fetch resource.
  - If STATE kind and not satisfied → REJECT, PRECONDITION_FAIL.
  - If REUSABLE and `available < amount` → either WAIT or REJECT depending on profile.
  - If CONSUMABLE and `available < amount` → REJECT, INSUFFICIENT_RES.
- [ ] Return ALLOW_FULL or ALLOW_DEGRADED based on profile.
- [ ] **Update decision counters in snapshot**: `total_decisions++`, plus `total_grants` / `total_denials` / `total_degradations` as appropriate.

> Important: `check` must be **pure with respect to resource state** (no reserve, no consume). It may update statistics counters; this is acceptable because the decision (action+reason+denied_resource+requested+available) is identical for identical resource state.

### P5.2 — Determinism invariant

- [ ] Add comment in `check` documenting the determinism invariant: same resource state → same decision output (counters notwithstanding).
- [ ] Add test `test_decision_is_pure` that calls `check` twice and asserts the decision struct (excluding library counters) is identical.

### P5.3 — Implement `loxbudget_enter`

- [ ] Run the same validation as `check`.
- [ ] If decision is not ALLOW_FULL or ALLOW_DEGRADED, populate output decision and return appropriate status — but **do not** issue a lease.
- [ ] **Two-phase atomic reservation**:
  - **Phase A**: validate every need will succeed. No mutation.
  - **Phase B**: under critical section, apply all reservations:
    - REUSABLE: `reserved += amount`.
    - CONSUMABLE: `used += amount`.
    - Update `high_watermark`.
- [ ] Allocate lease slot (linear scan; `MAX_LEASES` is small).
- [ ] If no slot available → `ERR_NO_SPACE`.
- [ ] Set lease fields: `acquired_at_ms = now_ms()`, `magic = base ^ slot_index`, `id = slot_index`, `op = op_id`.
- [ ] Track lease in internal lease table.
- [ ] Increment `active_lease_count`.

### P5.4 — Implement `loxbudget_leave`

- [ ] Validate lease.id < MAX_LEASES.
- [ ] Validate lease.magic matches `base ^ id`.
- [ ] Validate lease slot is currently active.
- [ ] Under critical section:
  - For each REUSABLE need: `reserved -= amount` (saturating to 0 in case of any drift).
  - CONSUMABLE: no change (already permanently consumed).
  - STATE: no change.
- [ ] Mark lease slot free.
- [ ] Zero out lease magic in the slot to prevent stale-lease replay.
- [ ] Decrement `active_lease_count`.

### P5.5 — Critical section discipline

- [ ] All mutations in `enter` Phase B and in `leave` happen between `loxbudget_hal_critical_enter()` and `loxbudget_hal_critical_exit()`.
- [ ] Default weak implementations are no-ops; bare-metal single-threaded users pay nothing.

### P5.6 — Tests

- [ ] `test_check_allow_full`: simple success path.
- [ ] `test_check_pressure_degraded`: profile maps ELEVATED → DEGRADED; pressure set to ELEVATED; check returns DEGRADED.
- [ ] `test_check_pressure_reject`: profile maps SURVIVAL → REJECT; check returns REJECT, reason PRESSURE_BLOCK.
- [ ] `test_check_lockdown_passthrough`: op with `OPF_LOCKDOWN_PASS` succeeds in LOCKDOWN; op without flag is rejected.
- [ ] `test_check_unknown_op`: unknown op_id → REJECT, reason UNKNOWN_OP.
- [ ] `test_check_insufficient_resource`: limit=10, used=8, request 5 → WAIT or REJECT per profile, reason INSUFFICIENT_RES.
- [ ] `test_check_state_resource_fail`: STATE resource limit=0 → REJECT, PRECONDITION_FAIL.
- [ ] `test_decision_is_pure`: as in P5.2.
- [ ] `test_enter_leave_cycle`: enter → leave restores resource state.
- [ ] `test_enter_consumable_persists`: enter → leave keeps consumable counter.
- [ ] `test_no_partial_reservation`: 3 needs, 3rd fails. After failed enter, all resources unchanged.
- [ ] `test_double_leave_detected`: enter → leave → leave (same lease) returns BAD_STATE.
- [ ] `test_lease_magic_per_instance`: enter on instance A, attempt leave on instance B → BAD_STATE.
- [ ] `test_max_concurrent_leases`: enter MAX_LEASES times → next enter returns NO_SPACE.
- [ ] `test_watermark_tracking`: high_watermark updates correctly across enter/leave cycles.

### Phase 5 exit criteria

- [ ] All Phase 5 tests pass.
- [ ] No banned symbols.
- [ ] Library `.bss` = 0.
- [ ] **`test_no_partial_reservation` passes — this is the most important test in V0.1.**
- [ ] CI green.

---

## Phase 6 — HAL Implementation

**Goal**: weak default HAL works, callback override works, fail-closed mode works, permissive helper works.

### P6.1 — Weak default symbols in `src/loxbudget_hal.c`

- [ ] Macro `LOXBUDGET_WEAK` per spec §12 (GCC/Clang/IAR/Keil/MSVC).
- [ ] `loxbudget_hal_now_ms`: returns 0.
- [ ] `loxbudget_hal_critical_enter` / `loxbudget_hal_critical_exit`: empty.
- [ ] `loxbudget_hal_boot_proven`: returns LOXBUDGET_FALSE (fail-closed).
- [ ] `loxbudget_hal_voltage_ok`: returns LOXBUDGET_FALSE.
- [ ] `loxbudget_hal_network_up`: returns LOXBUDGET_FALSE.

### P6.2 — Permissive HAL accessor

- [ ] `loxbudget_hal_default_permissive()` returns pointer to a static `const loxbudget_hal_callbacks_t` whose all functions return `LOXBUDGET_TRUE`.
- [ ] Place in same TU as weak defaults.

### P6.3 — Callback HAL routing in core

- [ ] In every place core calls a HAL function, prefer `cfg->hal_callbacks->fn(cfg->hal_user)` if non-NULL; else call the weak symbol.
- [ ] Document this routing in code comments.

### P6.4 — MSVC path

- [ ] On MSVC, weak symbols don't behave the same. Document that MSVC users must provide `cfg->hal_callbacks`.
- [ ] Add CI job: `msvc-host` builds tests with callback HAL.

### P6.5 — Tests

- [ ] `test_hal_callback_overrides_weak`: provide callback that returns specific time; verify it's used.
- [ ] `test_hal_strict_with_callbacks`: register op with HAL precondition + callbacks → success.
- [ ] `test_hal_strict_without_callbacks`: same op, no callbacks → ERR_HAL_NOT_CONFIGURED.
- [ ] `test_hal_permissive_helper`: use `loxbudget_hal_default_permissive()` in a test, register an op with `OPF_REQUIRES_BOOT_PROVEN`, expect success.

### Phase 6 exit criteria

- [ ] All HAL tests pass on GCC/Clang/MSVC.
- [ ] Fail-closed default behavior verified.
- [ ] CI green.

---

## Phase 7 — Footprint and Cross-Compile

**Goal**: prove the library actually fits where we say it fits.

### P7.1 — Cortex-M0 cross-compile target

- [ ] Add Makefile target `tiny`:
  ```
  tiny:
      arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Os -flto \
          -ffunction-sections -fdata-sections \
          -Wl,--gc-sections -nostdlib \
          -c src/loxbudget.c src/loxbudget_hal.c \
          -o build/tiny/loxbudget.o
  ```

### P7.2 — Footprint measurement

- [ ] After cross-compile, run `arm-none-eabi-size --format=sysv build/tiny/loxbudget.o`.
- [ ] Parse `.text`, `.rodata`, `.bss` from output.
- [ ] **Assert library `.bss == 0`**. If not, fail loudly. Find the global. Remove it.
- [ ] Assert `.text < 4096`. If not, identify the heaviest function (`arm-none-eabi-nm --print-size --size-sort`) and decide: optimize or document.

### P7.3 — Banned symbols on cross-compiled binary

- [ ] Run `tools/check_banned_symbols.sh build/tiny/loxbudget.o`. Must pass.

### P7.4 — Wire into CI

- [ ] CI job `tiny-footprint` runs cross-compile + size check + banned-symbols.
- [ ] Job fails if budgets exceeded.

### P7.5 — Document actual numbers

- [ ] Update README.md with actual measured footprint (replace any placeholder targets).
- [ ] Add a `benchmarks/v0.1_footprint.txt` file with size output.

### Phase 7 exit criteria

- [ ] Cross-compile succeeds.
- [ ] `.text < 4 KiB`, `.bss = 0`.
- [ ] Real numbers documented.
- [ ] CI green.

---

## Phase 8 — Minimal Example

**Goal**: a 50-line example that an embedded engineer can read and understand in 60 seconds.

### P8.1 — Write the example

- [ ] Create `examples/01_bare_metal_minimal/main.c`.
- [ ] Use `loxbudget_hal_default_permissive()` (it's the minimal example, not production).
- [ ] Demonstrate:
  - storage declaration
  - init
  - 1-2 resources
  - 1 op with 1-2 needs
  - one `check` call
  - response based on action
- [ ] **Hard limit**: 50 lines including blank lines, comments, includes, and `main`.

### P8.2 — Verify line count in CI

- [ ] Add CI step: `wc -l < examples/01_bare_metal_minimal/main.c | awk '$1 > 50 { exit 1 }'`.
- [ ] If this fails, the example is too long. Simplify it.

### P8.3 — Build and run the example

- [ ] Add Makefile target: `make example` builds and runs it on host.
- [ ] Output should be deterministic (e.g., prints decision, exits 0).

### P8.4 — Document in README

- [ ] In README.md, paste the example verbatim under "Quick Start".
- [ ] Below it, explain in 3-4 sentences what just happened.

### Phase 8 exit criteria

- [ ] Example exists, builds, runs, ≤ 50 lines.
- [ ] README quick-start matches example.
- [ ] CI green.

---

## Phase 9 — Final Polish

**Goal**: tag v0.1.0.

### P9.1 — Review all 8 mandatory tests from V0.1_SCOPE.md

Cross-check that every test required by `V0.1_SCOPE.md` exists and passes:

- [ ] test_init_invalid_args
- [ ] test_init_valid
- [ ] test_decision_is_pure
- [ ] test_no_partial_reservation
- [ ] test_lease_lifecycle
- [ ] test_lease_magic_per_instance
- [ ] test_unknown_op_graceful
- [ ] test_hal_strict_fail_closed

If any are missing or weak, add them now.

### P9.2 — Static analysis pass

- [ ] Run `clang-tidy` with project config; fix or `// NOLINT(...)` justify all warnings.
- [ ] Run `cppcheck --enable=all`; address findings.
- [ ] Run sanitizers on host tests: `-fsanitize=address,undefined`. Must be clean.

### P9.3 — Documentation pass

- [ ] README.md: project description, quick-start, footprint table with real numbers, license.
- [ ] CHANGELOG.md: list V0.1 features and tag date.
- [ ] DESIGN.md: ensure references to "TBD" or "see V0.1" are resolved.
- [ ] SPEC.md: no edits — frozen.

### P9.4 — Determinism check

- [ ] Optional: write a separate harness that calls `check` 1000 times across random valid inputs and asserts no outlier latency exceeds 2× median. For V0.1 this can be on host, not on target — target measurement comes in V0.2+.

### P9.5 — Tag the release

- [ ] All CI green for at least 5 consecutive commits on `main`.
- [ ] All `V0.1_SCOPE.md` done criteria boxes ticked.
- [ ] `git tag v0.1.0`.
- [ ] Push tag.
- [ ] Write release notes summarizing what V0.1 delivers and what's *not* in it (link to V0.2 roadmap).

### Phase 9 exit criteria

- [ ] Tag `v0.1.0` exists on `main`.
- [ ] All done criteria from `V0.1_SCOPE.md` met.
- [ ] CI green.
- [ ] Release notes published.

---

## What Comes Next

After V0.1 ships:

1. Read `WORKLOG_V0.2.md`. It builds on V0.1.
2. Do not, under any circumstances, sneak V0.2 features into V0.1 patches. V0.1 patch releases (`v0.1.x`) are bug fixes only.
3. Collect feedback from V0.1 users (or your own dogfooding). V0.2 priorities should be informed by what hurt in V0.1.

---

## Common Mistakes to Avoid

Listed for the agent to recognize and avoid:

1. **"Just adding a small audit ring"** during V0.1 implementation. Audit is V0.2. Stop.
2. **Splitting `loxbudget.c` into multiple files** during V0.1. One file is the rule. Split in V0.2 if needed.
3. **Adding a static `loxbudget_t` instance** "for the example". The example uses user-owned storage. Period.
4. **Calling `printf` in tests** for debug output. Use `assert` and exit codes. Tests must run silently on success.
5. **Including `<stdbool.h>`** because `loxbudget_bool_t` feels weird. It feels weird because it's intentional. See DESIGN §6.
6. **Adding `float` to calibration code**. There is no calibration code in V0.1. (And when there is, no floats. See DESIGN §6.)
7. **Skipping the determinism test** because the implementation "obviously" is pure. The test exists because the implementation is *not* obviously pure to readers. Keep it.
8. **Lowering footprint targets** to "make CI green" instead of optimizing. If the library exceeds the budget, find out why and fix the cause.
9. **Treating MSVC as a second-class target.** It's CI-tested. It must work.
10. **Releasing without `_Static_assert`s on struct sizes.** The whole `LOXBUDGET_REQUIRED_SIZE` macro depends on these. Without them, future struct edits silently break user code.

---

## When You're Stuck

If a phase is taking 3× the estimate, pause and ask:

1. **Am I implementing something that's not in V0.1 scope?** Check `V0.1_SCOPE.md`.
2. **Am I trying to make a test pass that wasn't in the original list?** That's likely a Phase N+1 concern.
3. **Am I working around a core promise?** Re-read `DESIGN.md §2`.
4. **Is the SPEC ambiguous?** Open a discussion. Don't guess.

---

*End of V0.1 worklog.*
