# TODO (pre-release)

## Tagging plan

Tags must be created on a clean, committed state (CI green, no uncommitted changes).

- `v0.3.0`
  - Tag the last commit you consider the frozen/stable "V0.3 baseline".
  - Sanity: `make test`, `make integration`, CI green.
- `v1.0.0-rc1`
  - Tag the first "release candidate" commit you want external testers to try.
  - Must include: `releases/v1.0.0-rc1.md`, single-header CI passes, fuzz-smoke + coverage jobs pass.
- `v1.0.0`
  - Tag the final commit after RC feedback is addressed.
  - Must include: `releases/v1.0.0.md`, docs finalized, CI green.

Suggested commands:

```sh
git tag -a v1.0.0-rc1 -m "v1.0.0-rc1"
git tag -a v1.0.0 -m "v1.0.0"
git push origin --tags
```

Notes:

- Pushing a `v*` tag triggers `.github/workflows/release.yml` to create a GitHub Release and attach `single_header/loxbudget.h`.

## When we resume work

- Decide whether to keep iterating on V1.0 (polish/bench/real HW) or start V1.1 (causality).
- Run ESP32 Arduino demo soak on real HW and record stability/perf (e.g. `examples/esp32_arduino_loxbudget`, commands `run N` / `soak N`).
- If continuing V1.0:
  - Run a longer fuzz campaign locally (hours/day) and record outcomes.
  - Fill `benchmarks/v1.0_real_hardware.md`.
  - Re-check calibration outlier behavior against `SPEC.md` §14 on real workload captures (outlier rate sanity).
