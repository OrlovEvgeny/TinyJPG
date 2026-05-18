# Contributing

TinyJPG is being rebuilt as a C++23 project on `feature/cpp-rewrite`.

## Source Layout

- Public headers live under `include/tinyjpg/` and use the `.hh` extension.
- Implementation files live under `src/` and use the `.cc` extension.
- Tests live under `tests/` and should exercise public behavior through the library targets.

## Local Build

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Release builds use the `release` preset:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

## Standards

- C++23 is the baseline language mode.
- Warnings are treated as errors in project presets and CI.
- Format C++ files with the repository `.clang-format`.
- Keep changes small and focused. Commit messages use Conventional Commits.
