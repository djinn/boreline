# boreline Protocol v1

## Transport

TCP + TLS 1.3 (OpenSSL 3). ALPN identifier: `boreline/1`.

## Frame Format

All integers are network byte order (big endian).

### Frame Header (16 bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Magic (16)           | Ver (8) |   Type (8)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Flags (16)          |       Reserved (16)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Stream ID (32)                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Payload Length (32)                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Constants

| Constant | Value |
|----------|-------|
| Magic | `0x424C` |
| Protocol Version | `0x01` |
| Max Payload | 64 KiB |
| Default Data Target | 16 KiB |
| Max Control Payload | 4 KiB |
| Default Stream Window | 256 KiB |
| Min Window | 64 KiB |
| Max Window | 4 MiB |

## Frame Types

| Type | Code | Payload |
|------|------|---------|
| HELLO | 0x01 | Protocol version + capabilities |
| AUTH | 0x02 | Token + session challenge |
| AUTH_RESULT | 0x03 | Status code |
| REGISTER | 0x04 | Service name + type + origin |
| REGISTER_RESULT | 0x05 | Status + assigned hostname |
| OPEN | 0x06 | Destination origin |
| OPEN_RESULT | 0x07 | Status |
| DATA | 0x08 | Application bytes |
| WINDOW_UPDATE | 0x09 | Credit increment |
| HALF_CLOSE | 0x0A | (none) |
| RESET | 0x0B | Error code |
| PING | 0x0C | Opaque data |
| PONG | 0x0D | Echo of PING data |
| ERROR | 0x0E | Error code + message |
| GOAWAY | 0x0F | Last stream ID + reason |

## Error Codes

| Code | Name | Meaning |
|------|------|---------|
| 0 | NONE | No error |
| 1 | PROTOCOL_VIOLATION | Invalid frame sequence |
| 2 | INTERNAL | Internal server error |
| 3 | FLOW_CONTROL | Flow control violation |
| 4 | STREAM_NOT_FOUND | Unknown stream ID |
| 5 | STREAM_CLOSED | Stream already closed |
| 6 | STREAM_LIMIT | Too many streams |
| 7 | AUTH_FAILED | Authentication failed |
| 8 | TOKEN_REJECTED | Token invalid/revoked |
| 9 | REGISTRATION | Service registration failed |
| 10 | RESOURCE_EXHAUSTED | Memory/connection limit |
| 11 | TIMEOUT | Operation timed out |
| 12 | GOAWAY | Server shutting down |

## Stream States

```
IDLE → OPEN → HALF_CLOSED_LOCAL → CLOSED
            → HALF_CLOSED_REMOTE → CLOSED
            → CLOSED (simultaneous close)
            → RESET (any state)
```

## Flow Control

Per-stream receive window starts at 256 KiB (configurable). Connection-wide
window and per-session memory cap are mandatory. Senders cannot transmit DATA
beyond granted credit. WINDOW_UPDATE must be validated against overflow and
maximum credit. Backpressure must stop socket reads rather than buffering
unbounded data.

## Authentication

High-entropy scoped tokens, hashed at rest (if persisted), compared in
constant time, sent only over authenticated TLS. Fresh server nonces
bind authentication to the session handshake.