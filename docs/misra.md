# MISRA C:2012 notes (non-certified)

`loxbudget` is written to be *MISRA-friendly* (no heap, no floats, explicit bounds, no hidden globals), but it is **not** a formally certified MISRA C:2012 component.

## What we do in CI

- `clang-tidy` (see `.clang-tidy`) as a baseline static analysis gate
- `cppcheck` as an additional static analysis pass (see `.github/workflows/ci.yml`)
- Banned-symbols check (`tools/check_banned_symbols.sh`) to keep "hosty" / nondeterministic APIs out of the core
- Footprint budgets (`tools/footprint_check.sh` + `ci/footprint_budget.yaml`) including **library `.bss = 0`**

## Typical MISRA concerns (and how loxbudget approaches them)

- **Determinism / bounded work**: no unbounded loops over external input; scan bounds are defined by config macros and caller-provided sizes.
- **No dynamic memory**: the library never allocates; the caller supplies `storage[]`.
- **No floating point**: integer-only logic by design.
- **No global mutable state**: library `.bss = 0` is enforced; all state is user-owned.

## Known, intentional non-goals

- Full MISRA rule-by-rule compliance reporting is not produced automatically.
- Toolchain-specific MISRA rule configuration (e.g., proprietary checkers) is not bundled.

If you need a formal MISRA report, treat `loxbudget` as a candidate component and run it through your organization’s MISRA toolchain/config, then pin a commit hash/tag.

