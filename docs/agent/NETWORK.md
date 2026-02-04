# Network & Serial

> **When to read this:** Before modifying API endpoints, WebSocket protocol, WiFi behaviour, or serial commands.

## REST API v1

Base URL: `http://lightwaveos.local/api/v1/`

Key endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/effects` | GET | Paginated effects list |
| `/api/v1/effects/set` | POST | Set effect `{effectId: N}` |
| `/api/v1/parameters` | GET/POST | Visual parameters |
| `/api/v1/device/status` | GET | Uptime, heap, network |
| `/api/v1/transitions/trigger` | POST | Trigger transition |
| `/api/v1/batch` | POST | Batch operations (max 10) |

Rate limits: 20 req/sec HTTP, 50 msg/sec WebSocket. Returns 429 when exceeded.

Response format: `{"success": true, "data": {...}, "timestamp": 12345, "version": "1.0"}`

**Full API reference:** [docs/api/api-v1.md](../api/api-v1.md)

## WebSocket

Connect to `ws://lightwaveos.local/ws`

v1 commands: `transition.trigger`, `effects.getMetadata`, `batch`, `parameters.get`, `ledStream.subscribe`, `audio.subscribe`

Legacy commands: `setEffect`, `setBrightness`, `setZoneEffect`, etc.

## UDP Streaming (iOS)

LED and audio binary frames are delivered via UDP to avoid TCP ACK timeouts on weak WiFi. WebSocket remains for commands and JSON events.

| Channel | Transport | Data |
|---------|-----------|------|
| Commands/status | WS (TCP) | JSON messages |
| LED frames | UDP | 966 bytes, magic `0xFE`, 10 FPS |
| Audio frames | UDP | 464 bytes, magic `0x00445541`, 15 FPS |

**Negotiation:** iOS sends `{"type": "ledStream.subscribe", "udpPort": 41234}` via WS. Firmware sends UDP datagrams to `clientIP:41234`. Clients without `udpPort` receive WS binary (backward compatible).

**Key files:** `UdpStreamer.h/cpp` (firmware), `UDPStreamReceiver.swift` (iOS)

## Serial Commands

Connect at 115200 baud.

| Command | Action |
|---------|--------|
| `e` | Effects menu |
| `p` | Palette selection |
| `b <0-255>` | Set brightness |
| `s <1-50>` | Set speed |
| `h` | Help menu |
| `l` | List effects |
| `net status` | WiFi status |
| `validate <id>` | Run effect validation |

## Debug System

Per-domain debug control. Full docs: [firmware/v2/docs/debugging/DEBUG_SYSTEM.md](../../firmware/v2/docs/debugging/DEBUG_SYSTEM.md)

```
dbg                       Show current config
dbg <0-5>                 Set global level (0=OFF, 2=WARN default)
dbg audio <0-5>           Set audio domain level
dbg render <0-5>          Set render domain level
dbg status                Print audio health (one-shot)
dbg spectrum              Print 64-bin FFT (one-shot)
dbg memory                Print heap/stack (one-shot)
dbg interval status <N>   Auto-print every N seconds
```
