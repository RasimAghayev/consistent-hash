# Contributing

Thank you for considering a contribution to ConsistentHash! This document
describes how to report bugs, propose features, and submit pull requests.

## Reporting bugs

Use the bug issue template in `.github/ISSUE_TEMPLATE/bug_report.md`. Include:
the compiler version, C++ standard, operating system, steps to reproduce,
expected vs. actual behavior, and any relevant logs. Never paste secrets or
credentials.

## Proposing features

Use the feature request template. Describe the use case, why it matters, and
any API sketch. Large features need an ADR under `docs/adr/`.

## Submitting a PR

1. Create a branch from `preview`: `feature/ISSUE-<n>-<slug>`.
2. Implement the change, including unit/property tests and documentation.
3. Run `make verify` (or the CI-equivalent in Docker) and ensure it is green.
4. Commit using Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`,
   `docs:`, `build:`, `ci:`, `chore:`). No AI signatures or co-author trailers.
5. Open a PR targeting `test`, linked to the issue (`Fixes #N`).
6. CI must be green and required reviews obtained before merge.

## Development setup

```bash
# Build and run tests in Docker (recommended)
make build
make test

# Or with a local C++ toolchain and CMake 3.20+
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Coding standards

* C++17 (C++20 where beneficial). Clang-format style: 4-space indent.
* One logical change per commit; conventional commit titles.
* No comments beyond cross-file dependency call-site notes.
* Header-first design; keep the public API in `include/consistent_hash/`.

## License

MIT. By contributing you agree your contributions are licensed under MIT.
