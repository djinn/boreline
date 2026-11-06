# Threat Model

## Assets

- Tunnel server private key
- Authentication tokens
- Forwarded application traffic
- Service registration state
- Session and stream state

## Trust Boundaries

```
[Internet] ←→ [boreline server] ←→ [boreline client] ←→ [Local origin]
                                       Authenticated TLS 1.3
```

## Threats

### Hostile internet ingress

An attacker on the public internet connects to the public ingress or tunnel
listener. The server must:
- Terminate TLS at the edge with valid certificates
- Validate all frame headers before allocation
- Reject unauthenticated tunnel connections
- Apply header and request limits on HTTP ingress
- Apply rate limiting and max concurrent connection limits
- Never expose admin or metrics endpoints on public listeners

### Malicious or compromised client

An authenticated client may attempt to:
- Register hostnames it does not own
- Open streams to arbitrary destinations (prevented by origin allowlist)
- Send malformed frames to crash the server
- Exhaust memory by opening many streams (prevented by budgets)
- Replay captured tokens (prevented by session binding and nonces)

### Stolen authentication token

An attacker who gains access to a token file can:
- Connect to the tunnel server as the legitimate client
- Mitigation: tokens are high-entropy, scoped, revocable, sent only over TLS,
  never logged, and can be rotated

### Slowloris / slow read attacks

An attacker holds connections open by sending data very slowly:
- Mitigation: request and idle timeouts, per-stream and per-connection
  timeouts, nonblocking I/O with bounded wait times

### Memory exhaustion

An attacker sends many frames with declared large payloads:
- Mitigation: maximum payload size checked before allocation (64 KiB),
  per-stream receive window (256 KiB default), per-session memory cap,
  backpressure stops socket reads rather than buffering

### Request smuggling / HTTP desync

Attacker crafts ambiguous HTTP requests:
- Mitigation: use vetted HTTP parser, reject conflicting
  Content-Length + Transfer-Encoding, reject malformed header values,
  strip untrusted Forwarded/X-Forwarded-* headers

### Malformed frames

Attacker sends frames with invalid magic, version, reserved fields, or
oversized payloads:
- Mitigation: validate all header fields before any processing or allocation;
  reject wrong magic/version, non-zero reserved, unsupported flags, invalid
  stream transitions, oversized frames

### Connection churn

Attacker rapidly connects and disconnects:
- Mitigation: rate-limit connection acceptance, bounded session table,
  backoff reconnection on client side

### Replay attacks

Captured authentication frames:
- Mitigation: fresh server nonces per session, authentication bound to
  session negotiation

### DNS misconfiguration

Wrong wildcard DNS setup:
- Mitigation: validate Host and SNI against configured wildcard zones,
  clear error messages

### Client-to-client isolation failure

One client accessing another's registered hostname:
- Mitigation: authentication binds registration to session, routing table
  validates ownership, hostname uniqueness enforced at registration

## Residual Risks

- Side-channel attacks on constant-time comparisons (hardware-dependent)
- TLS implementation vulnerabilities in OpenSSL 3
- Timing attacks on the protocol parser (not currently constant-time)
- Physical access to server or client hardware
- Supply-chain attacks on dependencies

## Testing

Each threat above must have corresponding tests. Security invariants are
tested automatically in CI.