# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project follows semantic versioning once `v1.0.0` is released.

## [Unreleased]

### Changed

- Tightened CI and release-handoff documentation for downstream consumers.
- Added CMake install/export support and `ctest` registration for packaged verification.

## [1.0.0-rc1] - 2026-05-15

### Added

- Host-side calibration export and reporting workflow.
- Optional adapters for microlog, microhealth, microconf, microbus, nvlog, and loxguard.
- Generated single-header distribution with CI validation against core and adapter tests.
- Expanded CI coverage across GCC, Clang, sanitizers, fuzz smoke, cross-compilation, coverage, and footprint checks.

### Changed

- Public API marked as intended to be stable for the final `v1.0.0` release.
- Release-candidate documentation aligned around integration feedback and validation before final release.

## [0.3.0]

### Added

- Rate windows and lifetime budget enforcement.
- Integration demos and CI jobs for scenario coverage.

## [0.2.0]

### Added

- Audit-focused examples and documentation updates.

## [0.1.0]

### Added

- Initial core API: `init`, `config`, `check`, `enter`, and `leave`.
