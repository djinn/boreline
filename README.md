# boreline

[![Build](https://github.com/djinn/boreline/actions/workflows/build.yml/badge.svg)](https://github.com/djinn/boreline/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**boreline** is a self-hosted, high-performance, secure reverse tunnel. A developer exposes a local HTTP or TCP service through a Linux edge server using an outbound, persistent, authenticated TLS 1.3 connection.

## Quick start

```bash
# On the server
boreline server --config /etc/boreline/server.toml

# On the client
boreline http 3000 --name demo
boreline tcp 5432 --name postgres

# Status
boreline status
```

## Documentation

- [SPECS.md](SPECS.md) — Full product and engineering specification
- [docs/protocol.md](docs/protocol.md) — Wire protocol specification
- [docs/threat-model.md](docs/threat-model.md) — Security threat model
- [examples/](examples/) — Example configuration files

## Building

### Prerequisites

- C23-capable compiler (GCC 14+ or Clang 17+)
- CMake 3.28+
- OpenSSL 3.0+

### Quick build

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest -V
```

### Build with sanitizers

```bash
cmake .. -DBORELINE_WITH_ASAN=ON -DBORELINE_WITH_UBSAN=ON
cmake --build .
ctest -V
```

## Project layout

```
boreline/
 AGENTS.md SPECS.md README.md LICENSE SECURITY.md CONTRIBUTING.md
 CMakeLists.txt cmake/ .github/workflows/
 include/boreline/         # Public headers
 src/common/               # Portable shared code
 src/server/               # Linux server
 src/client/               # Linux/macOS client
 src/platform/linux/       # epoll backend
 src/platform/macos/       # kqueue backend
 tests/unit/               # Deterministic unit tests
 tests/integration/        # Loopback integration tests
 tests/fuzz/               # Fuzz targets
 benchmarks/               # Performance benchmarks
 examples/                 # Example configurations
 docs/                     # Protocol and threat model docs
```

## Platform support

| Component | Linux | macOS |
|-----------|-------|-------|
| Server    | ✅    | ❌    |
| Client    | ✅    | ✅    |
| Shared    | ✅    | ✅    |

## License

MIT — see [LICENSE](LICENSE). Third-party dependency licenses are audited before each release.