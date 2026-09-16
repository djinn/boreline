# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | ✅        |

## Reporting a Vulnerability

Please report security vulnerabilities by opening a GitHub Security Advisory or
emailing the maintainers. Do not file public issues for security vulnerabilities.

## Security Invariants

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

See [docs/threat-model.md](docs/threat-model.md) for the full threat model.