# Control Lease v1 (Option 4 Hard Exclusive)

## Summary

Control lease is an explicit hard-exclusive lock for mutating control paths.

- Lease authority: WebSocket owner connection.
- Lease transport: `control.acquire`, `control.heartbeat`, `control.release`.
- REST proof-of-ownership: `X-Control-Lease` token on mutating requests while lease is active.
- Scope: `global` in v1.
- Takeover: disabled in v1 (`takeoverAllowed=false`).
- Token encoding: base64url (RFC 4648 URL-safe alphabet) with no padding.
- Token entropy: CSPRNG-generated, minimum 16 random bytes (128-bit entropy).

## Render stream interplay

- `render.stream.start`, `render.stream.stop`, and binary frame ingest are lease-gated.
- Only the active lease owner WebSocket client can own an external render stream session.
- Observe clients can still read `render.stream.status`, `device.status`, and lease status.

## Canonical Lease State

| Field | Type | Notes |
|---|---|---|
| `active` | bool | Lease active flag |
| `leaseId` | string | Stable within lease lifetime, format `cl_<hex>` |
| `leaseToken` | string | Secret token (owner-only in acquire response, never in status/broadcast telemetry) |
| `scope` | string | `"global"` |
| `ownerWsClientId` | uint32 | AsyncWebSocket client id |
| `ownerClientName` | string | Client-provided name |
| `ownerInstanceId` | string | Client instance UUID |
| `acquiredAtMs` | uint32 | `millis()` snapshot |
| `expiresAtMs` | uint32 | `millis()` expiry |
| `ttlMs` | uint32 | default `5000` |
| `heartbeatIntervalMs` | uint32 | default `1000` |
| `takeoverAllowed` | bool | `false` |

## Security and Time Semantics

- Lease lifecycle authority is WebSocket only. REST can never grant or refresh leases.
- `leaseToken` must never appear in status payloads, `control.stateChanged` broadcasts, or telemetry logs.
- Internal expiry checks use wrap-safe monotonic arithmetic (`millis()` wrap tolerated).
- `remainingMs` is the client-rendering source of truth for countdown UX.

## WebSocket Commands

### `control.acquire`

Request:

```json
{
  "type": "control.acquire",
  "requestId": "req-101",
  "clientName": "K1 Composer",
  "clientInstanceId": "6f0d84a3-8f49-4da8-8c70-0b8f6db5a870",
  "scope": "global"
}
```

Success (`control.acquired`):

```json
{
  "type": "control.acquired",
  "requestId": "req-101",
  "success": true,
  "data": {
    "active": true,
    "leaseId": "cl_9f4a2137c1d6",
    "leaseToken": "2Ab8P9Qk0kY2eP7nX7c2Yw",
    "scope": "global",
    "ownerWsClientId": 12,
    "ownerClientName": "K1 Composer",
    "ownerInstanceId": "6f0d84a3-8f49-4da8-8c70-0b8f6db5a870",
    "ttlMs": 5000,
    "heartbeatIntervalMs": 1000,
    "acquiredAtMs": 1205530,
    "expiresAtMs": 1210530,
    "remainingMs": 5000,
    "takeoverAllowed": false
  }
}
```

Locked error:

```json
{
  "type": "error",
  "requestId": "req-101",
  "success": false,
  "error": {
    "code": "CONTROL_LOCKED",
    "message": "Control lease is held by another client",
    "ownerClientName": "K1 Composer",
    "remainingMs": 3120,
    "scope": "global"
  }
}
```

Acquire rules:

- If no active lease: acquire succeeds.
- If active lease owner is the same WebSocket client id: reacquire refreshes expiry.
- If active lease owner differs: `CONTROL_LOCKED`.
- Force takeover is not supported in v1.

### `control.heartbeat`

Request:

```json
{
  "type": "control.heartbeat",
  "requestId": "req-102",
  "leaseId": "cl_9f4a2137c1d6",
  "leaseToken": "2Ab8P9Qk0kY2eP7nX7c2Yw"
}
```

Success (`control.heartbeatAck`):

```json
{
  "type": "control.heartbeatAck",
  "requestId": "req-102",
  "success": true,
  "data": {
    "leaseId": "cl_9f4a2137c1d6",
    "ttlMs": 5000,
    "remainingMs": 5000,
    "expiresAtMs": 1211540
  }
}
```

Errors:

- `LEASE_INVALID`: token/id mismatch.
- `LEASE_EXPIRED`: lease timed out.
- `CONTROL_LOCKED`: another owner now holds lock.

### `control.release`

Request:

```json
{
  "type": "control.release",
  "requestId": "req-103",
  "leaseId": "cl_9f4a2137c1d6",
  "leaseToken": "2Ab8P9Qk0kY2eP7nX7c2Yw",
  "reason": "user_exit"
}
```

Success (`control.released`):

```json
{
  "type": "control.released",
  "requestId": "req-103",
  "success": true,
  "data": {
    "released": true,
    "leaseId": "cl_9f4a2137c1d6",
    "releasedAtMs": 1207800,
    "reason": "user_exit"
  }
}
```

### `control.status`

Request:

```json
{ "type": "control.status", "requestId": "req-104" }
```

Response:

```json
{
  "type": "control.status",
  "requestId": "req-104",
  "success": true,
  "data": {
    "active": true,
    "leaseId": "cl_9f4a2137c1d6",
    "scope": "global",
    "ownerClientName": "K1 Composer",
    "ownerInstanceId": "6f0d84a3-8f49-4da8-8c70-0b8f6db5a870",
    "ownerWsClientId": 12,
    "remainingMs": 2140,
    "ttlMs": 5000,
    "heartbeatIntervalMs": 1000,
    "takeoverAllowed": false
  }
}
```

### `control.stateChanged` broadcast

Broadcast to all WebSocket clients on acquire, heartbeat, release, expiry, or owner disconnect.

```json
{
  "type": "control.stateChanged",
  "event": "acquired",
  "success": true,
  "data": {
    "active": true,
    "leaseId": "cl_9f4a2137c1d6",
    "scope": "global",
    "ownerClientName": "K1 Composer",
    "ownerInstanceId": "6f0d84a3-8f49-4da8-8c70-0b8f6db5a870",
    "ownerWsClientId": 12,
    "ttlMs": 5000,
    "heartbeatIntervalMs": 1000,
    "remainingMs": 2140,
    "takeoverAllowed": false
  }
}
```

## REST Contract

### `GET /api/v1/control/status`

Read current lease state.

### Mutating REST while lease active

Mutating endpoints are any requests that change device state, presets, effect selection, parameters, transport state, or persistent settings.

Required headers:

- `X-Control-Lease: <leaseToken>`
- `X-Control-Lease-Id: <leaseId>` (optional diagnostic)

Responses:

- `428 LEASE_REQUIRED`: missing `X-Control-Lease`.
- `403 LEASE_INVALID`: token invalid for provided active `leaseId`.
- `409 CONTROL_LOCKED`: lease owned by another client.
- `409 LEASE_EXPIRED`: lease timed out before request validation.
- `423` is intentionally not used (avoid WebDAV-specific lock semantics in this API).

## Canonical Lock Error Schema

REST:

```json
{
  "success": false,
  "error": {
    "code": "CONTROL_LOCKED",
    "message": "Control lease is held by another client",
    "ownerClientName": "K1 Composer",
    "remainingMs": 1820,
    "scope": "global",
    "requiredHeader": "X-Control-Lease"
  },
  "timestamp": 1208600,
  "version": "2.0"
}
```

WS:

```json
{
  "type": "error",
  "requestId": "req-201",
  "success": false,
  "error": {
    "code": "CONTROL_LOCKED",
    "message": "Command blocked by active control lease",
    "ownerClientName": "K1 Composer",
    "remainingMs": 1820,
    "scope": "global"
  }
}
```

## Mutability Matrix

| Source | Lease inactive | Lease active, owner | Lease active, non-owner |
|---|---|---|---|
| WS mutating commands | allow | allow | block |
| REST mutating commands | allow | allow with token | block |
| Encoder input | allow | block | block |
| Serial mutating input | allow | block | block |
| Read-only REST/WS | allow | allow | allow |

## Exemptions

These remain permitted under lock:

- `control.acquire`
- `control.heartbeat`
- `control.release`
- `control.status`
- `auth.*`
- OTA routes/commands (`/api/v1/firmware/*`, `/update`, `ota.*`)
