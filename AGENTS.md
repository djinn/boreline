# Agnet instructions for boreline development

## Repository overview

boreline is a self-hosted reverse tunnel implemented in ISO C23:
- `include/boreline/` — public API headers
- `src/common/` — portable protocol parser, ring buffer, stream state machine,
  config parser, logging, timers
- `src/server/` — Linux tunnel server
- `src/client/` — Linux/macOS tunnel client
- `src/platform/` — epoll (Linux) and kqueue (macOS) event loop adapters
- `tests/` — unit, integration, and fuzz tests
- `docs/` — protocol and threat model documentation

## Build and test

```bash
cmake -B build && cmake --build build && ctest --test-dir build
```

## Conventions

- C23, strict warnings, `-Werror` in CI
- All network-controlled lengths validated before allocation
- All sockets nonblocking; partial writes handled
- Secret-bearing values never logged
- Tests are deterministic; no real DNS or production credentials needed
- ASan/UBSan clean is a requirement for merged code

## Key spec requirements (see SPECS.md)

- Frame header: 16 bytes, magic 0x424C, version 1
- Max payload 64 KiB; control max 4 KiB; default data target 16 KiB
- Per-stream window 256 KiB default
- Stream 0 is connection control only
- Fail closed: no anonymous tunnels, no unrestricted port allocation