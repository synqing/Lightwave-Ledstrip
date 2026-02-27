# Protocol Map: v2 Hub ↔ Tab5 Node WebSocket Control Plane

**Purpose:** Inventory of WebSocket messages, flows, abstract state effects, and idempotency rules for Choreo conformance checking.

**Date:** 2026-01-18  
**Scope:** Core control-plane flow (connect → hello/status → parameter change)

---

## Architecture

**Hub:** LightwaveOS v2 (`firmware/v2/src/network/webserver/`)  
**Node:** Tab5.encoder (`firmware/Tab5.encoder/src/network/`)  
**Transport:** WebSocket over TCP/IP (port 80)  
**Message Format:** JSON with `type` field (ArduinoJson v7+)

---

## Core Flow (Phase 3 Initial Scope)

### 1. Connection Establishment

**Tab5 (Node) → v2 (Hub):** WebSocket Connect
- **Message type:** TCP/WS handshake (not JSON)
- **Abstract state effect:** `connState := "CONNECTING"` on Hub
- **Correlation keys:** None (connection-level event)
- **Idempotency rule:** Reconnect creates new `connEpoch`; old epoch state is discarded

**Tab5 (Node) → v2 (Hub):** `getStatus` (implicit hello)
- **Message type:** `{"type": "getStatus"}`
- **Abstract state effect:** Hub broadcasts `status` message to all connected clients
- **Correlation keys:** `connEpoch` (implicit, per-connection)
- **Idempotency rule:** **Idempotent** - safe to retry across epochs (always returns current state)

**v2 (Hub) → Tab5 (Node):** `status` (full state sync)
- **Message type:** `{"type": "status", "effectId": N, "brightness": M, "speed": K, ...}`
- **Abstract state effect:** 
  - Node: `handshakeComplete := true`
  - Node: `lastAppliedParams := {...}` (all parameters from status)
  - Node: `lastStatusTs := ts_ms` (current timestamp)
- **Correlation keys:** `connEpoch` (implicit, per-connection)
- **Idempotency rule:** **Idempotent** - reapplying same parameter values is safe (no side effects)

---

### 2. Parameter Updates (Core Flow)

**Tab5 (Node) → v2 (Hub):** `parameters.set`
- **Message type:** `{"type": "parameters.set", "brightness": N, "speed": M, ...}`
- **Abstract state effect:**
  - Hub: `activeParams["brightness"] := N` (immediate update)
  - Hub: Broadcasts `parameters.changed` to all clients (including sender)
- **Correlation keys:** `connEpoch` (implicit)
- **Idempotency rule:** **Idempotent** - `parameters.set` with same values is safe (no side effects)

**v2 (Hub) → Tab5 (Node):** `parameters.changed`
- **Message type:** `{"type": "parameters.changed", ...}` (notification)
- **Abstract state effect:**
  - Node: `lastAppliedParams := {...}` (if parameters differ)
  - Node: May trigger `getStatus` request if state is stale
- **Correlation keys:** `connEpoch` (implicit)
- **Idempotency rule:** **Idempotent** - duplicate notifications are safe (no side effects)

**Tab5 (Node) → v2 (Hub):** `effects.setCurrent`
- **Message type:** `{"type": "effects.setCurrent", "effectId": N}`
- **Abstract state effect:**
  - Hub: `activeEffectId := N` (immediate effect switch)
  - Hub: Broadcasts `effect.changed` to all clients
- **Correlation keys:** `connEpoch` (implicit)
- **Idempotency rule:** **Idempotent** - `effects.setCurrent` with same `effectId` is safe (effect activation is idempotent)

---

### 3. Zone Updates (Tab5 Extension)

**Tab5 (Node) → v2 (Hub):** `zone.setEffect`
- **Message type:** `{"type": "zone.setEffect", "zoneId": N, "effectId": M}`
- **Abstract state effect:**
  - Hub: `zones[N].effectId := M` (immediate zone update)
  - Hub: Broadcasts `zone.status` or `zones.changed` to all clients
- **Correlation keys:** `connEpoch` (implicit), `zoneId` (explicit)
- **Idempotency rule:** **Idempotent** - `zone.setEffect` with same `zoneId` and `effectId` is safe

**v2 (Hub) → Tab5 (Node):** `zone.status`
- **Message type:** `{"type": "zone.status", "zones": [...]}`
- **Abstract state effect:**
  - Node: Updates zone parameters for Unit B encoders (indices 8-15)
- **Correlation keys:** `connEpoch` (implicit)
- **Idempotency rule:** **Idempotent** - reapplying same zone state is safe

---

## Message Inventory (Extended Coverage)

### Tab5 → v2 (Node → Hub Commands)

| Message Type | Abstract State Effect | Correlation Keys | Idempotency |
|--------------|----------------------|------------------|-------------|
| `getStatus` | Hub broadcasts `status` | `connEpoch` | ✅ Always safe |
| `parameters.set` | `activeParams` updated, broadcast `parameters.changed` | `connEpoch` | ✅ Safe if values unchanged |
| `effects.setCurrent` | `activeEffectId` changed, broadcast `effect.changed` | `connEpoch` | ✅ Safe if `effectId` unchanged |
| `zone.setEffect` | `zones[zoneId].effectId` updated | `connEpoch`, `zoneId` | ✅ Safe if same values |
| `zone.setBrightness` | `zones[zoneId].brightness` updated | `connEpoch`, `zoneId` | ✅ Safe if same values |
| `zone.setSpeed` | `zones[zoneId].speed` updated | `connEpoch`, `zoneId` | ✅ Safe if same values |
| `zone.setPalette` | `zones[zoneId].paletteId` updated | `connEpoch`, `zoneId` | ✅ Safe if same values |

### v2 → Tab5 (Hub → Node Notifications)

| Message Type | Abstract State Effect | Correlation Keys | Idempotency |
|--------------|----------------------|------------------|-------------|
| `status` | `handshakeComplete := true`, `lastAppliedParams := {...}` | `connEpoch` | ✅ Always safe |
| `parameters.changed` | `lastAppliedParams` updated (if changed) | `connEpoch` | ✅ Safe (notification only) |
| `effect.changed` | Node UI updates effect display | `connEpoch` | ✅ Safe (notification only) |
| `zone.status` | Zone parameters synced | `connEpoch` | ✅ Safe (state sync) |

---

## Connection Lifecycle Events

### Connection Events (Abstract)

| Event Type | Source | Abstract State Effect | Correlation |
|------------|--------|----------------------|-------------|
| `ws.connect` | TCP/WS handshake | `connState := "CONNECTING"` | `connEpoch` increments |
| `ws.connected` | TCP/WS established | `connState := "CONNECTED"` | `connEpoch` (current) |
| `ws.disconnect` | TCP/WS closed | `connState := "DISCONNECTED"`, clear `pendingCommands` | `connEpoch` (closed) |

---

## Correlation Keys

**Primary correlation key:** `connEpoch`
- **Scope:** Per-connection session
- **Increment:** On each successful WebSocket reconnect
- **Usage:** All messages within an epoch share ordering guarantees

**Secondary correlation keys (optional):**
- **`requestId`:** Optional string field for request/response pairing (not all messages use it)
- **`zoneId`:** Explicit zone identifier (0-3) for zone-specific commands

**Message ID (not currently used):**
- Current protocol does **not** include explicit `messageId` or sequence numbers
- Deduplication relies on `(type, payload, connEpoch)` hash

---

## Idempotency Rules (Predicates)

### Parameter Updates

**Predicate:** `parameters.set({param: value})` is idempotent if:
- Same `param` field name
- Same `value` (integer/string)
- Any `connEpoch` (safe across reconnects)

**Example:** `{"type": "parameters.set", "brightness": 200}` applied twice → same final state.

### Effect Changes

**Predicate:** `effects.setCurrent({effectId: N})` is idempotent if:
- Same `effectId`
- Any `connEpoch`

**Example:** `{"type": "effects.setCurrent", "effectId": 12}` applied twice → same final state.

### Zone Updates

**Predicate:** `zone.setEffect({zoneId: N, effectId: M})` is idempotent if:
- Same `zoneId`
- Same `effectId`
- Any `connEpoch`

**Example:** `{"type": "zone.setEffect", "zoneId": 1, "effectId": 5}` applied twice → same final state.

---

## Message Soup Semantics

**Within a connection epoch:**
- Messages may arrive in **any order** (TCP guarantees in-order delivery, but application logic processes messages asynchronously).
- **Duplicate detection:** Use `(type, payload, connEpoch)` hash (no explicit message IDs).
- **State updates are commutative:** Applying parameter updates in different orders produces the same final state (last value wins).

**Across epochs:**
- Messages from different epochs are **independent**.
- Node must **re-sync state** via `getStatus` on new epoch.
- **State may be stale:** Hub state may have changed during Node's disconnected period.

---

## State Synchronization Rules

### Handshake Requirement

**Invariant:** Node must **not apply parameters** before `handshakeComplete == true`.

**Verification:**
- `handshakeComplete := true` only after receiving first `status` message from Hub.
- Node's `getStatus` (or implicit Hub broadcast) triggers `status` response.
- Node applies parameters from `status` message only after `handshakeComplete` flag set.

### Acknowledgment Semantics

**Current protocol:** **Implicit acknowledgments** via `status` / `parameters.changed` notifications.

**Rule:** If Node sends `parameters.set` and receives `parameters.changed` with matching values in the same `connEpoch`, the update is acknowledged.

**No explicit ACK messages:** Protocol relies on state sync messages (`status`, `parameters.changed`) as implicit acks.

---

## Future Expansion (Out of Scope for Phase 3)

**Messages not in initial scope:**
- Streaming subscriptions (`ledStream.subscribe`, `audio.subscribe`)
- Batch operations (`batch`)
- Transitions (`transition.trigger`)
- Audio-specific commands (`audio.parameters.set`)
- Motion commands (`motion.*`)
- Color correction commands (`colorCorrection.*`)

**Rationale:** Start with **one core flow** (connect → status → parameter change) before expanding coverage.

---

**Version:** 1.0  
**Next Update:** After Phase 3 (Choreo step model) validation
