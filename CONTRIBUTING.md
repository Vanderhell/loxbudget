# Contributing

## Build & test

- Host build + tests: `make test`
- Build examples: `make examples`
- CMake build: `cmake -S . -B build && cmake --build build`

## Style

- Formatting is enforced in CI via `clang-format`.
- Prefer small, focused changes and keep the public API stable.
