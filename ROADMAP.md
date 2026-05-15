# Roadmap

## Release path

Tags must be created on a clean, committed state with green CI.

### `v1.0.0-rc1`

- First release candidate for external testers.
- Must include: `releases/v1.0.0-rc1.md`
- Must pass: single-header CI, fuzz smoke, coverage, host builds, cross-builds

### `v1.0.0`

- Final release after RC feedback is addressed.
- Must include: `releases/v1.0.0.md`
- Scope is limited to integration feedback, documentation cleanup, and platform validation.
- No feature expansion is planned before `v1.0.0`.

Suggested commands:

```sh
git tag -a v1.0.0-rc1 -m "v1.0.0-rc1"
git tag -a v1.0.0 -m "v1.0.0"
git push origin --tags
```

Notes:

- Pushing a `v*` tag triggers `.github/workflows/release.yml` to create a GitHub Release and attach `single_header/loxbudget.h`.

## After `v1.0.0`

- Decide whether to keep iterating on V1.0 polish and hardware evidence or start V1.1 causality work.
- Run ESP32 Arduino demo soak on real hardware and record stability/performance results.
- Run a longer local fuzz campaign and record outcomes.
- Fill `benchmarks/v1.0_real_hardware.md`.
- Re-check calibration outlier behavior from `SPEC.md` section 14 against captured real workloads.
