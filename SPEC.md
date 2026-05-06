# loxbudget — Complete Specification (v2)

**Pre-flight checks, learned budgets, and survival decisions for embedded C operations.**

> **Version**: 2.0 (post-review revision)
> **Status**: Specification frozen for V0.1 implementation.
> **Changes from v1**: see [Appendix C — Changelog](#appendix-c--changelog-from-v1).

---

## Table of Contents

1. [Project Definition](#1-project-definition)
2. [Goals and Non-Goals](#2-goals-and-non-goals)
3. [Core Concepts](#3-core-concepts)
4. [Architecture Overview](#4-architecture-overview)
5. [Public API Specification](#5-public-api-specification)
6. [Data Types and Structures](#6-data-types-and-structures)
7. [Decision Engine](#7-decision-engine)
8. [Pressure State Machine](#8-pressure-state-machine)
9. [Resource Model](#9-resource-model)
10. [Operation Profiles](#10-operation-profiles)
11. [Feature Gates and Profiles](#11-feature-gates-and-profiles)
12. [HAL Layer (Fail-Closed)](#12-hal-layer-fail-closed)
13. [Storage Model](#13-storage-model)
14. [Self-Calibration Mode](#14-self-calibration-mode)
15. [Time-Windowed Budgets](#15-time-windowed-budgets)
16. [Audit Trail](#16-audit-trail)
17. [Causality Tracking (V1.1)](#17-causality-tracking-v11)
18. [Adapters](#18-adapters)
19. [Repository Structure](#19-repository-structure)
20. [Build System and Deployment](#20-build-system-and-deployment)
21. [Testing Strategy](#21-testing-strategy)
22. [CI Requirements](#22-ci-requirements)
23. [Footprint Targets](#23-footprint-targets)
24. [Versioning and Roadmap](#24-versioning-and-roadmap)
25. [Use Cases and Demos](#25-use-cases-and-demos)
26. [Coding Conventions](#26-coding-conventions)
27. [Error Handling](#27-error-handling)
28. [Thread Safety](#28-thread-safety)
29. [Documentation Requirements](#29-documentation-requirements)
30. [Out of Scope](#30-out-of-scope)
31. [Hard Project Rules](#31-hard-project-rules)

---

## 1. Project Definition

### Short positioning

> **loxbudget — pre-flight checks for embedded C operations.**

### Long positioning (English)

> loxbudget is a tiny no-heap C99 library that learns, checks, and enforces practical resource budgets before embedded operations run. It returns a deterministic decision — run normally, run degraded, wait, reject, or enter survival mode — based on declared operation profiles and current system pressure.

### Long positioning (Slovak)

> loxbudget je malá C99 knižnica bez heapu, ktorá sa pred spustením operácie rozhodne, či má firmware dosť bezpečného rozpočtu na RAM, sloty, flash zápisy, čas alebo iné limitované zdroje. Vráti deterministické rozhodnutie: spusti normálne, spusti v zníženom režime, počkaj, odmietni, alebo prejdi do survival režimu.

### One-liner

> Drop one header into your project. No RTOS required. No heap. Pay only for what you use.

---

## 2. Goals and Non-Goals

### Primary goals

- **Universality** — runs on bare-metal, FreeRTOS, Zephyr, ESP-IDF, NuttX, and host (Linux/macOS/Windows) without modification.
- **Easy deployment** — single-header drop-in, no required build system, no required RTOS.
- **Linker-friendly** — pay only for the features you enable; nothing unused appears in the binary.
- **Determinism** — bounded execution time for every public function; no allocation; no recursion.
- **No dependencies** — only `<stdint.h>`, `<stddef.h>`, and `<string.h>` (for `memcpy`/`memset`).
- **No floats anywhere** — including all optional features. All math is integer or fixed-point.
- **Zero library `.bss`** — the library has no global mutable state. All storage is user-owned.
- **Fail-closed by default** — missing HAL configuration must cause an explicit error, never a silent permissive default.

### Secondary goals

- Small enough for Cortex-M0+ class devices (TINY profile).
- Useful enough for production IoT firmware on Cortex-M4/ESP32/RP2040 (FULL profile).
- Testable on host without cross-compiler.
- Composable with existing watchdog, logger, health, and event-bus libraries.

### Non-goals

- Not an RTOS or scheduler.
- Not a memory allocator.
- Not a logger or event bus.
- Not a watchdog.
- Not a safety-certified framework (DO-178C, IEC 61508, ISO 26262).
- Not a replacement for AUTOSAR ResourceManager.
- Not a formal verification tool.
- Not a profiler.

---

## 3. Core Concepts

### Operation
A named unit of work that consumes resources (e.g. `MQTT_PUBLISH`, `OTA_UPDATE`, `DEBUG_LOG`).

### Resource
A bounded quantity that operations consume (e.g. RAM arena bytes, MQTT outbox slots, flash write count, time budget).

### Operation profile
Compile-time declaration of an operation's needs and behavior under different pressure states.

### Decision
The runtime outcome of asking *"can this operation run now?"*. One of:
`ALLOW_FULL`, `ALLOW_DEGRADED`, `WAIT`, `REJECT`, `LOCKDOWN`.

### Pressure
A coarse-grained system-wide stress indicator: `NORMAL`, `ELEVATED`, `CRITICAL`, `SURVIVAL`, `LOCKDOWN`.

### Lease
An active reservation of resources, held while an operation runs. Returned on `enter`, released on `leave`.

### Budget instance
A self-contained loxbudget context, owning its resource table, operation profiles, audit trail, and pressure state. Multiple instances may coexist.

---

## 4. Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                      │
│              (firmware operations: MQTT, OTA, …)            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼ check / enter / leave
┌─────────────────────────────────────────────────────────────┐
│                      loxbudget Core                         │
│  ┌─────────────────┐  ┌──────────────────┐  ┌────────────┐  │
│  │ Decision Engine │  │ Resource Table   │  │  Pressure  │  │
│  └─────────────────┘  └──────────────────┘  └────────────┘  │
│  ┌─────────────────┐  ┌──────────────────┐  ┌────────────┐  │
│  │ Operation Table │  │  Lease Table     │  │   Snapshot │  │
│  └─────────────────┘  └──────────────────┘  └────────────┘  │
└─────────────────────────────────────────────────────────────┘
        │              │             │              │
        ▼              ▼             ▼              ▼
┌──────────┐  ┌──────────────┐  ┌─────────┐  ┌──────────────┐
│  Audit   │  │ Rate Windows │  │ Calibr. │  │  Causality   │
│ (V0.2,   │  │   (V0.3,     │  │ (V1.0,  │  │  (V1.1,      │
│  opt)    │  │    opt)      │  │   opt)  │  │    opt)      │
└──────────┘  └──────────────┘  └─────────┘  └──────────────┘
        │              │             │              │
        ▼              ▼             ▼              ▼
┌─────────────────────────────────────────────────────────────┐
│                   HAL Layer (fail-closed)                   │
│ now_ms · critical_enter/exit · boot_proven · voltage_ok ... │
└─────────────────────────────────────────────────────────────┘
        │              │             │              │
        ▼              ▼             ▼              ▼
┌─────────────────────────────────────────────────────────────┐
│                         Adapters                            │
│  microlog · microhealth · microconf · microbus · loxguard   │
└─────────────────────────────────────────────────────────────┘
```

### Layering rules

- **Core** is mandatory and self-contained. Has no knowledge of optional layers.
- **Optional features** (audit, rate, calibration, causality) compile out completely when disabled.
- **HAL** is fail-closed: missing required configuration is an error, not a silent default.
- **Adapters** live in separate translation units and are never linked unless explicitly used.

---

## 5. Public API Specification

### Initialization

```c
loxbudget_status_t
loxbudget_init(loxbudget_t *budget,
               void *storage, size_t storage_size,
               const loxbudget_config_t *cfg);

loxbudget_status_t
loxbudget_deinit(loxbudget_t *budget);
```

### Resource configuration

```c
loxbudget_status_t
loxbudget_set_resource(loxbudget_t *budget,
                       loxbudget_resource_id_t id,
                       uint16_t limit,
                       loxbudget_resource_kind_t kind);

loxbudget_status_t
loxbudget_get_resource(const loxbudget_t *budget,
                       loxbudget_resource_id_t id,
                       loxbudget_resource_view_t *out);
```

### Operation registration

```c
loxbudget_status_t
loxbudget_register_op(loxbudget_t *budget,
                      const loxbudget_op_profile_t *profile);

loxbudget_status_t
loxbudget_op_set_need(loxbudget_t *budget,
                      loxbudget_op_id_t op,
                      loxbudget_resource_id_t resource,
                      uint16_t amount);
```

### Decision API (lightweight check)

```c
loxbudget_status_t
loxbudget_check(loxbudget_t *budget,
                loxbudget_op_id_t op,
                loxbudget_decision_t *out);
```

`check` is a non-mutating query. It returns the decision but does not reserve any resources.

### Lease API (atomic reservation)

```c
loxbudget_status_t
loxbudget_enter(loxbudget_t *budget,
                loxbudget_op_id_t op,
                loxbudget_lease_t *out_lease);

loxbudget_status_t
loxbudget_leave(loxbudget_t *budget,
                loxbudget_lease_t lease);
```

`enter` performs an atomic multi-resource reservation. If any resource is insufficient, no reservation is made.

`leave` releases a lease and returns its reusable resources.

> **`loxbudget_yield_check` is NOT in V0.1.** Deferred to V0.3+ after rate windows are proven and yield semantics under pressure are well understood.

### Pressure API

```c
loxbudget_status_t
loxbudget_set_pressure(loxbudget_t *budget,
                       loxbudget_pressure_t pressure);

loxbudget_pressure_t
loxbudget_get_pressure(const loxbudget_t *budget);
```

### Snapshot API (read-only diagnostics)

```c
loxbudget_status_t
loxbudget_snapshot(const loxbudget_t *budget,
                   loxbudget_snapshot_t *out);
```

### Optional: Audit trail (V0.2+)

```c
#if LOXBUDGET_ENABLE_AUDIT_TRAIL
loxbudget_status_t
loxbudget_audit_get_recent(const loxbudget_t *budget,
                           loxbudget_decision_record_t *out,
                           size_t max_records,
                           size_t *out_count);

loxbudget_status_t
loxbudget_audit_clear(loxbudget_t *budget);
#endif
```

### Optional: Rate windows (V0.3+)

```c
#if LOXBUDGET_ENABLE_RATE_WINDOWS
loxbudget_status_t
loxbudget_set_rate_limit(loxbudget_t *budget,
                         loxbudget_resource_id_t res,
                         loxbudget_window_t window,
                         uint32_t limit);

loxbudget_status_t
loxbudget_set_lifetime_limit(loxbudget_t *budget,
                             loxbudget_resource_id_t res,
                             uint32_t lifetime_max);

loxbudget_status_t
loxbudget_get_burn_rate(const loxbudget_t *budget,
                        loxbudget_resource_id_t res,
                        loxbudget_burn_rate_t *out);
#endif
```

### Optional: Calibration (V1.0+)

```c
#if LOXBUDGET_ENABLE_CALIBRATION
loxbudget_status_t
loxbudget_calibrate_begin(loxbudget_t *budget,
                          loxbudget_op_id_t op,
                          uint32_t target_samples);

loxbudget_status_t
loxbudget_calibrate_sample(loxbudget_t *budget,
                           loxbudget_op_id_t op,
                           const loxbudget_sample_t *sample);

loxbudget_status_t
loxbudget_calibrate_end(loxbudget_t *budget,
                        loxbudget_op_id_t op,
                        loxbudget_suggested_profile_t *out);
#endif
```

### Optional: Causality (V1.1+)

```c
#if LOXBUDGET_ENABLE_CAUSALITY
loxbudget_status_t
loxbudget_op_may_trigger(loxbudget_t *budget,
                         loxbudget_op_id_t parent,
                         loxbudget_op_id_t child,
                         loxbudget_trigger_kind_t kind);
#endif
```

---

## 6. Data Types and Structures

### Boolean type (no `<stdbool.h>` dependency)

```c
typedef uint8_t loxbudget_bool_t;

#define LOXBUDGET_TRUE   1u
#define LOXBUDGET_FALSE  0u
```

> **Rationale**: the library claims dependency only on `<stdint.h>`, `<stddef.h>`, `<string.h>`. To uphold that claim, no `<stdbool.h>` is included. All public APIs use `loxbudget_bool_t`.

### Identifiers

```c
typedef uint8_t  loxbudget_resource_id_t;
typedef uint8_t  loxbudget_op_id_t;
typedef uint8_t  loxbudget_lease_id_t;
```

### Status codes

```c
typedef enum {
    LOXBUDGET_OK                     =  0,
    LOXBUDGET_ERR_INVALID_ARG        = -1,
    LOXBUDGET_ERR_NOT_INITIALIZED    = -2,
    LOXBUDGET_ERR_NO_SPACE           = -3,
    LOXBUDGET_ERR_NOT_FOUND          = -4,
    LOXBUDGET_ERR_DUPLICATE          = -5,
    LOXBUDGET_ERR_OVERFLOW           = -6,
    LOXBUDGET_ERR_BAD_STATE          = -7,
    LOXBUDGET_ERR_FEATURE_DISABLED   = -8,
    LOXBUDGET_ERR_HAL_NOT_CONFIGURED = -9,   /* fail-closed HAL */
    LOXBUDGET_ERR_ALIGNMENT          = -10,
    LOXBUDGET_ERR_VERSION_MISMATCH   = -11
} loxbudget_status_t;
```

### Decision actions

```c
typedef enum {
    LOXBUDGET_ALLOW_FULL     = 0,
    LOXBUDGET_ALLOW_DEGRADED = 1,
    LOXBUDGET_WAIT           = 2,
    LOXBUDGET_REJECT         = 3,
    LOXBUDGET_LOCKDOWN       = 4
} loxbudget_action_t;
```

### Pressure states

```c
typedef enum {
    LOXBUDGET_PRESSURE_NORMAL    = 0,
    LOXBUDGET_PRESSURE_ELEVATED  = 1,
    LOXBUDGET_PRESSURE_CRITICAL  = 2,
    LOXBUDGET_PRESSURE_SURVIVAL  = 3,
    LOXBUDGET_PRESSURE_LOCKDOWN  = 4
} loxbudget_pressure_t;
```

### Resource kinds

```c
typedef enum {
    LOXBUDGET_RES_REUSABLE   = 0,  /* RAM, slots: reserved and returned */
    LOXBUDGET_RES_CONSUMABLE = 1,  /* flash writes, packets: spent */
    LOXBUDGET_RES_STATE      = 2   /* boolean preconditions: boot proven */
} loxbudget_resource_kind_t;
```

### Priority

```c
typedef enum {
    LOXBUDGET_PRIO_LOW      = 0,
    LOXBUDGET_PRIO_NORMAL   = 1,
    LOXBUDGET_PRIO_HIGH     = 2,
    LOXBUDGET_PRIO_CRITICAL = 3
} loxbudget_priority_t;
```

### Reason codes

```c
typedef enum {
    LOXBUDGET_REASON_OK                  = 0,
    LOXBUDGET_REASON_INSUFFICIENT_RES    = 1,
    LOXBUDGET_REASON_RATE_LIMIT          = 2,
    LOXBUDGET_REASON_LIFETIME_EXHAUSTED  = 3,
    LOXBUDGET_REASON_PRESSURE_BLOCK      = 4,
    LOXBUDGET_REASON_LOCKDOWN_ACTIVE     = 5,
    LOXBUDGET_REASON_PRECONDITION_FAIL   = 6,
    LOXBUDGET_REASON_CAUSAL_CASCADE      = 7,
    LOXBUDGET_REASON_UNKNOWN_OP          = 8,
    LOXBUDGET_REASON_HAL_NOT_CONFIGURED  = 9
} loxbudget_reason_t;
```

### Decision result

```c
typedef struct {
    loxbudget_action_t       action;
    loxbudget_pressure_t     pressure;
    loxbudget_resource_id_t  denied_resource;  /* valid if action != ALLOW_* */
    uint16_t                 requested;
    uint16_t                 available;
    uint8_t                  reason;
} loxbudget_decision_t;
```

### Resource entry (internal)

```c
typedef struct {
    uint16_t limit;
    uint16_t used;
    uint16_t reserved;
    uint16_t high_watermark;
    uint8_t  kind;
    uint8_t  flags;
} loxbudget_resource_t;
```

`_Static_assert(sizeof(loxbudget_resource_t) == 12, ...)` — required.

### Resource view (read-only snapshot)

```c
typedef struct {
    uint16_t limit;
    uint16_t used;
    uint16_t reserved;
    uint16_t available;
    uint16_t high_watermark;
    uint8_t  kind;
} loxbudget_resource_view_t;
```

### Operation profile

```c
typedef struct {
    loxbudget_op_id_t       op_id;
    uint8_t                 priority;       /* loxbudget_priority_t */
    uint8_t                 action_normal;
    uint8_t                 action_elevated;
    uint8_t                 action_critical;
    uint8_t                 action_survival;
    uint8_t                 action_lockdown;
    uint8_t                 flags;
} loxbudget_op_profile_t;
```

`_Static_assert(sizeof(loxbudget_op_profile_t) == 8, ...)` — required.

### Operation flags

```c
#define LOXBUDGET_OPF_REQUIRES_BOOT_PROVEN  (1u << 0)
#define LOXBUDGET_OPF_REQUIRES_VOLTAGE_OK   (1u << 1)
#define LOXBUDGET_OPF_REQUIRES_NETWORK_UP   (1u << 2)
#define LOXBUDGET_OPF_PERSIST_AUDIT         (1u << 3)
#define LOXBUDGET_OPF_BYPASS_RATE_LIMIT     (1u << 4)
#define LOXBUDGET_OPF_CALIBRATABLE          (1u << 5)
#define LOXBUDGET_OPF_LOCKDOWN_PASS         (1u << 6)  /* allowed in LOCKDOWN */
```

### Lease (packed for 8-byte size, no padding)

```c
typedef struct {
    uint32_t acquired_at_ms;   /* 32-bit aligned first */
    uint16_t magic;            /* per-instance validation token */
    uint8_t  id;               /* slot index */
    uint8_t  op;               /* op_id this lease was issued for */
} loxbudget_lease_t;
```

`_Static_assert(sizeof(loxbudget_lease_t) == 8, "lease must be 8 bytes, no padding")` — required.

### Need (resource requirement)

```c
typedef struct {
    loxbudget_resource_id_t resource;
    uint8_t                 _pad;
    uint16_t                amount;
} loxbudget_need_t;
```

`_Static_assert(sizeof(loxbudget_need_t) == 4, ...)` — required.

### Decision record (audit trail)

```c
typedef struct {
    uint32_t                timestamp_ms;
    loxbudget_op_id_t       op_id;
    loxbudget_resource_id_t denied_resource;
    uint16_t                requested;
    uint16_t                available;
    uint8_t                 action;
    uint8_t                 pressure;
    uint8_t                 reason;
    uint8_t                 _pad;
} loxbudget_decision_record_t;
```

`_Static_assert(sizeof(loxbudget_decision_record_t) == 16, ...)` — required.

### Calibration sample

```c
typedef struct {
    uint16_t ram_used;
    uint16_t stack_estimate;
    uint16_t flash_writes;
    uint16_t queue_peak;
    uint32_t duration_us;
} loxbudget_sample_t;
```

### Suggested profile (calibration output)

```c
typedef struct {
    uint16_t ram_p50;
    uint16_t ram_p95;
    uint16_t ram_p99;
    uint16_t ram_max;             /* observed maximum */
    uint32_t duration_p95_us;
    uint32_t duration_p99_us;
    uint32_t duration_max_us;
    uint16_t suggested_ram_limit;
    uint32_t suggested_time_limit_us;
    uint16_t outlier_count;
    uint32_t sample_count;
} loxbudget_suggested_profile_t;
```

> **Note**: `p99.9` is intentionally not tracked. P² estimator convergence to p99.9 requires 10 000+ samples, which is impractical for typical embedded calibration runs. The library tracks p50/p95/p99 and observed max; suggested limits use `max(p99 + abs_margin, observed_max + pct_margin)`.

### Configuration

```c
typedef struct {
    uint8_t  max_resources;
    uint8_t  max_ops;
    uint8_t  max_concurrent_leases;
    uint8_t  audit_size;             /* power of 2 or 0 */
    uint8_t  hal_strict;             /* 1 = fail-closed, 0 = allow weak defaults */
    uint8_t  _reserved[3];
    uint16_t flags;
    /* Optional callback HAL (overrides weak symbols if non-NULL) */
    const loxbudget_hal_callbacks_t *hal_callbacks;
    void *hal_user;
} loxbudget_config_t;
```

> **Important**: `hal_strict` defaults to `1` (fail-closed). Setting it to `0` requires explicit acknowledgement in code that the application accepts permissive HAL defaults.

### Snapshot (full read-only state)

```c
typedef struct {
    uint8_t  pressure;
    uint8_t  active_lease_count;
    uint8_t  resource_count;
    uint8_t  op_count;
    uint32_t total_decisions;
    uint32_t total_grants;
    uint32_t total_denials;
    uint32_t total_degradations;
    uint32_t uptime_ms;
} loxbudget_snapshot_t;
```

---

## 7. Decision Engine

### Algorithm

For an operation `op` at pressure `P`:

1. **Look up profile** by `op_id`. If not found → `REJECT`, reason `UNKNOWN_OP`.
2. **Check pressure-mapped action** from profile (`action_normal` … `action_lockdown`).
   If the mapped action is already `WAIT/REJECT/LOCKDOWN`, return it immediately (with reason `PRESSURE_BLOCK` or `LOCKDOWN_ACTIVE`).
3. **Special case for LOCKDOWN pressure**:
   - Operations with `OPF_LOCKDOWN_PASS` flag may proceed if their explicit action_lockdown is not REJECT.
   - This ensures `PANIC_DUMP` and similar critical evidence operations can complete even in LOCKDOWN.
4. **Check HAL preconditions** (e.g. `OPF_REQUIRES_BOOT_PROVEN`).
   - If `hal_strict = 1` and the required HAL function is not configured → `REJECT`, reason `HAL_NOT_CONFIGURED`.
   - If the HAL function returns `LOXBUDGET_FALSE` → `REJECT`, reason `PRECONDITION_FAIL`.
5. **For each declared need**:
   - If resource is `STATE` and not satisfied → `REJECT`, reason `PRECONDITION_FAIL`.
   - If `available < amount` → `WAIT` or `REJECT` (depending on profile mapping), reason `INSUFFICIENT_RES`.
   - If rate limit (when enabled) violated → `REJECT`, reason `RATE_LIMIT`.
   - If lifetime limit reached → `REJECT`, reason `LIFETIME_EXHAUSTED`.
6. **For causality (when enabled)**: walk transitive `may_trigger` graph up to depth limit; check budget for cascade with Q8-scaled needs.
7. **All checks pass** → return mapped action (`ALLOW_FULL` or `ALLOW_DEGRADED`).

### Determinism guarantees

- All loops are bounded by compile-time constants (`MAX_RESOURCES`, `MAX_OPS`, `MAX_NEEDS_PER_OP`).
- No recursion. Causality graph walked iteratively with explicit stack of fixed depth.
- No allocation. No syscalls. No floats.
- `check()` complexity: `O(needs_per_op + causality_depth × max_edges)`.
- `enter()` complexity: same as `check()` plus O(needs_per_op) for atomic reservation.

### Atomic reservation

`enter` reserves all needs in two phases:

1. **Validation phase** — verify every need can be satisfied; do not mutate.
2. **Commit phase** — apply all reservations in a single critical section.

If validation fails at any need, no mutation occurs. This guarantees no partial state.

### Determinism invariant test

```c
/* This test must always pass: */
void test_decision_is_pure(void) {
    loxbudget_decision_t d1, d2;
    loxbudget_check(&budget, op, &d1);
    loxbudget_check(&budget, op, &d2);
    /* No state changed between calls — same input → same output */
    assert(memcmp(&d1, &d2, sizeof(d1)) == 0);
}
```

---

## 8. Pressure State Machine

### State transitions

Pressure is set by the application (typically driven by an external health monitor like `microhealth`). The library does not auto-detect pressure from internal counters in V1.0, but it does:

- Maintain hysteresis hints via watermarks.
- Expose burn rate (when rate windows are enabled) so the application can decide.
- Allow forced transitions via `loxbudget_set_pressure`.

### Recommended transition policy (informational)

| From       | To         | Trigger                                      |
|------------|------------|----------------------------------------------|
| NORMAL     | ELEVATED   | any resource > 70% utilization               |
| ELEVATED   | CRITICAL   | any resource > 85% utilization               |
| CRITICAL   | SURVIVAL   | any resource > 95% or critical denial        |
| any        | LOCKDOWN   | external command (panic, crash-loop, etc.)   |
| LOCKDOWN   | any        | only after boot validation                   |

This is **policy**, not enforced. The library accepts whatever the application sets.

### Effect on operations

Each operation profile maps each pressure state to an action:

```c
const loxbudget_op_profile_t mqtt_publish = {
    .op_id            = LOX_OP_MQTT_PUBLISH,
    .priority         = LOXBUDGET_PRIO_NORMAL,
    .action_normal    = LOXBUDGET_ALLOW_FULL,
    .action_elevated  = LOXBUDGET_ALLOW_DEGRADED,
    .action_critical  = LOXBUDGET_ALLOW_DEGRADED,
    .action_survival  = LOXBUDGET_REJECT,
    .action_lockdown  = LOXBUDGET_REJECT,
    .flags            = 0
};

const loxbudget_op_profile_t panic_dump = {
    .op_id            = LOX_OP_PANIC_DUMP,
    .priority         = LOXBUDGET_PRIO_CRITICAL,
    .action_normal    = LOXBUDGET_ALLOW_FULL,
    .action_elevated  = LOXBUDGET_ALLOW_FULL,
    .action_critical  = LOXBUDGET_ALLOW_FULL,
    .action_survival  = LOXBUDGET_ALLOW_FULL,
    .action_lockdown  = LOXBUDGET_ALLOW_FULL,
    .flags            = LOXBUDGET_OPF_LOCKDOWN_PASS |
                        LOXBUDGET_OPF_BYPASS_RATE_LIMIT |
                        LOXBUDGET_OPF_PERSIST_AUDIT
};
```

### LOCKDOWN audit guarantee

Even in LOCKDOWN pressure, operations with `OPF_PERSIST_AUDIT | OPF_LOCKDOWN_PASS` flags must be allowed to record their own audit entry. Otherwise post-mortem analysis can never determine why the system entered LOCKDOWN.

---

## 9. Resource Model

### Resource kinds

**Reusable** — reserved on enter, returned on leave. Examples: RAM arena bytes, queue slots, parser depth.

**Consumable** — spent permanently (within a window or lifetime). Examples: flash writes, packets sent.

**State** — boolean preconditions evaluated externally. Examples: boot-proven, voltage-OK, network-up.

### Resource semantics

| Operation        | Reusable                          | Consumable                | State                    |
|------------------|-----------------------------------|---------------------------|--------------------------|
| `enter`          | reserved += amount                | used += amount            | check only               |
| `leave`          | reserved -= amount                | no change                 | no change                |
| availability     | limit - used - reserved           | limit - used              | external bool            |

### Watermark tracking

Every resource maintains `high_watermark` updated atomically on each successful reservation. Useful for production diagnostics: *"what was the peak memory we ever needed?"*

### Resource overflow protection

All counters use `uint16_t` and saturate (do not wrap). An attempt to push past the limit is denied, never accepted with overflow.

---

## 10. Operation Profiles

### Static (X-macro) approach — recommended for MCU

Define operations and resources in `.def` files:

```c
/* app_resources.def */
LOXBUDGET_RES(RAM_ARENA,     4096, REUSABLE)
LOXBUDGET_RES(MQTT_OUTBOX,   8,    REUSABLE)
LOXBUDGET_RES(FLASH_WRITES,  32,   CONSUMABLE)
LOXBUDGET_RES(BOOT_PROVEN,   1,    STATE)

/* app_operations.def: name, prio, normal, elev, crit, surv, lockdown, flags */
LOXBUDGET_OP(MQTT_PUBLISH, NORMAL,   ALLOW_FULL, ALLOW_DEGRADED, ALLOW_DEGRADED, REJECT,     REJECT,     0)
LOXBUDGET_OP(OTA_UPDATE,   HIGH,     ALLOW_FULL, REJECT,         REJECT,         REJECT,     REJECT,     OPF_REQUIRES_BOOT_PROVEN)
LOXBUDGET_OP(DEBUG_LOG,    LOW,      ALLOW_FULL, ALLOW_DEGRADED, REJECT,         REJECT,     REJECT,     0)
LOXBUDGET_OP(PANIC_DUMP,   CRITICAL, ALLOW_FULL, ALLOW_FULL,     ALLOW_FULL,     ALLOW_FULL, ALLOW_FULL, OPF_LOCKDOWN_PASS|OPF_PERSIST_AUDIT)
```

Generation patterns:

```c
/* Generate enum */
#define LOXBUDGET_RES(name, limit, kind) LOX_RES_##name,
typedef enum {
    #include "app_resources.def"
    LOX_RES_COUNT
} loxbudget_resource_id_t;
#undef LOXBUDGET_RES

/* Generate initialization */
#define LOXBUDGET_RES(name, lim, kind) \
    loxbudget_set_resource(&budget, LOX_RES_##name, lim, LOXBUDGET_RES_##kind);
#include "app_resources.def"
#undef LOXBUDGET_RES
```

### Dynamic (runtime) approach — for host tooling and tests

```c
loxbudget_op_profile_t profile = {
    .op_id           = LOX_OP_MQTT_PUBLISH,
    .priority        = LOXBUDGET_PRIO_NORMAL,
    .action_normal   = LOXBUDGET_ALLOW_FULL,
    /* ... */
};
loxbudget_register_op(&budget, &profile);
loxbudget_op_set_need(&budget, LOX_OP_MQTT_PUBLISH, LOX_RES_RAM_ARENA, 512);
loxbudget_op_set_need(&budget, LOX_OP_MQTT_PUBLISH, LOX_RES_MQTT_OUTBOX, 1);
```

Both approaches share the same internal representation.

---

## 11. Feature Gates and Profiles

### Compile-time switches

```c
/* loxbudget_config.h - defaults */

#ifndef LOXBUDGET_ENABLE_AUDIT_TRAIL
  #define LOXBUDGET_ENABLE_AUDIT_TRAIL    0
#endif

#ifndef LOXBUDGET_ENABLE_RATE_WINDOWS
  #define LOXBUDGET_ENABLE_RATE_WINDOWS   0
#endif

#ifndef LOXBUDGET_ENABLE_CALIBRATION
  #define LOXBUDGET_ENABLE_CALIBRATION    0
#endif

#ifndef LOXBUDGET_ENABLE_CAUSALITY
  #define LOXBUDGET_ENABLE_CAUSALITY      0
#endif

#ifndef LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS
  #define LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS 0
#endif

#ifndef LOXBUDGET_MAX_RESOURCES
  #define LOXBUDGET_MAX_RESOURCES         8
#endif

#ifndef LOXBUDGET_MAX_OPS
  #define LOXBUDGET_MAX_OPS               16
#endif

#ifndef LOXBUDGET_MAX_NEEDS_PER_OP
  #define LOXBUDGET_MAX_NEEDS_PER_OP      4
#endif

#ifndef LOXBUDGET_MAX_LEASES
  #define LOXBUDGET_MAX_LEASES            4
#endif

#ifndef LOXBUDGET_AUDIT_SIZE
  #define LOXBUDGET_AUDIT_SIZE            0    /* must be 0 or power of 2 */
#endif
```

### Convenience profiles

```c
#define LOXBUDGET_PROFILE_TINY          0
#define LOXBUDGET_PROFILE_STANDARD      1
#define LOXBUDGET_PROFILE_FULL          2
#define LOXBUDGET_PROFILE_EXPERIMENTAL  3
```

| Profile        | Audit | Rate | Calib | Causality | Strings | Default audit_size |
|----------------|-------|------|-------|-----------|---------|--------------------|
| TINY           |   ✗   |  ✗   |   ✗   |     ✗     |    ✗    |        0           |
| STANDARD       |   ✓   |  ✗   |   ✗   |     ✗     |    ✗    |        16          |
| FULL           |   ✓   |  ✓   |   ✓   |     ✗     |    ✓    |        32          |
| EXPERIMENTAL   |   ✓   |  ✓   |   ✓   |     ✓     |    ✓    |        64          |

> **TINY profile rule**: `audit_size = 0` is mandatory. Audit code does not compile in.

Selection:

```c
#define LOXBUDGET_PROFILE LOXBUDGET_PROFILE_TINY
#define LOXBUDGET_IMPLEMENTATION
#include "loxbudget.h"
```

### Linker-friendliness rules

- **No global instances.** User owns all storage.
- **Library `.bss` = 0 bytes.** Asserted in CI on every build.
- **No global constructors.** No `__attribute__((constructor))`.
- Disabled features must produce zero code and zero data.
- Diagnostic strings live in a separate translation unit. With `LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS=0` they are not referenced.
- All optional features are guarded by `#if` (not `#ifdef`) to allow `=0` to fully exclude them.

---

## 12. HAL Layer (Fail-Closed)

### Required functions

```c
uint32_t loxbudget_hal_now_ms(void);
```

Returns monotonic milliseconds since boot. Required if any of: rate windows, audit timestamps, calibration durations, lease age.

### Optional functions

```c
void loxbudget_hal_critical_enter(void);
void loxbudget_hal_critical_exit(void);
```

Called around mutating operations if the application is multi-threaded or the budget is accessed from interrupt context.

Default weak implementations are no-ops. On bare-metal single-threaded firmware nothing further is needed.

### State queries

```c
loxbudget_bool_t loxbudget_hal_boot_proven(void);
loxbudget_bool_t loxbudget_hal_voltage_ok(void);
loxbudget_bool_t loxbudget_hal_network_up(void);
```

### Fail-closed semantics (CRITICAL)

When `hal_strict = 1` (the default):

1. If any registered operation has `OPF_REQUIRES_BOOT_PROVEN` and **no** `boot_proven` callback was provided in `cfg->hal_callbacks` AND no non-weak symbol overrides the default, then `loxbudget_register_op` returns `LOXBUDGET_ERR_HAL_NOT_CONFIGURED`.
2. Same for `OPF_REQUIRES_VOLTAGE_OK` and `OPF_REQUIRES_NETWORK_UP`.
3. The default weak implementations exist only to make the library link; in strict mode their use is detected and refused at registration time.

Detection mechanism:

```c
/* Default weak implementations are tagged so the library can detect
   they have not been overridden. */
LOXBUDGET_WEAK loxbudget_bool_t loxbudget_hal_boot_proven(void) {
    /* This default is never reachable in strict mode. 
       If ever called, it returns FALSE (fail-closed). */
    return LOXBUDGET_FALSE;
}
```

The library uses an explicit registration check rather than relying on weak-symbol introspection (which is not portable across MSVC/IAR).

When `hal_strict = 0`:

- The library accepts weak defaults.
- This must be opted in explicitly in `cfg->hal_strict = 0`.
- This is intended for host tests and the minimal example, never for production firmware.

### Permissive HAL helper

```c
/* Explicit, named function — applications must invoke it consciously. */
const loxbudget_hal_callbacks_t *
loxbudget_hal_default_permissive(void);
```

This returns a callback struct where all state queries return `LOXBUDGET_TRUE`. Use cases:

- Host unit tests that don't model HAL state.
- The `01_bare_metal_minimal` example.

Production code must not use this. CI checks for its presence in production builds and warns.

### Weak symbol macro

```c
#if defined(__GNUC__) || defined(__clang__)
  #define LOXBUDGET_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
  #define LOXBUDGET_WEAK __weak
#elif defined(__ARMCC_VERSION)
  #define LOXBUDGET_WEAK __attribute__((weak))
#elif defined(_MSC_VER)
  #define LOXBUDGET_WEAK  /* MSVC: requires callback HAL fallback */
#else
  #define LOXBUDGET_WEAK
#endif
```

### Callback HAL (preferred for portability)

```c
typedef struct {
    uint32_t          (*now_ms)(void *user);
    void              (*critical_enter)(void *user);
    void              (*critical_exit)(void *user);
    loxbudget_bool_t  (*boot_proven)(void *user);
    loxbudget_bool_t  (*voltage_ok)(void *user);
    loxbudget_bool_t  (*network_up)(void *user);
} loxbudget_hal_callbacks_t;
```

If `cfg->hal_callbacks` is non-NULL, the library uses callbacks exclusively and ignores weak symbols. This is the recommended approach for MSVC, IAR, and any toolchain where weak-symbol behavior is uncertain.

---

## 13. Storage Model

### User-supplied buffer

`LOXBUDGET_REQUIRED_SIZE` is a **compile-time macro** (not a function), so user storage can live in `.bss` with size known at compile time.

```c
#define LOXBUDGET_REQUIRED_SIZE(n_res, n_ops, audit_n)             \
    ( /* header */                                                 \
      32u                                                          \
    + /* resource table */                                         \
      (n_res)  * 12u                                               \
    + /* op profile table */                                       \
      (n_ops)  * 8u                                                \
    + /* op needs table */                                         \
      (n_ops)  * LOXBUDGET_MAX_NEEDS_PER_OP * 4u                   \
    + /* lease slots */                                            \
      LOXBUDGET_MAX_LEASES * 8u                                    \
    + /* audit ring */                                             \
      (audit_n) * 16u                                              \
    + /* alignment slack */                                        \
      16u )
```

Use:

```c
static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(8, 16, 32)];
loxbudget_t budget;
loxbudget_init(&budget, storage, sizeof(storage), &cfg);
```

### Layout

```
+--------------------------------------+ offset 0
| Header (instance metadata + magic)   |
+--------------------------------------+
| Resource table (n_resources entries) |
+--------------------------------------+
| Op profile table (n_ops entries)     |
+--------------------------------------+
| Op needs table (n_ops × max_needs)   |
+--------------------------------------+
| Lease table (max_leases entries)     |
+--------------------------------------+
| Rate windows (if enabled)            |
+--------------------------------------+
| Audit ring (if enabled, audit_size)  |
+--------------------------------------+
| Calibration buffer (if enabled)      |
+--------------------------------------+
```

### Alignment

All sub-structures are sized to `sizeof(uint32_t)` boundaries. The buffer must be `uint32_t`-aligned. The init function checks alignment and returns `LOXBUDGET_ERR_ALIGNMENT` if violated.

### Multiple instances

```c
static uint8_t app_storage[LOXBUDGET_REQUIRED_SIZE(8, 16, 32)];
static uint8_t boot_storage[LOXBUDGET_REQUIRED_SIZE(2, 4, 0)];

loxbudget_t app_budget;
loxbudget_t boot_budget;

loxbudget_init(&app_budget,  app_storage,  sizeof(app_storage),  &app_cfg);
loxbudget_init(&boot_budget, boot_storage, sizeof(boot_storage), &boot_cfg);
```

### Persistence across resets

Place the storage buffer in a `.noinit` section to preserve audit trail and watermarks across soft resets:

```c
__attribute__((section(".noinit")))
static uint8_t budget_storage[LOXBUDGET_REQUIRED_SIZE(8, 16, 32)];
```

The init function detects valid magic and preserves audit/watermark data while resetting volatile state.

### Per-instance lease magic

To prevent stale leases from one instance being accepted by another:

```c
/* On init: */
budget->lease_magic_base = (uint16_t)((uintptr_t)budget ^ now_ms());

/* On enter (slot_index is the lease_table index): */
lease->magic = budget->lease_magic_base ^ (uint16_t)slot_index;

/* On leave: */
if (lease.magic != (budget->lease_magic_base ^ slot_index)) {
    return LOXBUDGET_ERR_BAD_STATE;
}
```

---

## 14. Self-Calibration Mode

### Purpose

Eliminate guesswork in setting limits. Run firmware in calibration mode under realistic conditions; loxbudget records resource usage and suggests safe limits.

### Workflow

```c
/* 1. Begin calibration for a specific operation */
loxbudget_calibrate_begin(&budget, LOX_OP_MQTT_PUBLISH, 1000);

/* 2. Inside the operation, sample resource usage */
void mqtt_publish(void) {
    loxbudget_sample_t sample = {0};
    uint32_t t0 = loxbudget_hal_now_ms();

    /* ... do work, track peak RAM, count flash writes ... */

    sample.ram_used     = arena_peak_used();
    sample.flash_writes = flash_write_count();
    sample.queue_peak   = mqtt_queue_peak();
    sample.duration_us  = (loxbudget_hal_now_ms() - t0) * 1000u;

    loxbudget_calibrate_sample(&budget, LOX_OP_MQTT_PUBLISH, &sample);
}

/* 3. After enough samples, get suggested profile */
loxbudget_suggested_profile_t suggested;
loxbudget_calibrate_end(&budget, LOX_OP_MQTT_PUBLISH, &suggested);
```

### Estimator

Uses the **P² algorithm** (Jain & Chlamtac, 1985) for online percentile estimation. Properties:

- O(1) memory per percentile tracked.
- O(1) per sample.
- No sample storage required.
- Tracks p50, p95, p99 simultaneously.
- All math is **integer or Q16.16 fixed-point** — no floats.

Storage per calibration target: ~64 bytes.

> **p99.9 not tracked**: P² convergence to p99.9 requires 10 000+ samples. For typical embedded calibration runs (hundreds to a few thousand samples), p99 plus observed_max gives a more robust suggestion.

### Outlier detection (no IQR, integer-only)

A sample is flagged as outlier if **either** condition holds:

```c
sample > (p99 * 3u) / 2u       /* sample > p99 × 1.5 */
sample > (max_at_500 * 12u) / 10u  /* sample > stable_max × 1.2 */
```

`max_at_500` is the observed maximum after the first 500 samples (a "stable baseline"). Before sample 500, only the first condition applies.

This approach is integer-only, requires no IQR tracking, and catches the two practical outlier patterns: gross outliers and late surges.

### Suggested limits formula

```c
/* All integer arithmetic. All values in same units. */
suggested_ram_limit = MAX(
    p99 + RAM_ABS_MARGIN,           /* e.g. RAM_ABS_MARGIN = 32 bytes */
    (observed_max * 105u) / 100u    /* observed_max + 5% */
);

suggested_time_limit_us = MAX(
    p99_us + TIME_ABS_MARGIN_US,    /* e.g. 500 us */
    (observed_max_us * 110u) / 100u /* observed_max + 10% */
);
```

The margins ensure that even if all samples were at max, the limit gives breathing room. Users may override.

### Calibration report (host tool)

```
$ loxbudget-report calibration.bin

Operation: mqtt_publish
  Samples:        1000
  RAM:
    p50:          256 B
    p95:          384 B
    p99:          448 B
    max:          512 B
    suggested:    538 B  (max + 5%)
  Duration:
    p95:          1800 us
    p99:          2300 us
    max:          2900 us
    suggested:    3190 us  (max + 10%)
  Flash writes per op: 1.0 (avg)
  Outliers:       3 (likely OOM near-miss)

  Confidence:     HIGH (1000+ samples)
  Recommendation: limits are safe for production
```

---

## 15. Time-Windowed Budgets

### Purpose

Prevent flash wear, MQTT storms, log floods, and queue saturation by limiting consumable resources over time windows.

### Window types

```c
typedef enum {
    LOXBUDGET_WINDOW_SECOND   = 0,
    LOXBUDGET_WINDOW_MINUTE   = 1,
    LOXBUDGET_WINDOW_HOUR     = 2,
    LOXBUDGET_WINDOW_DAY      = 3,
    LOXBUDGET_WINDOW_LIFETIME = 4
} loxbudget_window_t;
```

### Implementation: Q16.16 token bucket (no floats)

```c
typedef struct {
    uint32_t tokens_q16;        /* current tokens, Q16.16 fixed-point */
    uint32_t capacity_q16;      /* max tokens (Q16.16) */
    uint32_t refill_per_ms_q16; /* tokens added per ms (Q16.16) */
    uint32_t last_refill_ms;
} loxbudget_bucket_t;
```

Refill computation (integer-only):

```c
/* refill_per_ms_q16 is precomputed at configuration time:
   For 60 tokens per minute: (60 << 16) / 60000 ms = 65 (Q16.16)
   For 1000 tokens per hour: (1000 << 16) / 3600000 ms = 18 (Q16.16) */

uint32_t now_ms = loxbudget_hal_now_ms();
uint32_t elapsed = now_ms - bucket->last_refill_ms;

/* Saturating add to prevent overflow */
uint64_t refill = (uint64_t)elapsed * bucket->refill_per_ms_q16;
uint64_t new_tokens = (uint64_t)bucket->tokens_q16 + refill;

if (new_tokens > bucket->capacity_q16) {
    new_tokens = bucket->capacity_q16;
}
bucket->tokens_q16 = (uint32_t)new_tokens;
bucket->last_refill_ms = now_ms;

/* Consume: amount must be in Q16.16 too: amount << 16 */
uint32_t consume_q16 = (uint32_t)amount << 16;
if (bucket->tokens_q16 < consume_q16) {
    return DENIED;  /* Not enough tokens */
}
bucket->tokens_q16 -= consume_q16;
return GRANTED;
```

### Lifetime bucket (special case)

`LOXBUDGET_WINDOW_LIFETIME` does not use the token bucket. Instead, a simple monotonic counter:

```c
typedef struct {
    uint32_t consumed;
    uint32_t limit;
} loxbudget_lifetime_t;
```

When `consumed >= limit`, the resource is permanently denied unless explicitly reset (reserved for OEM service mode).

### Multi-window combining

```c
loxbudget_set_rate_limit(&budget, LOX_RES_FLASH_WRITES, LOXBUDGET_WINDOW_MINUTE, 60);
loxbudget_set_rate_limit(&budget, LOX_RES_FLASH_WRITES, LOXBUDGET_WINDOW_HOUR,   1000);
loxbudget_set_lifetime_limit(&budget, LOX_RES_FLASH_WRITES, 100000);
```

Operation passes only if **all** configured windows allow it. A single denial from any window denies the operation.

### Burn rate query

```c
typedef struct {
    uint32_t per_minute;
    uint32_t per_hour;
    uint32_t estimated_exhaustion_ms;  /* 0 = never / not applicable */
} loxbudget_burn_rate_t;

loxbudget_get_burn_rate(&budget, LOX_RES_FLASH_WRITES, &br);
```

The library does **not** auto-degrade based on burn rate. It exposes the data; the application decides whether to escalate pressure.

### Alternative: fixed-window counter (V0.3 fallback)

For environments where Q16.16 token buckets are too complex to verify, a simpler fixed-window counter is available:

```c
typedef struct {
    uint32_t window_start_ms;
    uint32_t window_duration_ms;
    uint32_t consumed;
    uint32_t limit;
} loxbudget_fixed_window_t;
```

Reset `consumed` to 0 when `now_ms - window_start_ms >= window_duration_ms`. Less accurate (allows up to 2× burst at window boundaries) but simpler. Selectable at compile time:

```c
#define LOXBUDGET_RATE_IMPL_TOKEN_BUCKET   1
#define LOXBUDGET_RATE_IMPL_FIXED_WINDOW   2

#ifndef LOXBUDGET_RATE_IMPL
  #define LOXBUDGET_RATE_IMPL LOXBUDGET_RATE_IMPL_FIXED_WINDOW
#endif
```

V0.3 ships with fixed-window default; V0.4+ may switch default to token bucket once the implementation is fuzzed and verified.

---

## 16. Audit Trail

### Purpose

Record every decision so that post-mortem analysis can answer *"why was this operation denied just before the crash?"*

### Storage

Fixed-size ring buffer in user-owned storage. Configurable size (must be power of 2 or zero).

### Sizing recommendations

| Target class                     | Recommended `audit_size` | Audit RAM |
|----------------------------------|--------------------------|-----------|
| TINY profile (always)            | 0 (audit disabled)       | 0 B       |
| ATmega/Cortex-M0+ (≤ 4 KB RAM)   | 0 or 8                   | 0–128 B   |
| Cortex-M3 (8–16 KB RAM)          | 16                       | 256 B     |
| Cortex-M4F+ (32+ KB RAM)         | 32–64                    | 512 B–1 KiB |

> **Rule**: TINY profile requires `audit_size = 0`. Audit code does not compile in.

### Recorded fields

See `loxbudget_decision_record_t` in §6 (16 bytes).

### Retrieval

```c
loxbudget_decision_record_t records[16];
size_t count;
loxbudget_audit_get_recent(&budget, records, 16, &count);
```

Retrieves the most recent N records in newest-first order. Non-mutating.

### Filtering (V0.4+, optional)

```c
loxbudget_audit_filter_t f = {
    .op_mask     = (1u << LOX_OP_OTA_UPDATE),
    .action_mask = (1u << LOXBUDGET_REJECT) | (1u << LOXBUDGET_LOCKDOWN),
    .since_ms    = boot_time_ms - 60000u
};
loxbudget_audit_get_filtered(&budget, &f, records, 16, &count);
```

Optional. Compile out if `LOXBUDGET_ENABLE_AUDIT_FILTER=0`.

### Integration with loxguard / nvlog

Adapter modules (separate translation units) provide:

```c
/* loxbudget_loxguard_adapter.h */
void loxbudget_loxguard_attach(loxbudget_t *budget,
                               loxguard_t *guard);
/* On critical denials or lockdown, the adapter pushes audit
   records into loxguard's blackbox automatically. */
```

### Clean separation

> **loxbudget**: *why an operation was not allowed.*
> **loxguard**: *what happened when something went wrong despite controls.*

---

## 17. Causality Tracking (V1.1)

> **Status**: not in MVP. Planned for V1.1 after V1.0 sees production use.

### Purpose

Solve the cascading-cost problem: each operation looks safe in isolation, but their downstream effects together overload the system.

### API

```c
loxbudget_op_may_trigger(&budget,
    LOX_OP_MQTT_PUBLISH,    /* parent */
    LOX_OP_NVLOG_WRITE,     /* child  */
    LOXBUDGET_TRIGGER_ALWAYS);

loxbudget_op_may_trigger(&budget,
    LOX_OP_PARSER_RESPONSE,
    LOX_OP_MQTT_PUBLISH,
    LOXBUDGET_TRIGGER_MAYBE);
```

### Trigger kinds (Q8 fixed-point weights, no floats)

```c
typedef enum {
    LOXBUDGET_TRIGGER_NEVER  = 0,    /* Q8: 0.000 — disabled edge */
    LOXBUDGET_TRIGGER_RARE   = 32,   /* Q8: 0.125 */
    LOXBUDGET_TRIGGER_MAYBE  = 128,  /* Q8: 0.500 */
    LOXBUDGET_TRIGGER_ALWAYS = 255   /* Q8: ~1.000 */
} loxbudget_trigger_kind_t;
```

### Algorithm

When `check(parent)` is called:

1. Compute parent's direct needs (as usual).
2. Walk the may-trigger graph iteratively (DFS with explicit stack, max depth = 3 by default).
3. For each edge with weight `w` (Q8), add the child's needs scaled by `w`:
   ```c
   /* round-to-nearest fixed-point multiply */
   scaled_need = (need * weight_q8 + 128u) >> 8;
   ```
4. For `RARE` edges: ignore unless system is at `CRITICAL` or below.
5. Cycle detection: maintain a visited bitmap; on cycle, abort cascade and log a configuration warning.

### Limits

```c
#define LOXBUDGET_CAUSALITY_MAX_EDGES   32
#define LOXBUDGET_CAUSALITY_MAX_DEPTH   3
#define LOXBUDGET_CAUSALITY_VISIT_BITS  (LOXBUDGET_MAX_OPS)
```

### Cost

- Memory: `MAX_EDGES × 3 bytes = 96 B` for graph, plus `(MAX_OPS + 7)/8 = 2 B` visited bitmap.
- Time: O(edges × depth) per check, bounded by 96 operations.

### Why deferred to V1.1

- Correctness of cascade weighting needs real workload data.
- Cycle handling, edge ordering, and weight semantics will likely need iteration based on production feedback.
- V1.0 must prove the core decision engine first.

---

## 18. Adapters

Adapters are separate translation units that integrate loxbudget with other libraries. **Optional. Never required.**

### microlog adapter

```c
void loxbudget_microlog_attach(loxbudget_t *budget, microlog_t *log,
                               loxbudget_action_t min_action_to_log);
```

### microhealth adapter

```c
void loxbudget_microhealth_attach(loxbudget_t *budget, microhealth_t *health,
                                  const loxbudget_pressure_thresholds_t *t);
```

### microconf adapter

```c
loxbudget_status_t
loxbudget_microconf_load(loxbudget_t *budget, microconf_t *conf);
```

### microbus adapter

```c
void loxbudget_microbus_attach(loxbudget_t *budget, microbus_t *bus);
/* publishes: PRESSURE_CHANGED, BUDGET_DENIED, LOCKDOWN_ENTERED */
```

### nvlog adapter

```c
void loxbudget_nvlog_attach(loxbudget_t *budget, nvlog_t *log,
                            uint8_t min_severity);
```

### loxguard adapter

```c
void loxbudget_loxguard_attach(loxbudget_t *budget, loxguard_t *guard);
```

### Adapter rules

- Each adapter is a separate `.c` file.
- Adapters never appear in the core build.
- Adapters depend only on public APIs of both libraries.
- Adapters may have their own feature gates.

---

## 19. Repository Structure

### V0.1 minimal structure (target for first release)

```
loxbudget/
├── README.md
├── LICENSE                              # MIT
├── CHANGELOG.md
├── SPEC.md                              # this document
├── V0.1_SCOPE.md                        # frozen V0.1 scope
├── DESIGN.md                            # rationale and rules
│
├── include/
│   └── loxbudget.h                      # ~200 LOC: types, prototypes
│
├── src/
│   ├── loxbudget.c                      # ~600 LOC: single core file
│   └── loxbudget_hal.c                  # ~50 LOC: weak defaults
│
├── tests/
│   └── test_core.c                      # init, decision, atomic, pressure
│
├── examples/
│   └── 01_bare_metal_minimal/
│       └── main.c                       # < 50 lines
│
├── tools/
│   └── check_banned_symbols.sh
│
├── ci/
│   └── footprint_budget.yaml
│
├── Makefile
└── CMakeLists.txt
```

> **V0.1 hard rule**: no more than **one core `.c` file**. Splitting into multiple files is a V0.2+ activity, after the API has stabilized.

### V1.0+ target structure

```
loxbudget/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── SPEC.md
│
├── include/
│   └── loxbudget/
│       ├── loxbudget.h
│       ├── loxbudget_config.h
│       ├── loxbudget_hal.h
│       └── loxbudget_types.h
│
├── src/
│   ├── loxbudget_core.c
│   ├── loxbudget_resources.c
│   ├── loxbudget_ops.c
│   ├── loxbudget_audit.c                # opt-in
│   ├── loxbudget_rate.c                 # opt-in
│   ├── loxbudget_calibration.c          # opt-in
│   ├── loxbudget_causality.c            # opt-in (V1.1)
│   ├── loxbudget_strings.c              # opt-in
│   └── loxbudget_hal_default.c          # weak default HAL
│
├── single_header/
│   └── loxbudget.h                      # auto-generated
│
├── adapters/
│   ├── microlog/
│   ├── microhealth/
│   ├── microconf/
│   ├── microbus/
│   ├── nvlog/
│   └── loxguard/
│
├── examples/
│   ├── 01_bare_metal_minimal/
│   ├── 02_freertos_mqtt_storm/
│   ├── 03_esp_idf_flash_budget/
│   ├── 04_zephyr_basic/
│   ├── 05_ota_during_instability/
│   ├── 06_calibration_workflow/
│   └── 07_host_simulation/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── footprint/
│   └── fuzz/
│
├── tools/
│   ├── amalgamate.py
│   ├── footprint_check.py
│   ├── calibration_report.py
│   ├── audit_decoder.py
│   ├── scenario_replay.py
│   └── check_banned_symbols.sh
│
├── benchmarks/
├── ci/
├── docs/
├── Makefile
├── CMakeLists.txt
└── meson.build
```

---

## 20. Build System and Deployment

### Three integration paths

#### Path 1: Single-header drop-in (simplest)

```c
/* in exactly one .c file in your project */
#define LOXBUDGET_IMPLEMENTATION
#include "loxbudget.h"

/* in all other files */
#include "loxbudget.h"
```

No build system changes. Works with any C99 compiler.

#### Path 2: Make integration

```makefile
LOXBUDGET_DIR := vendor/loxbudget
include $(LOXBUDGET_DIR)/loxbudget.mk
```

#### Path 3: CMake integration

```cmake
add_subdirectory(vendor/loxbudget)
target_link_libraries(myapp PRIVATE loxbudget)

target_compile_definitions(myapp PRIVATE
    LOXBUDGET_ENABLE_AUDIT_TRAIL=1
    LOXBUDGET_ENABLE_RATE_WINDOWS=1
    LOXBUDGET_MAX_RESOURCES=12)
```

### Single-header generation

The single-header file is auto-generated from multi-file sources by `tools/amalgamate.py` (V1.0+). Until V1.0 the library *is* a single file, so amalgamation is unnecessary.

### Toolchain support matrix

| Toolchain          | Status   | Notes                              |
|--------------------|----------|------------------------------------|
| GCC ≥ 4.9          | full     | primary target                     |
| Clang ≥ 7          | full     | primary target                     |
| MSVC 2019+         | full     | host testing; uses callback HAL    |
| arm-none-eabi-gcc  | full     | Cortex-M targets                   |
| IAR EWARM ≥ 8.40   | tested   | weak symbols verified              |
| Keil MDK (armcc 6) | tested   | clang-based                        |
| avr-gcc            | core only| TINY profile fits ATmega328+       |
| riscv-none-elf-gcc | full     | CH32V, ESP32-C3 verified           |
| TI cl430           | best-effort | not in CI                       |

---

## 21. Testing Strategy

### Unit tests (V0.1 minimum)

Mandatory test cases for V0.1:

- `test_init`: invalid args, insufficient storage, alignment, valid init, double-init
- `test_decision_is_pure`: same input → same output (determinism invariant)
- `test_no_partial_reservation`: failed atomic enter → no resource changed
- `test_lease_lifecycle`: enter → leave, double-leave detection, magic validation
- `test_lease_magic_per_instance`: lease from instance A rejected by instance B
- `test_unknown_op`: graceful denial, no UB
- `test_hal_strict_fail_closed`: register_op with REQUIRES_BOOT_PROVEN and no HAL → ERR_HAL_NOT_CONFIGURED
- `test_struct_sizes`: `_Static_assert` on all key structs

V0.2+ adds:

- `test_audit_ring`: wrap-around, ordering, filter
- `test_lockdown_audit_passes`: PANIC_DUMP allowed in LOCKDOWN

V0.3+ adds:

- `test_rate_windows`: token bucket math, multi-window AND semantics, lifetime
- `test_rate_q16_no_overflow`: stress test Q16.16 arithmetic at edges

V1.0+ adds:

- `test_calibration_p2_accuracy`: P² estimator within 10% of analytical p99 for n ≥ 1000
- `test_calibration_outlier_detection`

### Integration tests (V0.3+)

- **MQTT storm**: 10 000 publish requests, network down, expect graceful degradation.
- **Flash burnout**: 100 minutes of debug logs, expect rate limit at 60/min.
- **OTA during lockdown**: forced lockdown, OTA must be denied with clear reason.
- **Boot recovery**: simulated crash loop, expect `BOOT_NOT_PROVEN` denials.

### Footprint tests

```yaml
# ci/footprint_budget.yaml
tiny:
  text_max:  4096
  bss_max:   0      # library .bss MUST be 0
standard:
  text_max:  8192
  bss_max:   0
full:
  text_max:  12288
  bss_max:   0
```

CI fails if any profile exceeds budget. **`bss_max = 0` is mandatory across all profiles.**

### Fuzz tests (V0.3+)

`libfuzzer` and `AFL++` against:

- Random inputs to `loxbudget_check`.
- Random sequences of `enter`/`leave`/`set_pressure`.
- Random calibration sample streams (when calibration is in scope).

Goal: no crashes, no UB-sanitizer reports after 24h fuzzing.

### Determinism tests

A separate test harness measures cycles for each public function across 1000 random inputs and asserts no outlier exceeds 2× median.

---

## 22. CI Requirements

### Required gates (must pass before merge)

1. **Compile matrix** — every supported toolchain, every profile.
2. **Unit tests** — 100% pass.
3. **Integration tests** (V0.3+) — 100% pass.
4. **Footprint check** — no profile exceeds its budget; library `.bss = 0`.
5. **Static analysis** — clean `clang-tidy`, `cppcheck`, `scan-build`.
6. **Sanitizers** — clean `-fsanitize=address,undefined` on host.
7. **No banned symbols** — `malloc`, `free`, `printf`, `fopen`, `exit`, `abort`, any float symbol must not appear in core.
8. **Single-header build** (V1.0+) — `loxbudget.h` compiles standalone.
9. **Coverage** — ≥ 95% lines on core.
10. **Format** — `clang-format` clean.
11. **Permissive HAL detection** — production builds warn if `loxbudget_hal_default_permissive` is referenced.

### Banned-symbols enforcement

```bash
#!/bin/bash
# tools/check_banned_symbols.sh
OBJ=$1
BANNED='malloc|free|calloc|realloc|printf|fprintf|sprintf|fopen|exit|abort'
BANNED_FLOAT='__floatsi|__floatdi|__divdf|__muldf|__adddf'

if nm --undefined-only "$OBJ" 2>/dev/null | grep -E "$BANNED|$BANNED_FLOAT"; then
    echo "FAIL: banned symbols referenced in $OBJ"
    exit 1
fi
echo "PASS: no banned symbols"
```

### Library `.bss = 0` enforcement

```bash
# CI script
BSS=$(arm-none-eabi-size build/tiny/loxbudget.o | awk 'NR==2 {print $3}')
if [ "$BSS" != "0" ]; then
    echo "FAIL: library .bss is $BSS, must be 0"
    exit 1
fi
```

### Footprint trend tracking

Each CI run posts flash/RAM numbers to a tracking dashboard. PRs that increase footprint by more than 5% require justification in the PR description.

### Cross-compilation builds (compile-only)

- `arm-none-eabi-gcc` for Cortex-M0/M3/M4/M7/M33
- `avr-gcc` for ATmega328 (TINY only)
- `riscv-none-elf-gcc` for RV32IMC
- `xtensa-esp32-elf-gcc` for ESP32

---

## 23. Footprint Targets

> **Numbers are targets, not promises.** Real numbers will be published once V0.1 is benchmarked.

### Honest footprint table

The library distinguishes **four** memory categories:

| Category              | Definition                                            |
|-----------------------|-------------------------------------------------------|
| Library `.text`       | Code in flash                                         |
| Library `.rodata`     | Constant data in flash                                |
| Library `.bss`        | **Must be 0**. Library has no global mutable state.   |
| Per-instance storage  | User-supplied buffer; size = `LOXBUDGET_REQUIRED_SIZE` |
| Stack peak            | Measured per public function                          |

### TINY profile (Cortex-M0+, -Os)

| Metric                | Target    |
|-----------------------|-----------|
| Library `.text`       | < 4 KiB   |
| Library `.rodata`     | < 256 B   |
| Library `.bss`        | **0 B**   |
| Per-instance (default config) | ~256 B    |
| Stack peak            | < 128 B   |
| `check()` latency     | < 200 cycles typical |
| `enter()` latency     | < 400 cycles typical |

### STANDARD profile (Cortex-M3, -O2)

| Metric                | Target     |
|-----------------------|------------|
| Library `.text`       | < 8 KiB    |
| Library `.rodata`     | < 512 B    |
| Library `.bss`        | **0 B**    |
| Per-instance (default config) | ~768 B     |
| Stack peak            | < 192 B    |
| `check()` latency     | < 300 cycles |
| `enter()` latency     | < 500 cycles |

### FULL profile (Cortex-M4F, -O2)

| Metric                | Target     |
|-----------------------|------------|
| Library `.text`       | < 12 KiB   |
| Library `.rodata`     | < 1 KiB    |
| Library `.bss`        | **0 B**    |
| Per-instance (default config) | ~1.5 KiB   |
| Stack peak            | < 256 B    |
| `check()` latency     | < 500 cycles |
| `enter()` latency     | < 800 cycles |

### Methodology

- Compiler: `arm-none-eabi-gcc 12.x` with `-Os` or `-O2`, `-flto`, `-ffunction-sections -fdata-sections`, `-Wl,--gc-sections`.
- `.text`/`.rodata`/`.bss` measurement: `arm-none-eabi-size --format=sysv` on the linked test binary minus baseline empty firmware.
- Stack peak: `puncover` or runtime stack painting; measured per public function.
- Cycles: `DWT->CYCCNT` on Cortex-M, averaged over 1000 invocations with random valid inputs.

---

## 24. Versioning and Roadmap

### Versioning policy

Semantic Versioning 2.0.0:

- **Major** — breaking API or ABI changes.
- **Minor** — new features, backward compatible.
- **Patch** — bug fixes only.

Pre-1.0 (`0.x.y`) versions may break API in minor bumps; will be clearly noted.

### Roadmap

#### V0.1 — Tiny Core (frozen scope)

- 11 API functions: `init`, `deinit`, `set_resource`, `register_op`, `op_set_need`, `check`, `enter`, `leave`, `set_pressure`, `get_pressure`, `snapshot`
- Decision engine with `ALLOW_FULL/DEGRADED/WAIT/REJECT/LOCKDOWN`
- Watermark tracking
- Fail-closed HAL with weak defaults + callback fallback
- Single core `.c` file
- Unit tests (8 mandatory cases)
- One example: `01_bare_metal_minimal` (< 50 lines)
- CI: GCC + Clang + MSVC, footprint check, banned-symbols check
- **No audit, no rate, no calibration, no causality, no adapters, no yield_check.**

Done criteria:
- All unit tests pass on GCC/Clang/MSVC.
- Cross-compiles for arm-none-eabi-gcc Cortex-M0.
- `.text < 4 KiB` on Cortex-M0+ -Os.
- Library `.bss = 0`.
- Minimal example < 50 lines.
- Banned-symbols check passes.

#### V0.2 — Audit + Diagnostics

- `loxbudget_audit_get_recent`, `loxbudget_audit_clear`
- Decision record ring buffer
- Optional diagnostic strings
- microlog adapter
- Two more examples: `02_freertos_mqtt_storm`, `03_esp_idf_flash_budget`

#### V0.3 — Time-Windowed Budgets

- Rate limits (second/minute/hour/day)
- Lifetime limits
- Burn rate query
- Fixed-window counter (default) and Q16.16 token bucket (opt-in)
- Integration test: `flash_burnout`

#### V1.0 — Self-Calibration + Production-Ready

- Calibration begin/sample/end
- P² percentile estimator (integer-only)
- Suggested profile output
- Host-side calibration report tool
- All adapters: microhealth, microconf, microbus, nvlog, loxguard
- Full test suite, full CI gates
- Documentation: getting-started, porting guide, calibration guide
- Stable API commitment
- Single-header amalgamation (auto-generated)

#### V1.1 — Causality Tracking

- `op_may_trigger` graph
- Transitive budget check
- Q8 fixed-point cascade weighting
- Cycle detection
- Configurable depth and weighting
- Scenario replay tool

#### V1.x — Additional features (deferred)

- `yield_check` cooperative checkpoint
- Pressure forecasting from burn-rate trends
- Borrow/lend between modules
- Quorum decisions for mesh devices
- Energy budgets

---

## 25. Use Cases and Demos

### Demo 1: MQTT storm

**Setup**: device publishes telemetry every second. Network drops for 10 minutes.

**Without loxbudget**: outbox saturates, queue grows, RAM exhaustion, eventually watchdog reset; critical fault event lost.

**With loxbudget**:
- `MQTT_PUBLISH_TELEMETRY` profile maps `ELEVATED → DEGRADED`, `CRITICAL → REJECT`.
- `MQTT_PUBLISH_FAULT` profile maps all states → `ALLOW_FULL`.
- When outbox hits 75%, pressure escalates to `ELEVATED`; telemetry switches to compact CBOR.
- When outbox hits 95%, telemetry rejected; fault events still go through.
- Network returns; outbox drains; pressure drops; full telemetry resumes.

### Demo 2: OTA during instability

**Setup**: device reboots into recovery after suspected crash loop.

**Without loxbudget**: scheduled OTA fires regardless of state; partial flash leads to brick.

**With loxbudget**:
- `OTA_UPDATE` profile has `OPF_REQUIRES_BOOT_PROVEN`.
- HAL exposes `loxbudget_hal_boot_proven()` driven by microboot.
- Until two clean boots have passed, OTA is rejected with reason `PRECONDITION_FAIL` / `BOOT_NOT_PROVEN`.
- Audit trail records the rejection.

### Demo 3: Flash burnout / log storm

**Setup**: misconfigured logging fires `info` level at 100 Hz.

**Without loxbudget**: 360 000 flash writes per hour; flash worn out in days.

**With loxbudget**:
- `LOX_RES_FLASH_WRITES` rate-limited to 60/min, 1000/hour, 100 000/lifetime.
- `DEBUG_LOG` and `INFO_LOG` profiles share this resource; `CRITICAL_LOG` has `OPF_BYPASS_RATE_LIMIT`.
- After 60 logs in the first minute, debug/info logs are rejected; critical events still write.
- Burn-rate API exposes the trend.

### Demo 4: Calibration workflow

**Setup**: developer wants to set realistic limits on a new MQTT publish path.

**Workflow**:
1. Add `loxbudget_calibrate_begin(LOX_OP_MQTT_PUBLISH, 1000)` at boot.
2. Instrument `mqtt_publish` to record samples.
3. Run firmware on real workload for 10 minutes.
4. `loxbudget_calibrate_end` produces suggested limits.
5. Developer copies suggested values into the static op profile.
6. Calibration code is removed from production build (`#if LOXBUDGET_ENABLE_CALIBRATION = 0`).

### Demo 5: Host simulation

**Setup**: CI must verify the firmware survives a synthetic load.

**Workflow**:
- Test compiles loxbudget for host using callback HAL.
- `scenario_replay.py` emits a sequence of operation requests and pressure changes.
- Test asserts no critical operation was denied, no resource went negative, no UB occurred.

---

## 26. Coding Conventions

### Language

- **C99** strict. No GCC extensions in headers (allowed in `.c` behind `#ifdef`).
- No VLAs. No `_Generic` in public API.
- `_Static_assert` allowed and encouraged.
- **No floats anywhere in core or any optional feature.**

### Naming

- Public API: `loxbudget_*` prefix, `snake_case`.
- Public types: `loxbudget_*_t` suffix.
- Public macros: `LOXBUDGET_*` prefix, `UPPER_SNAKE_CASE`.
- Internal symbols: `lb__*` prefix, `static`.
- Enum values: prefixed with the enum name (`LOXBUDGET_ALLOW_FULL`, not just `ALLOW_FULL`).

### Formatting

- 4-space indentation. No tabs.
- Brace on same line for control flow, next line for functions.
- Max line length: 100 characters.
- `clang-format` config in repo root.

### Headers

- Every public header has `#pragma once` and traditional include guard.
- Headers must compile standalone (no required prior `#include`).
- Forward declarations preferred over includes when possible.

### Banned constructs

- `malloc`, `calloc`, `realloc`, `free`
- `printf`, `fprintf`, `sprintf`, `puts`
- `assert.h`'s `assert` (use `LOXBUDGET_ASSERT` macro that may be redefined)
- `setjmp`, `longjmp`
- `stdio.h` in core
- `math.h` in core
- `stdbool.h` in core
- All floating-point types and operations
- C++ keywords as identifiers (forward-compat with C++)

### Macros

- Public macros are documented and minimal.
- Function-like macros wrap arguments in parentheses and use `do { ... } while (0)` for statements.
- No macro magic that hides control flow.

### Comments

- Every public function has a Doxygen-style block comment.
- Internal functions: brief comment if non-obvious.
- No commented-out code.
- No `TODO` without an issue number.

### Mandatory `_Static_assert` checks

```c
_Static_assert(sizeof(loxbudget_lease_t)           == 8,  "lease size");
_Static_assert(sizeof(loxbudget_op_profile_t)      == 8,  "profile size");
_Static_assert(sizeof(loxbudget_resource_t)        == 12, "resource size");
_Static_assert(sizeof(loxbudget_decision_record_t) == 16, "audit record size");
_Static_assert(sizeof(loxbudget_need_t)            == 4,  "need size");
```

These prevent silent struct-size growth that would invalidate `LOXBUDGET_REQUIRED_SIZE`.

---

## 27. Error Handling

### Return codes

All public functions return `loxbudget_status_t`. `LOXBUDGET_OK` is zero; errors are negative.

### Output parameters

Functions that produce data write to caller-provided pointers. The pointers must be non-NULL; the library checks and returns `LOXBUDGET_ERR_INVALID_ARG` on NULL.

### No silent failures

If an invalid argument is passed (NULL pointer, out-of-range ID), the function returns an error code. It does **not** silently no-op.

### Assertions

`LOXBUDGET_ASSERT(cond)` is used for invariants that should never fail. Default expansion is empty (production builds). User may redefine to call `assert()` or trap.

```c
#ifndef LOXBUDGET_ASSERT
  #define LOXBUDGET_ASSERT(c) ((void)0)
#endif
```

### No exceptions

The library does not use `setjmp/longjmp`, does not call `abort`, does not call `exit`. The worst it can do is return an error code.

### Lease validity

Leases carry a per-instance `magic` field set on `enter` and validated on `leave`. A double-leave or a leave on a stale lease is detected and returns `LOXBUDGET_ERR_BAD_STATE` without corrupting state.

---

## 28. Thread Safety

### Default: not thread-safe

The library does not use locks internally. On a single-threaded bare-metal firmware this is the right default — no overhead.

### Optional: critical-section HAL

When the application calls into loxbudget from multiple contexts (e.g. ISR + main loop, or RTOS tasks), it should override:

```c
void loxbudget_hal_critical_enter(void);
void loxbudget_hal_critical_exit(void);
```

These are called around all mutating operations. Implementations:

- Bare-metal with ISR access: disable global interrupts.
- FreeRTOS task-only access: `taskENTER_CRITICAL`.
- Zephyr: `irq_lock`/`irq_unlock` or `k_mutex_lock`.
- POSIX host tests: `pthread_mutex_lock`.

### Reentrancy

Public functions are **not** reentrant. Calling `loxbudget_check` from inside a critical section guarded by another `loxbudget_check` is undefined behavior. The HAL critical section must be brief and non-recursive.

### Multi-instance independence

Different `loxbudget_t` instances share no global state and may be used concurrently from different contexts without coordination. The per-instance lease magic guarantees that leases from one instance cannot be replayed against another.

---

## 29. Documentation Requirements

### README.md (must be excellent)

Top-of-readme requirements:

- One-sentence description.
- One-paragraph elaboration.
- Three-line "what it is / what it isn't" block.
- Quick-start code snippet that compiles.
- Footprint table (with separated `.text`/`.bss`/per-instance/stack).
- License badge.

### Getting started guide

Must walk a complete user from `git clone` to a working `01_bare_metal_minimal` example in under 10 minutes.

### Porting guide (V1.0+)

Per-platform sections:

- Bare-metal (no RTOS)
- FreeRTOS
- Zephyr
- ESP-IDF
- NuttX
- Linux/macOS host (for testing)

Each section: HAL setup, critical-section setup, time source, example.

### Calibration guide (V1.0+)

Must explain:

- When to calibrate.
- How to instrument operations.
- What sample size is enough.
- How to interpret outliers.
- How to remove calibration code from production.

### API reference

Auto-generated from Doxygen comments. Hosted on the project site.

### Design rationale

Long-form document explaining:

- Why operation-level (not task-level) contracts.
- Why no allocation.
- Why X-macros for static config.
- Why pressure is set externally, not auto-detected.
- Why fail-closed HAL.
- Why no floats anywhere.
- Why causality is V1.1, not V1.0.

### FAQ

- Is this safety-certified? *No.*
- Can I use it with FreeRTOS? *Yes, see porting guide.*
- Does it replace my watchdog? *No, it complements it.*
- How does it differ from AUTOSAR ResourceManager? *Smaller scope, no certification, drop-in.*
- How does it differ from Linux cgroups? *Cgroups are kernel-level, this is user-level for MCUs.*
- Can two instances share resources? *No, by design. Each instance is self-contained.*

---

## 30. Out of Scope

Explicitly **not** part of loxbudget, now or in the future:

- Task scheduling
- Memory allocation
- Filesystem access
- Network protocol implementation
- Cryptographic primitives
- Configuration parsing (JSON/YAML/TOML)
- Logging infrastructure
- Persistent event storage
- Real-time clock management
- Power management
- Inter-process communication
- Machine learning anomaly detection
- Formal verification proofs
- Safety certification artifacts
- Replacement for AUTOSAR, ARINC 653, or Integrity OS
- Replacement for FreeRTOS/Zephyr/RT-Thread
- Replacement for cgroups, systemd-oomd, Android LMKD
- GUI dashboards
- Remote management

---

## 31. Hard Project Rules

These rules are **enforced in CI** and apply to every commit:

1. **Core must not call** `malloc`, `free`, `calloc`, `realloc`.
2. **Core must not call** `printf`, `fprintf`, `sprintf`, `fopen`, `puts`, or any `<stdio.h>` function.
3. **Core must not have** any global mutable instance. Library `.bss = 0`.
4. **Core must not use** floating-point types or operations anywhere.
5. **Core must not include** `<stdbool.h>`, `<math.h>`, `<stdio.h>`.
6. **Disabled feature must not** leave any symbol in the binary.
7. **TINY profile must have** `audit_size = 0` (audit code does not compile in).
8. **Minimal example must be** ≤ 50 lines.
9. **Single-header (V1.0+) must** compile standalone in a fresh translation unit with `-Wall -Wextra -Wpedantic`.
10. **V0.1 must have** at most one core `.c` file.
11. **All public structs must have** `_Static_assert` on size.
12. **All optional features must** be guarded by `#if FEATURE` (not `#ifdef`).
13. **Production builds must** warn if `loxbudget_hal_default_permissive` is referenced.
14. **`hal_strict = 1`** is the default. `hal_strict = 0` requires explicit, conscious opt-in.
15. **Determinism invariant**: `check(op)` called twice with no other intervening calls must produce identical results.

---

## Appendix A: Comparison with related systems

| System                | Scope            | Cert.   | Heap | Drop-in | Per-op contracts |
|-----------------------|------------------|---------|------|---------|------------------|
| AUTOSAR ResourceMgr   | automotive ECUs  | yes     | no   | no      | yes              |
| ARINC 653             | avionics         | yes     | no   | no      | partition-level  |
| Linux cgroups + PSI   | servers/Android  | n/a     | yes  | no      | task/cgroup      |
| Android LMKD          | mobile OS        | n/a     | yes  | no      | process-level    |
| FreeRTOS heap_5       | RTOS apps        | varies  | yes  | partial | no               |
| Zephyr k_mem_slab     | RTOS apps        | varies  | yes  | partial | no               |
| **loxbudget**         | embedded firmware| no      | no   | **yes** | **yes**          |

loxbudget's niche: small MCU production firmware where AUTOSAR is overkill, Linux solutions don't apply, and RTOS primitives don't address operation-level contracts.

---

## Appendix B: Final positioning

> **loxbudget** is a tiny no-heap C99 pre-flight check layer for embedded firmware. It lets developers define lightweight operation profiles — RAM, queue slots, flash-write budget, time budget, and custom resources — and returns a deterministic decision before the operation starts: run normally, run degraded, wait, reject, or enter survival mode.

> Drop one header into your project. No build system changes. No RTOS required. Pay only for what you use.

---

## Appendix C — Changelog from v1

This appendix documents corrections applied between v1 and v2 of this specification, in response to technical review.

### C-001: `bool` replaced with `loxbudget_bool_t`

**Issue**: v1 HAL used `bool` while claiming dependency only on `<stdint.h>`, `<stddef.h>`, `<string.h>`.
**Fix**: introduced `typedef uint8_t loxbudget_bool_t` with `LOXBUDGET_TRUE`/`LOXBUDGET_FALSE` macros. `<stdbool.h>` is now banned.
**Affected sections**: §6, §12, §26.

### C-002: Causality weights are Q8 fixed-point

**Issue**: v1 described `MAYBE` as "scaled by 0.5" — implies float.
**Fix**: trigger kinds are Q8 fixed-point integers (`ALWAYS=255`, `MAYBE=128`, `RARE=32`, `NEVER=0`). Multiplication uses round-to-nearest: `(need * weight + 128) >> 8`.
**Affected sections**: §17.

### C-003: Dropped `p99.9` references

**Issue**: v1 estimator tracked p50/p95/p99 but the suggested-limit formula used `p99.9 × margin`.
**Fix**: suggested limit is `max(p99 + abs_margin, observed_max + pct_margin)`. P² estimator does not track p99.9 (impractical sample requirements).
**Affected sections**: §6, §14.

### C-004: `LOXBUDGET_REQUIRED_SIZE` is a macro

**Issue**: v1 declared it as a function, but described it as compile-time.
**Fix**: now an explicit macro returning a compile-time constant expression.
**Affected sections**: §13.

### C-005: Token bucket is Q16.16, no floats

**Issue**: v1 described refill as `limit / window_duration` — implies float.
**Fix**: refill rate stored as Q16.16 (`refill_per_ms_q16`), precomputed at config time. All arithmetic is integer. `LOXBUDGET_WINDOW_LIFETIME` uses a simpler monotonic counter (no refill).
**Affected sections**: §15.

### C-006: Fail-closed HAL

**Issue**: v1 default `boot_proven = true` was fail-open; users who forgot to wire HAL would get OTA approved silently.
**Fix**:
- New error: `LOXBUDGET_ERR_HAL_NOT_CONFIGURED`.
- New config field: `hal_strict` (default `1`).
- In strict mode, registering an op with `OPF_REQUIRES_BOOT_PROVEN` (etc.) without configured HAL fails at registration.
- Permissive HAL must be opted in via explicit `loxbudget_hal_default_permissive()` accessor.
**Affected sections**: §6, §12, §31.

### C-007: Footprint splits `.text` / `.rodata` / `.bss` / per-instance / stack

**Issue**: v1 footprint table mixed library RAM with user storage.
**Fix**: footprint table now has four explicit categories. Library `.bss = 0` is now a hard requirement.
**Affected sections**: §23, §31.

### C-008: Library `.bss = 0` is mandatory

**Issue**: v1 implied no global state but did not enforce it.
**Fix**: added CI gate that fails if any profile produces non-zero library `.bss`. Stated as Hard Project Rule #3.
**Affected sections**: §11, §22, §31.

### C-009: Audit sizing recommendations per MCU class

**Issue**: v1 said 64 records "fits comfortably even on small MCUs" — overstated for ATmega class.
**Fix**: added explicit table mapping target classes to recommended `audit_size`.
**Affected sections**: §16.

### C-010: Lease struct reordered for 8-byte no-padding layout

**Issue**: v1 lease struct (id u8, op u8, time u32, magic u16) had ARM padding to 24 B.
**Fix**: reordered to (time u32, magic u16, id u8, op u8) for natural 8 B layout. `_Static_assert` enforces it.
**Affected sections**: §6.

### C-011: Per-instance lease magic

**Issue**: v1 magic was constant; theoretically a stale lease from one instance could be replayed against another.
**Fix**: lease magic is `instance_base ^ slot_index`, where `instance_base` is set on init from instance address and timestamp.
**Affected sections**: §13.

### C-012: `yield_check` deferred to V0.3+

**Issue**: v1 included `yield_check` in V0.1 API but its semantics under pressure were undefined.
**Fix**: removed from V0.1 API. Will be specified in V0.3+ once rate windows and pressure dynamics are proven.
**Affected sections**: §5, §24.

### C-013: `hal_strict = 0` requires conscious opt-in

**Issue**: v1 had no mechanism preventing accidental permissive HAL.
**Fix**: `hal_strict` defaults to `1`; setting to `0` is a documented opt-in. Permissive HAL accessor is explicitly named so CI can detect it in production builds.
**Affected sections**: §12, §31.

### C-014: Hard Project Rules section added

**Issue**: v1 conventions were spread across multiple sections; review noted they should be enforceable rules.
**Fix**: added §31 listing 15 hard rules, all CI-enforced.
**Affected sections**: §31.

### C-015: V0.1 scope locked to single core `.c` file

**Issue**: v1 V0.1 roadmap showed many source files.
**Fix**: V0.1 must have exactly one core `.c` file (~600 LOC target) plus `loxbudget_hal.c`. Splitting is V0.2+ work.
**Affected sections**: §19, §24, §31.

### C-016: Banned-symbols list expanded with float intrinsics

**Issue**: v1 banned-symbols check did not catch hidden float pull-ins (e.g. `__floatsidf`).
**Fix**: CI script also rejects soft-float intrinsic symbols.
**Affected sections**: §22.

### C-017: `LOCKDOWN` audit guarantee

**Issue**: v1 did not specify behavior of `OPF_PERSIST_AUDIT` operations under LOCKDOWN.
**Fix**: added `OPF_LOCKDOWN_PASS` flag. Operations with this flag may proceed in LOCKDOWN if their `action_lockdown` is not REJECT, ensuring `PANIC_DUMP` always records its evidence.
**Affected sections**: §6, §7, §8.

---

*End of specification.*
