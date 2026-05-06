# AGENT — Start Here

> **Audience**: any agent (human or AI) tasked with implementing or extending loxbudget.
> **Purpose**: tell you what to read, in what order, and where to start working.

---

## What loxbudget Is

A tiny no-heap C99 library for embedded firmware. It answers one question:

> *Can this operation safely run right now?*

Read [DESIGN.md](./DESIGN.md) for the full philosophy. Read it before you write any code.

---

## Documents in This Pack

| File                  | Read order | Purpose |
|-----------------------|------------|---------|
| `AGENT_START_HERE.md` | 1          | This file. Navigation. |
| `DESIGN.md`           | 2          | Why the library is the way it is. The non-negotiables. |
| `SPEC.md`             | 3 (skim)   | Complete technical specification. Reference, not tutorial. |
| `V0.1_SCOPE.md`       | 4          | What is in V0.1. What is *not*. Done criteria. |
| `WORKLOG_V0.1.md`     | 5 (use)    | Step-by-step implementation plan with checklists. |
| `WORKLOG_V0.2.md`     | later      | After V0.1 ships. |
| `WORKLOG_V0.3.md`     | later      | After V0.2 ships. |
| `WORKLOG_V1.0.md`     | later      | After V0.3 ships. |
| `WORKLOG_V1.1.md`     | much later | Only if real users demand causality. |

---

## How to Start (V0.1)

1. **Read `DESIGN.md` end to end.** Don't skim. The design rules are not negotiable; understanding them is what makes the rest of the work make sense.

2. **Skim `SPEC.md` table of contents.** Note where things are. You'll come back to specific sections during implementation.

3. **Read `V0.1_SCOPE.md` carefully.** This is your contract. Anything outside this scope is not your job in V0.1.

4. **Open `WORKLOG_V0.1.md` and start at Phase 1, Task P1.1.** Work top to bottom. Check the boxes. Commit per task. Don't skip phases.

That's it. The worklog has everything you need.

---

## When to Stop and Ask

You should escalate (open an issue, ping the maintainer, ask for clarification) if any of the following happen:

- A core promise from `DESIGN.md §2` cannot be kept on your target platform.
- A done criterion in `V0.1_SCOPE.md` is genuinely impossible.
- The SPEC contradicts itself or is silent on a critical detail.
- A test fails in a way that suggests the design itself is wrong.
- You are tempted to add a feature not in the current version's scope.

**Do not silently work around these issues.** Surface them. The frozen scope is a contract; reality breaking the contract must be visible.

---

## Things to Refuse (Even If Asked)

The following requests **must be refused** even if the user asks:

1. *"Can you add a small global instance to make the API simpler?"* — No. See DESIGN §7.
2. *"Just one float for percentage math?"* — No. See DESIGN §6.
3. *"Add malloc, just for the calibration buffer?"* — No. See DESIGN §5.
4. *"Skip the tests for now, add them later?"* — No. Tests are part of the deliverable.
5. *"Lower the footprint target so CI passes?"* — No. Find the cause and fix it.
6. *"Sneak feature X from V0.2 into V0.1 because it's almost done?"* — No. Scope is frozen.

These refusals are why the library is small and adoptable. Politely explain the design rule and offer alternatives.

---

## Quick Reference: V0.1 Done Criteria

You're done with V0.1 when **all** of these are true:

- [ ] All 8 mandatory unit tests pass on GCC, Clang, MSVC.
- [ ] Cross-compiles for `arm-none-eabi-gcc` Cortex-M0 with `-Os`.
- [ ] Library `.text` < 4 KiB on Cortex-M0+ -Os.
- [ ] Library `.bss` = 0 bytes.
- [ ] Banned-symbols check passes.
- [ ] Minimal example builds and is ≤ 50 lines.
- [ ] All public struct sizes asserted via `_Static_assert`.
- [ ] `clang-format` and `clang-tidy` clean.
- [ ] AddressSanitizer + UBSan clean on host tests.
- [ ] CI green for 5 consecutive commits on `main`.
- [ ] All required documents present in repo root.
- [ ] `v0.1.0` tag pushed.

If any box is unchecked, you're not done. No exceptions.

---

## Mental Model

Whenever you're unsure what to do, return to this:

> **The library exists to give the application a clear, fast, deterministic permission slip — or a clear, fast, deterministic refusal.**

If your code does that, you're on track. If it does anything else, ask why.

---

*Now go read DESIGN.md.*
