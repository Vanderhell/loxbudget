# Contributing

## Build & test

- Host build + tests: `make test`
- Build examples: `make examples`
- CMake build: `cmake -S . -B build && cmake --build build`
- CMake test run: `ctest --test-dir build --output-on-failure`
- Local install sanity check: `cmake --install build --prefix ./dist`

## Style

- Formatting is enforced in CI via `clang-format`.
- Prefer small, focused changes and keep the public API stable.
