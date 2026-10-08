# Benchmarking notes for boreline

## Goal

Publish reproducible performance data. Claims are only valid with a
documented benchmark methodology.

## Benchmarks tracked

| Benchmark | Target |
|-----------|--------|
| 10,000 idle authenticated sessions | Memory < 32 KiB per session (excl. global) |
| 1,000 active forwarded streams | Bounded latency & memory |
| p99 added latency | < 10 ms same-region |
| Reconnect + re-register | < 5 s healthy network |
| 24-hour soak | No unbounded growth |

## Hardware reference

- CPU: 8+ physical cores, x86_64
- RAM: 32 GB
- NIC: 10 GbE
- Kernel: Ubuntu LTS linux (latest stable)

## TLS mode

- TLS 1.3, AES-256-GCM or ChaCha20-Poly1305 preferred
- Session resumption enabled
- ALPN: boreline/1

## Workload

- Fixed request size distribution:
  - 50% 1 KiB
  - 30% 16 KiB
  - 20% 64 KiB
- Keep-alive connections
- Client connects from a separate host on the same network

## Metrics collected

- Throughput (requests/sec, bytes/sec)
- p50/p95/p99 latency
- CPU utilization
- RSS
- File descriptor count
- Error rate
- Drops

## Running

```bash
# From a clean build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build

# Load benchmark (implemented in benchmarks/load)
# See benchmarks/README.md for details
```