# Contributing to boreline

Thank you for your interest in contributing!

## How to contribute

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes with descriptive commit messages
4. Ensure tests pass: `cmake --build . && ctest -V`
5. Run clang-format and clang-tidy if available
6. Open a pull request

## Coding standards

- ISO C23 with minimal extensions
- Strict compiler warnings: `-Wall -Wextra -Wpedantic -Werror`
- All allocations bounded; no unbounded reads/writes
- All file descriptors `O_CLOEXEC` where possible
- All sockets nonblocking
- No bare loops over network data without length limits
- ASan and UBSan clean at all times

## Testing

All changes must maintain or improve test coverage:
- Unit tests in `tests/unit/` for deterministic parser, state machine, and config tests
- Integration tests in `tests/integration/` for loopback e2e pipelines
- Fuzz targets in `tests/fuzz/` for the protocol parser

## Commit messages

Use conventional commits:
- `feat:` new feature
- `fix:` bug fix
- `test:` test changes
- `docs:` documentation
- `refactor:` code restructuring
- `chore:` build, CI, tooling

## Sign off

All contributions must include a Signed-off-by line (`git commit -s`)
indicating agreement with the [Developer Certificate of Origin](https://developercertificate.org/).