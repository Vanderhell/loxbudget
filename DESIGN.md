# loxbudget — Design Document

> **Audience**: anyone (human or AI agent) implementing or modifying loxbudget.
> **Purpose**: explain *why* the library is designed the way it is. This is the document that prevents well-meaning changes from breaking the project's core promises.
> **Read before**: opening any pull request that affects core code, public API, or build configuration.

---

## Table of Contents

1. [The One Sentence](#1-the-one-sentence)
2. [Core Promises](#2-core-promises)
3. [Why "Pre-Flight Checks"?](#3-why-pre-flight-checks)
4. [Why Operation-Level Contracts](#4-why-operation-level-contracts)
5. [Why No Heap](#5-why-no-heap)
6. [Why No Floats](#6-why-no-floats)
7. [Why No Globals](#7-why-no-globals)
8. [Why Single-Header](#8-why-single-header)
9. [Why Fail-Closed HAL](#9-why-fail-closed-hal)
10. [Why X-Macros for Configuration](#10-why-x-macros-for-configuration)
11. [Why Pressure is External, Not Auto-Detected](#11-why-pressure-is-external-not-auto-detected)
12. [Why Causality is V1.1, Not V1.0](#12-why-causality-is-v11-not-v10)
13. [Why Calibration Comes After Rate Windows](#13-why-calibration-comes-after-rate-windows)
14. [The Boundary Between loxbudget and loxguard](#14-the-boundary-between-loxbudget-and-loxguard)
15. [Decisions We Will Refuse](#15-decisions-we-will-refuse)
16. [Anti-Goals](#16-anti-goals)
17. [How to Evaluate a Proposed Change](#17-how-to-evaluate-a-proposed-change)
18. [Failure Modes We Care About](#18-failure-modes-we-care-about)
19. [The 50-Line Test](#19-the-50-line-test)
20. [Mental Model for Implementers](#20-mental-model-for-implementers)

---

## 1. The One Sentence

> **loxbudget answers one question: *can this operation safely run right now?***

Everything in this library exists to answer that question deterministically, in bounded time, on a small MCU, with no allocation, no surprises, and no hidden global state.

If a proposed change does not directly serve answering that question — or makes the answer slower, less deterministic, or harder to verify — the change is wrong, no matter how clever.

---

## 2. Core Promises

These promises are non-negotiable. They are enforced in CI. Breaking any of them is a release blocker.

1. **No heap.** No `malloc`, `calloc`, `realloc`, `free`. Anywhere. Ever.
2. **No floats.** No `float`, `double`, no soft-float intrinsics, no implicit float promotion.
3. **No globals.** Library `.bss = 0`. Every byte of mutable state lives in user-supplied storage.
4. **No hidden dependencies.** Only `<stdint.h>`, `<stddef.h>`, `<string.h>`. No `<stdbool.h>`, no `<stdio.h>`, no `<math.h>`, no `<assert.h>`.
5. **No surprises.** Every public function has bounded execution time. Every error returns a code. Nothing aborts. Nothing logs by itself. Nothing allocates.
6. **Pay only for what you use.** Disabled features produce zero code and zero data.
7. **Fail-closed.** Missing HAL configuration is an error, never a silent permissive default.
8. **Deterministic decisions.** Same input → same output. Always.

If you find yourself thinking *"just this one exception..."*, the answer is no. These promises are why anyone would adopt loxbudget. Lose one, lose them all.

---

## 3. Why "Pre-Flight Checks"?

The naming matters. Earlier iterations of this project used phrases like "resource contract layer," "admission control," and "survival policy framework." All technically correct. All wrong for adoption.

Embedded engineers don't want to adopt a *framework* or *layer*. They want a *check*. The pre-flight metaphor is precise:

- A pilot doesn't run the engine first to see if there's enough fuel. They check before takeoff.
- A pilot doesn't ask "can I fly?" — they ask "is *this specific flight* safe to start?"
- A pre-flight check has a definitive yes/no/conditional answer. It does not negotiate.

loxbudget is the embedded firmware equivalent. Before an operation runs, the application asks the library: *"is this safe to start?"* The library answers, with one of five well-defined outcomes:

- `ALLOW_FULL` — go.
- `ALLOW_DEGRADED` — go, but in reduced mode.
- `WAIT` — try again later.
- `REJECT` — do not run this now.
- `LOCKDOWN` — system is in survival mode.

That's the entire product. Everything else is implementation detail.

---

## 4. Why Operation-Level Contracts

The crucial design choice in loxbudget is that contracts are attached to **operations**, not to tasks, threads, or modules.

**Why not task-level (RTOS-style)?**
RTOSes already do task-level scheduling. Adding another task-level mechanism would duplicate work and conflict with the scheduler. More importantly, the failures we care about are not "task X used too much CPU" — they are "operation X tried to start when the system couldn't afford it."

**Why not module-level?**
Modules are too coarse. The MQTT module is fine until it tries to publish a 4 KB telemetry blob during low memory. The same module is also fine when publishing a 64-byte fault event. A module-level budget cannot distinguish these.

**Why operation-level?**
An operation is the smallest unit that has a meaningful resource contract. A `MQTT_PUBLISH` is different from a `MQTT_SUBSCRIBE`. A `DEBUG_LOG` is different from a `CRITICAL_LOG`. Each operation has its own pressure-state response policy. This is the right granularity.

This is also why the library has the concept of operation profiles as static, declarable data. The profile *is* the contract.

---

## 5. Why No Heap

Three reasons, in order of importance:

1. **Determinism.** `malloc` can fail, can fragment, can take variable time. None of these are acceptable in a pre-flight check that must produce an answer in bounded time.

2. **Auditability.** When a firmware crashes in the field, the developer needs to know exactly what state the library was in. Heap state is non-reproducible. Static state is.

3. **Trust.** Embedded engineers have learned to distrust libraries that allocate. A library that allocates secretly is a library that will surprise you at 3 AM in production. We want to be the library that does *not* surprise you.

The cost: the user has to declare maximum sizes at compile time. This is a feature, not a bug. It forces them to think about sizing up front.

---

## 6. Why No Floats

This rule sounds extreme. It's not. It's a hard requirement for these reasons:

1. **Many MCUs lack hardware float.** Cortex-M0, M0+, M3, parts of M4 (no FPU variants), AVR, MSP430, RISC-V cores without F extension. On these, float operations pull in tens of kilobytes of soft-float library. A loxbudget that pulled in `__floatsidf` would suddenly cost 10× its claimed footprint.

2. **Determinism.** Float operations have non-deterministic edge cases (denormals, NaNs, rounding modes). Integer math is exact and bit-reproducible.

3. **Verification.** Integer math is auditable. Float math is not. We want to be the library that can be reasoned about.

4. **It's not necessary.** Every quantity in this library can be expressed as integer or fixed-point:
   - Percentages → Q8 (0.125 → 32, 0.5 → 128, 1.0 → 255)
   - Token bucket refill → Q16.16
   - Time → milliseconds as `uint32_t`

When you see something that "needs" a float, the right move is fixed-point arithmetic, not relaxing this rule.

---

## 7. Why No Globals

Every single byte of mutable state in loxbudget lives in **user-owned storage**. The library itself has zero `.bss`.

**Why this matters:**

1. **Multiple instances.** A boot-time budget instance and a runtime budget instance can coexist without interfering. Globals would force a singleton.

2. **`.noinit` persistence.** The user can place their storage in `.noinit` to preserve audit trail across soft resets. With globals, this would be impossible without library cooperation that creates other problems.

3. **Testability.** Tests can create fresh instances without side effects from previous tests. With globals, every test would leak state.

4. **Linker friendliness.** A library with a static instance pays for that instance even when not used. With user-owned storage, users who don't use loxbudget pay nothing.

5. **Trust.** A library with no globals is a library you can audit by reading its public API. Globals create hidden state that must be reasoned about separately.

**Cost:** the user must declare `static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(...)]`. This is the same kind of cost as declaring an array — it's normal C.

---

## 8. Why Single-Header

Embedded developers will not adopt a library that requires CMake gymnastics, build system integration, or 15 source files to copy.

The single-header pattern (popularized by `stb`, `sokol`, `miniaudio`, `dr_libs`, `lua`) has a well-understood property: **the friction to try is near zero.**

```c
#define LOXBUDGET_IMPLEMENTATION
#include "loxbudget.h"
```

Two lines. Done. No `add_subdirectory`, no `CMakeLists.txt`, no `find_package`. If the library doesn't work, the user removes those two lines.

**Cost of single-header:**
- Slightly slower compile when used in many TUs (mitigated by `#define LOXBUDGET_IMPLEMENTATION` in only one).
- Discipline required to avoid macro collisions and namespace pollution.
- Auto-generation script needed once the library is split into multiple internal files (post-V1.0).

**Why the cost is worth it:**
The single biggest predictor of an embedded library's adoption is *how fast can a skeptical engineer try it*. Single-header crushes this metric. Nothing else comes close.

For V0.1, the library *is* a single source file. No amalgamation needed. From V1.0+ we split internally and amalgamate at release time.

---

## 9. Why Fail-Closed HAL

This is the most important safety decision in the library.

**The bad design (rejected):**
```c
// HAL with fail-open default
__attribute__((weak)) bool loxbudget_hal_boot_proven(void) {
    return true;  // default: assume boot is proven
}
```

What's wrong: a developer who registers an operation requiring `BOOT_PROVEN` but forgets to wire up the HAL gets a *silent pass*. Their OTA update runs during a crash loop. Their device bricks. They never see the warning because there is no warning.

**The good design (adopted):**
```c
// HAL with fail-closed default
__attribute__((weak)) loxbudget_bool_t loxbudget_hal_boot_proven(void) {
    return LOXBUDGET_FALSE;  // default: assume nothing is proven
}

// Plus: registering an op with REQUIRES_BOOT_PROVEN
// without configured HAL → LOXBUDGET_ERR_HAL_NOT_CONFIGURED at registration
```

If the developer forgets to wire HAL, **the library refuses to register the operation**. They cannot deploy without seeing the error. The compile and link succeed; the runtime registration fails clearly.

This is the embedded equivalent of "fail safe." Missing configuration must produce a visible error, not a silent permissive behavior. The cost (slightly more setup ceremony) is dwarfed by the benefit (no silent OTA-during-crash-loop disasters).

For host tests and the minimal example, an explicit `loxbudget_hal_default_permissive()` accessor exists. Production code that uses it is detected by CI and warned. Permissive HAL is opt-in by name, never by default.

---

## 10. Why X-Macros for Configuration

X-macros look weird to people unfamiliar with them. They are nonetheless the right tool for embedded static configuration.

**The alternative (runtime registration):**
```c
loxbudget_register_op(&budget, &mqtt_publish_profile);
loxbudget_op_set_need(&budget, LOX_OP_MQTT_PUBLISH, LOX_RES_RAM, 512);
```

This works, but:
- Profiles must live in RAM (or be `const` in flash + copied at registration).
- The `register_op` code is in the binary even for fully static configurations.
- IDs are runtime values; switch statements can't be optimized.

**The X-macro approach:**
```c
// app_ops.def
LOXBUDGET_OP(MQTT_PUBLISH, NORMAL, ALLOW_FULL, ALLOW_DEGRADED, REJECT, REJECT, REJECT, 0)
LOXBUDGET_OP(OTA_UPDATE,   HIGH,   ALLOW_FULL, REJECT,         REJECT, REJECT, REJECT, OPF_REQUIRES_BOOT_PROVEN)
```

This file generates:
1. The `enum` of operation IDs (compile-time constants).
2. The static `const` table of profiles (in `.rodata`, free).
3. The initialization code that registers them.

**Why it's worth the unfamiliarity:**
- IDs are compile-time. `switch (op_id)` becomes a jump table.
- Profiles live in flash, not RAM.
- Adding an operation is a one-line change.
- Removing an operation removes its enum value, causing compile errors at every use site — exactly what you want.

We provide both X-macro and runtime registration. X-macro is the recommended path for production firmware. Runtime is for host tests and tooling.

---

## 11. Why Pressure is External, Not Auto-Detected

Pressure (NORMAL/ELEVATED/CRITICAL/SURVIVAL/LOCKDOWN) is set by the application via `loxbudget_set_pressure`. The library does not auto-detect pressure from internal counters.

**Why not auto-detect?**

1. **The library doesn't know the system.** Watermark at 70% might be CRITICAL on one device and NORMAL on another. Only the application knows.

2. **Hysteresis is hard.** Auto-detection requires hysteresis to avoid oscillation. Hysteresis requires tuning. Tuning belongs in the application, not the library.

3. **Multiple inputs.** Real pressure depends on RAM, CPU, network, battery, temperature, queue depth, time since boot. The library only sees its own resources. The application sees everything.

4. **Composability.** Externally driven pressure means loxbudget composes well with `microhealth`, custom monitoring, or no monitoring at all. Auto-detection would lock in one model.

The library *exposes* what it sees (watermarks, burn rates) so the application can decide. The decision belongs to the application.

---

## 12. Why Causality is V1.1, Not V1.0

Causality tracking — declaring that operation A may trigger operation B, then checking budget for the cascade — is the most technically interesting feature in the roadmap.

It is also the most likely to break in unforeseen ways. Specifically:

- Cascade weighting (Q8 fixed-point) needs real workload data to calibrate.
- Cycle detection edge cases are subtle.
- Depth limits interact with how applications structure operations.
- Wrong implementation could cause false denials worse than no causality at all.

**The decision:** ship V1.0 without causality. Get production users. Learn what cascading actually looks like in real firmware. Then design V1.1 from data, not speculation.

This is also why `OPF_REQUIRES_BOOT_PROVEN` and `STATE` resources exist in V0.1: they cover ~80% of the practical "don't run this if X" cases without any graph machinery. Causality is for the remaining 20%, and we'll only know what those 20% look like once V1.0 is in the field.

---

## 13. Why Calibration Comes After Rate Windows

The roadmap is V0.1 (core) → V0.2 (audit) → V0.3 (rate) → V1.0 (calibration). It might seem like calibration should come earlier — it's the "killer feature."

**Why it doesn't:**

1. **Calibration produces *suggested limits*.** Without rate windows, those limits cover only static quantities (RAM, slots). Dynamic quantities (writes per minute) need rate windows to be meaningful.

2. **Calibration validates that the core works under realistic load.** Without rate windows, you can't simulate realistic load.

3. **Calibration's value scales with what you can constrain.** More features = richer calibration output.

4. **Each prior phase de-risks the next.** V0.1 proves core. V0.2 proves audit (so calibration's output can be logged). V0.3 proves rate windows (so calibration can suggest rate limits). V1.0 ties it together.

Shipping calibration first would mean shipping a feature whose primary output (suggested rate limits) the library cannot actually enforce.

---

## 14. The Boundary Between loxbudget and loxguard

If you also work on loxguard (the sibling library for crash evidence and recovery), the boundary is clean:

| Question                                       | Library     |
|------------------------------------------------|-------------|
| *Can this operation run now?*                  | loxbudget   |
| *Why was this operation denied?*               | loxbudget   |
| *What state was the system in at denial time?* | loxbudget   |
| *Why did the system crash?*                    | loxguard    |
| *What evidence do we have of the crash?*       | loxguard    |
| *How do we recover from the crash?*            | loxguard    |

**One sentence each:**
- **loxbudget**: prevent unsafe operations from starting.
- **loxguard**: document and recover from unsafe states that occurred anyway.

These are complementary, not redundant. A device using both has prevention (loxbudget) and post-mortem (loxguard). A device using only loxbudget has prevention. A device using only loxguard has post-mortem.

**The adapter** (`adapters/loxguard/`) lets loxbudget feed its audit trail into loxguard's blackbox automatically on critical events. This is the only place where the two libraries are aware of each other; otherwise they are independent.

---

## 15. Decisions We Will Refuse

The following requests will be **refused on principle**, not on availability of effort. If a contributor proposes them, the answer is no.

1. **"Just one optional malloc."** No.
2. **"Just one float for percentage math."** No.
3. **"Just one global instance for convenience."** No.
4. **"Just one printf for debugging."** No. Use audit trail.
5. **"Just one header from `<stdio.h>` for snprintf."** No.
6. **"Make pressure auto-detect from watermarks."** No. Application's job.
7. **"Add task scheduling."** No. RTOS's job.
8. **"Add safety certification."** No. Out of scope.
9. **"Replace the audit ring with a linked list."** No. Static is the point.
10. **"Use C11 atomics in core."** No. C99 only.
11. **"Add a JSON config parser to the core."** No. JSON belongs in host tools.
12. **"Add a CLI to the core."** No. CLI belongs in host tools.

These refusals are why the library is small and adoptable. Lose them, lose the project.

---

## 16. Anti-Goals

Stating what loxbudget is *not* is as important as stating what it is.

- **Not an RTOS.** We don't schedule. We don't preempt.
- **Not a memory allocator.** We don't allocate.
- **Not a logger.** We don't print.
- **Not a watchdog.** We don't reset the device.
- **Not a profiler.** We measure only what calibration explicitly samples.
- **Not a safety certification path.** We make no DO-178C / IEC 61508 / ISO 26262 claims.
- **Not a replacement for AUTOSAR ResourceManager.** We're smaller, simpler, uncertified.
- **Not a replacement for cgroups.** Different layer of the stack.
- **Not a "policy engine."** Policy is what the application defines via profiles. We just execute.
- **Not a framework.** A framework calls your code. Your code calls us.

Anyone confused about what loxbudget *is* can read this list to understand what it *isn't*. The latter is often clearer.

---

## 17. How to Evaluate a Proposed Change

When a pull request, feature request, or refactor lands, evaluate it against these questions in order:

1. **Does it serve the One Sentence?** (§1)
   *Does this help answer "can this operation safely run right now?"* If no, reject.

2. **Does it break a Core Promise?** (§2)
   If yes, reject without further discussion.

3. **Does it add a Refused Decision?** (§15)
   If yes, reject without further discussion.

4. **Does it pass the 50-line test?** (§19)
   If the minimal example would grow past 50 lines because of this change, reject or rework.

5. **Does it pay its own way in CI?**
   New feature → new tests, new footprint check, new banned-symbol check if applicable.

6. **Does it break the layering?** (§14)
   Core must not depend on optional features. Optional features must not depend on adapters. Adapters depend on public APIs only.

7. **Is the failure mode it addresses real?**
   Speculation about "what if someone wants..." is not evidence. A bug report or a real production scenario is.

If a change passes all seven, accept. If it fails one, the discussion is over.

---

## 18. Failure Modes We Care About

The library is designed to prevent or document specific failure modes. Knowing these helps prioritize:

### Prevented failures (V0.1+)
- **Operation runs when system can't afford it.** Core decision engine prevents this.
- **Partial resource reservation on failure.** Atomic enter prevents this.
- **Stale lease replayed across instances.** Per-instance lease magic prevents this.
- **OTA-during-crash-loop.** `OPF_REQUIRES_BOOT_PROVEN` + fail-closed HAL prevents this.

### Prevented failures (V0.2+)
- **Post-mortem analysis impossible.** Audit trail enables it.
- **LOCKDOWN audit silently dropped.** `OPF_LOCKDOWN_PASS` flag prevents this.

### Prevented failures (V0.3+)
- **Flash burnout from log storms.** Rate-windowed budgets prevent this.
- **MQTT outbox saturation under network outage.** Rate windows + degraded actions prevent this.
- **Lifetime exhaustion of consumable resources.** Lifetime counter prevents this.

### Prevented failures (V1.0+)
- **Limits set from guesswork.** Calibration suggests realistic limits.

### Prevented failures (V1.1+)
- **Cascading load: each operation is fine, their cascade is fatal.** Causality tracking prevents this.

### Failures we **don't** prevent
- Hardware failures.
- OS-level crashes.
- Stack overflows (loxbudget is data-driven, not stack-recursive).
- Bugs in the application's operation implementation.
- Bugs in the application's pressure logic.
- Adversarial misuse of the API.

The library prevents categories of mistakes. It does not prevent mistakes within those categories from being made by the application.

---

## 19. The 50-Line Test

A hard constraint on the project's complexity:

> **The minimal example (`examples/01_bare_metal_minimal/main.c`) must be at most 50 lines, including comments and includes.**

This is not arbitrary. It is the empirical threshold below which an embedded engineer will read and try the example versus close the tab.

The 50-line example must demonstrate:
- Initialization
- Resource setup
- Operation registration with at least one need
- A `check` or `enter` call
- A reasonable response to each decision outcome

If a change to the library makes the minimal example longer than 50 lines, the change is too complex. Either simplify the change or simplify the API.

This test is the most effective tool against scope creep. Use it.

---

## 20. Mental Model for Implementers

When implementing core functionality, hold this picture in mind:

**A pre-flight check is not a tax. It is a permission slip.**

The library's job is to give the application a clear, fast, deterministic permission slip — or a clear, fast, deterministic refusal. Everything else is in service of this:

- Resources are *the things you might run out of*.
- Operations are *the things you might want permission to do*.
- Pressure is *how cautious to be right now*.
- Decisions are *the permission slip itself*.
- Audit is *the record of permission slips issued*.
- Calibration is *figuring out what permission slips should reasonably cost*.
- Causality is *checking the permission slip covers the trip's downstream effects*.

When you implement, write code that an embedded engineer would read at 2 AM and immediately understand. Avoid clever. Prefer obvious. Prefer one straight-line function over three layered abstractions. Prefer integer math over fixed-point when integer suffices. Prefer fixed-point over float always.

The library that ships is the library that is read. Make it readable.

---

*End of design document.*
