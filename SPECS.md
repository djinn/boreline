Boreline: Product and Engineering Specification
Version: 0.1 (initial specification)
Status: Proposed
Language: ISO C23
Platforms: Linux server; Linux and macOS clients
License: MIT (project code); audit third-party licenses
1. Mission
Boreline is a self-hosted, high-performance, secure reverse tunnel. A developer exposes a local HTTP or TCP service through a Linux edge server using an outbound, persistent, authenticated TLS connection. It should be simple to operate, inexpensive at idle, bounded under load, and inspectable without a cloud account.

Non-goals for v1: global edge network, arbitrary UDP, browser dashboard, enterprise SSO, traffic recording by default, P2P NAT traversal, Kubernetes control plane, HTTP/3.
2. User experience
boreline server --config /etc/boreline/server.toml
boreline http 3000 --name demo
boreline tcp 5432 --name postgres
boreline status
boreline version
Expected HTTP result: https://demo.tunnel.example.com forwards to 127.0.0.1:3000 while the client is connected. TCP requires an explicitly assigned public listener or a server policy allowing allocation; never allocate unrestricted public ports by default.

Default behaviour: secure, fail closed, no anonymous public tunnels, local origin bound to loopback, useful errors, structured logs without secrets, graceful shutdown, reconnection with exponential backoff and jitter.
3. Architecture
3.1 Server (Linux only)
One event-driven server process accepts public HTTPS traffic and persistent outbound client tunnel connections on separate listeners. It terminates TLS at the edge, resolves HTTP Host/SNI to an authenticated tunnel registration, opens a logical stream over the associated client session, and forwards HTTP bytes to the local origin. TCP mode forwards opaque bytes after public listener selection. The server owns registration, authorization, stream IDs, backpressure, idle cleanup, rate limiting and metrics.

For v1, support HTTP/1.1 requests, keep-alive and WebSocket upgrades. HTTP/2 public ingress is a later milestone, not an implicit promise. Public TLS termination is mandatory for HTTP mode. Require a valid wildcard certificate (or configured certificate files) and wildcard DNS; automated ACME may be a later milestone.
3.2 Client (Linux and macOS)
One outbound TLS 1.3 connection to the server. After mutual protocol negotiation and token authentication, the client registers one or more local services. For each OPEN frame, it connects to the configured loopback origin and relays data bidirectionally. It never listens publicly. It reconnects and re-registers after disconnects. An origin allowlist prevents arbitrary destination selection by the server.
3.3 Shared core
Portable C23 protocol parser, bounded ring buffers, stream state machine, TLS adapter, timer abstraction, logging, configuration parsing, and deterministic unit tests. Linux uses epoll; macOS uses kqueue. A small poll-based backend may be used in tests. Never depend on Linux-only headers in shared/client macOS code.
3.4 Transport
TCP + TLS 1.3 (OpenSSL 3) for v1. Do not invent cryptography. Implement a small multiplexed protocol with per-stream flow control and strict memory budgets. ALPN identifier: boreline/1 (subject to TLS ALPN validation). Separate client-control and public-ingress listener ports.
4. Protocol v1
All integers use network byte order. Frame header (16 bytes): magic u16, version u8, type u8, flags u16, reserved u16, stream_id u32, payload_length u32. Reject wrong magic/version, nonzero reserved fields, unsupported flags, invalid stream transitions and oversize frames before allocation. Maximum payload 64 KiB; default DATA frame target 16 KiB. Stream 0 is connection control only.

Types: HELLO, AUTH, AUTH_RESULT, REGISTER, REGISTER_RESULT, OPEN, OPEN_RESULT, DATA, WINDOW_UPDATE, HALF_CLOSE, RESET, PING, PONG, ERROR, GOAWAY. Define payload schemas and error codes in docs/protocol.md before implementation. Control frames have tighter limits (4 KiB unless a documented exception). Stream IDs are unique per session, monotonically assigned, and not reused. Reject integer overflows and duplicate IDs.

Per-stream receive window starts at 256 KiB, configurable within hard limits. Connection-wide window and per-session memory cap are also mandatory. A sender cannot transmit DATA beyond granted credit. WINDOW_UPDATE must be validated against overflow and maximum credit. Backpressure must stop socket reads rather than buffering unbounded data. HALF_CLOSE preserves TCP half-close semantics; RESET tears down a stream. GOAWAY drains existing streams.

Authentication: server-generated high-entropy scoped tokens stored hashed at rest (if persisted), compared in constant time, sent only over authenticated TLS. Server certificate verification is mandatory; insecure bypass only in explicit test builds. Replay protection: use fresh server challenge/nonces and bind authentication to session negotiation; document exact handshake and threat model before coding. Rotate/revoke tokens; never log them.
5. HTTP ingress behaviour
Hostnames are registered only within configured wildcard zones. Normalize and validate Host and SNI, reject ambiguity and host-header smuggling. Reject conflicting Content-Length/Transfer-Encoding and malformed requests. Apply header and request-line limits, request timeouts, and max concurrent streams. Preserve streaming request/response bodies; do not buffer whole bodies. WebSocket upgrades become bidirectional byte streams after a valid handshake. Supply Forwarded/X-Forwarded-* only according to trusted proxy policy, stripping untrusted incoming variants. Do not expose internal admin endpoints on the public ingress listener.

HTTP/1.1 parsing must use a vetted parser or a rigorously fuzzed implementation; do not use ad hoc strstr parsing. Correctly handle chunked encoding, keep-alive, pipelining policy (explicitly reject or serialize safely), early disconnects and half-closes.
6. Configuration and CLI
Use a documented TOML subset or an audited dependency; config schema is versioned. Configuration precedence: CLI > environment > file > defaults. Never print token values. Provide --check-config, --json status output and stable exit codes. Example config files live in examples/.

Server configuration: public_bind, tunnel_bind, domain, tls_certificate, tls_key, token store, allowed_public_tcp_ports, max_sessions, max_streams_per_session, max_memory_per_session, timeouts, log level, metrics_bind. Default metrics/admin binds to loopback only. Linux systemd unit with sandboxing and least privilege; privilege separation or capabilities for ports below 1024.

Client configuration: server address, certificate trust, token source (file/env/OS secret store where feasible), named services, local origin, reconnect policy, max local connections. Require owner-only permissions for secret-bearing files.
7. Security invariants
1. No plaintext tunnel transport in production.
2. No disabled certificate verification in release builds.
3. No unbounded allocations from network-controlled lengths.
4. No arbitrary client origin connections; configured loopback allowlist by default.
5. No anonymous registration or arbitrary hostname claims.
6. No token, private key or request body logging by default.
7. No public administrative or metrics listener by default.
8. All sockets nonblocking; all writes handle partial writes and EAGAIN/EINTR.
9. All stream transitions validated; every resource has a single owner and deterministic cleanup.
10. Public TCP exposure is opt-in and constrained by server policy.
Threat model: hostile internet ingress, malicious/compromised client, stolen token, slowloris, memory exhaustion, request smuggling, malformed frames, connection churn, replay, DNS misconfiguration, and client-to-client isolation failures. Document residual risks.
8. Performance and resource budgets
Targets to test, not claims: 10,000 idle authenticated sessions on a suitably provisioned Linux host; 1,000 active forwarded streams; idle session memory under 32 KiB excluding library/global overhead (measure actual RSS); p99 added latency under 10 ms on a same-region low-latency network under stated load; no unbounded growth during 24-hour soak; reconnect and re-register in under 5 seconds under healthy network conditions. Define hardware, kernel, TLS mode, workload and baseline for each benchmark. Publish throughput, p50/p95/p99 latency, CPU, RSS, file descriptors, error rate and drops. Avoid zero-copy claims until verified with TLS enabled.
9. Implementation plan and acceptance gates
M0: Repository and reproducible build
CMake + C23, strict warnings, sanitizers, clang-format, clang-tidy where available, CI on Ubuntu and macOS for client/shared targets, Linux server target, LICENSE, README, SECURITY.md, CONTRIBUTING.md, threat model, protocol document. cmake --build and ctest pass. Explicitly verify compiler support for required C23 features; avoid immature features if they break portability.
M1: Protocol and event loop
Incremental parser/encoder with fuzz targets, stream state machine, Linux epoll and macOS kqueue adapters, socket/TLS I/O abstractions, bounded buffers, timers. Test fragmented/coalesced frames, malformed lengths, integer overflow, EAGAIN, half-close and cancellation. ASan/UBSan clean.
M2: Single-service HTTP tunnel
TLS connection, authenticated client, registration, edge hostname routing, local HTTP/1.1 origin, streaming bodies, WebSocket upgrade. End-to-end test with loopback public ingress and local origin on Linux; client smoke test on macOS. Document manual DNS/certificate setup.
M3: Reliability and multi-service
Multiple services and streams per client; fair scheduling, per-stream and connection flow control, heartbeat, backoff/jitter, disconnect recovery, draining shutdown, token rotation/revocation, quota enforcement, structured metrics. Soak and failure-injection tests.
M4: Hardened beta
Fuzzing corpus, adversarial tests, load/soak results, Linux packaging/systemd, macOS client packaging, reproducible release artifacts, SBOM, dependency/license audit, security review and release checklist. Publish limitations and benchmark methodology. Tag v0.1.0-beta only after gates pass.
M5: Optional later features
HTTP/2 ingress, ACME certificate automation, QUIC transport, request inspection with explicit opt-in, admin API, dynamic multi-node coordination, Windows client, UDP.
10. Testing matrix
- Ubuntu LTS x86_64/aarch64: server + client, unit/integration/fuzz/load.
- macOS arm64/x86_64 (as available): client + shared library, unit/integration.
- GCC and Clang versions with verified C23 support; pin minimum supported versions in README.
- TLS valid/expired/untrusted/wrong-host cases; malformed/partial/oversized frames; slow consumers; forced disconnects; token revocation; hostname conflicts; 10k idle sessions on appropriately tuned hardware.
- CI must never need real public DNS or production credentials; use loopback and test certificates.
11. Project layout
boreline/
  AGENTS.md SPECS.md README.md LICENSE SECURITY.md CONTRIBUTING.md
  CMakeLists.txt cmake/ .github/workflows/
  include/boreline/ src/common/ src/server/ src/client/ src/platform/linux/ src/platform/macos/
  tests/unit/ tests/integration/ tests/fuzz/ benchmarks/ examples/ docs/protocol.md docs/threat-model.md
12. Definition of done
A fresh contributor can build and test on supported platforms, start a Linux server with TLS and token authentication, run a Linux or macOS client, publish a local HTTP service through a wildcard hostname, observe bounded behaviour under backpressure, recover from a connection drop, and reproduce published benchmarks. CI is green; security invariants are covered by tests; limitations are documented. No feature is called complete merely because it compiles.