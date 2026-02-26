# Render Stream v1 (Dashboard-as-Brains)

## Summary

Render Stream v1 lets the dashboard execute effect logic locally and stream framebuffer data to K1 over binary WebSocket frames. K1 renders only the latest frame and remains deterministic under network jitter.

- transport: WebSocket binary frames (TCP)
- stream control: JSON commands (`render.stream.start`, `render.stream.stop`, `render.stream.status`)
- ownership: active control lease owner only
- stale safety: auto-fallback to internal renderer when stream goes stale

## Ownership and lease model

- Stream control and binary ingest are lease-gated.
- Only the active lease owner WebSocket client can start/stop stream or push binary frames.
- Observe clients can read status and subscribe to state broadcasts.

Related lease contract: [`control-lease-v1.md`](./control-lease-v1.md)

## WebSocket JSON contract

### `render.stream.start`

Request

```json
{
  "type": "render.stream.start",
  "requestId": "req-500",
  "targetFps": 120,
  "staleTimeoutMs": 750,
  "desiredPixelFormat": "rgb888",
  "desiredLedCount": 320,
  "clientName": "K1 Composer Dashboard",
  "clientInstanceId": "6f0d84a3-8f49-4da8-8c70-0b8f6db5a870"
}
```

Response (`render.stream.started`)

```json
{
  "type": "render.stream.started",
  "requestId": "req-500",
  "success": true,
  "data": {
    "active": true,
    "sessionId": "rs_f1a9d13209ac",
    "ownerWsClientId": 12,
    "targetFps": 120,
    "recommendedFps": 120,
    "staleTimeoutMs": 750,
    "frameContractVersion": 1,
    "pixelFormat": "rgb888",
    "ledCount": 320,
    "headerBytes": 16,
    "payloadBytes": 960,
    "maxPayloadBytes": 960,
    "mailboxDepth": 2,
    "lastFrameSeq": 0,
    "lastFrameRxMs": 0,
    "framesRx": 0,
    "framesRendered": 0,
    "framesDroppedMailbox": 0,
    "framesInvalid": 0,
    "framesBlockedLease": 0,
    "staleTimeouts": 0
  }
}
```

Errors

- `LEASE_REQUIRED`: no active lease
- `CONTROL_LOCKED`: lease owned by another client
- `STREAM_CONTRACT_MISMATCH`: unsupported pixel format or LED count

### `render.stream.stop`

Request

```json
{
  "type": "render.stream.stop",
  "requestId": "req-501",
  "reason": "user_stop"
}
```

Response (`render.stream.stopped`)

```json
{
  "type": "render.stream.stopped",
  "requestId": "req-501",
  "success": true,
  "data": {
    "stopped": true,
    "reason": "user_stop",
    "active": false,
    "sessionId": "rs_f1a9d13209ac"
  }
}
```

Errors

- `STREAM_NOT_ACTIVE`: no active stream session
- `CONTROL_LOCKED`: non-owner stop attempt

### `render.stream.status`

Request

```json
{
  "type": "render.stream.status",
  "requestId": "req-502"
}
```

Response (`render.stream.status`) returns same data object shape as `render.stream.started`.

### Broadcast: `render.stream.stateChanged`

Broadcast to all WS clients on start/stop/stale/lease-lost transitions.

```json
{
  "type": "render.stream.stateChanged",
  "event": "render.stream.stopped",
  "success": true,
  "data": {
    "active": false,
    "sessionId": "rs_f1a9d13209ac",
    "reason": "stale_timeout"
  }
}
```

## Binary frame contract (`frameContractVersion=1`)

### Header (16 bytes)

| Offset | Type | Field | Rule |
|---|---|---|---|
| `0..3` | bytes | magic | ASCII `K1F1` |
| `4` | `u8` | contractVersion | `1` |
| `5` | `u8` | pixelFormat | `1` (`rgb888`) |
| `6..7` | `u16` | flags | bitfield, default `0` |
| `8..11` | `u32` | seq | frame sequence, wrap allowed |
| `12..13` | `u16` | ledCount | `320` |
| `14..15` | `u16` | reserved | `0` |

All integer fields are little-endian.

### Payload

- length: `ledCount * 3`
- v1 fixed size: `960` bytes
- format: packed RGB888, strip order `[stripA(160), stripB(160)]`

### Ingest rules

- WS fragmentation is reassembled before contract validation.
- Non-owner binary frames are blocked and counted.
- Contract-invalid frames are dropped and counted.
- Device uses latest-frame-wins mailbox semantics (depth 2).

## Backpressure and latency policy

Dashboard sender policy:

- if `ws.bufferedAmount > queueDropThreshold`, drop frame
- do not queue old frames for catch-up
- continue sending the most recent frame next tick

Device policy:

- mailbox depth `2`
- overwrite stale pending frame slots
- render the most recent accepted frame at tick time

## Stale safety policy

If no valid frame is received within `staleTimeoutMs`:

- stream session transitions to inactive
- renderer falls back to internal effect mode
- `render.stream.stateChanged` is broadcast with reason `stale_timeout`
- stale counters are incremented

## Status exposure

`/api/v1/device/status` and WS status payloads include:

- `renderStream.*` session/contract fields
- `renderCounters.*` counters (`framesRx`, `framesRendered`, `framesDroppedMailbox`, `framesInvalid`, `framesBlockedLease`, `staleTimeouts`)

## Error codes

- `STREAM_NOT_ACTIVE`
- `STREAM_CONTRACT_MISMATCH`
- `STREAM_FRAME_INVALID`
- `CONTROL_LOCKED`
- `LEASE_REQUIRED`
- `LEASE_EXPIRED`

## Notes

- v1 transport is WebSocket binary only.
- UDP/WebTransport/WebRTC are deferred and can be added behind a new contract version.
