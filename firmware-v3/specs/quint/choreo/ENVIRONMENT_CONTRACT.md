# Environment Contract: v2 Hub ↔ Tab5 Node WebSocket Protocol

**Purpose:** Define fault model, ordering guarantees, and trace semantics for Choreo conformance checking.

**Date:** 2026-01-18  
**Scope:** WebSocket control plane between LightwaveOS v2 (Hub) and Tab5.encoder (Node)

---

## Connection Epoch (`connEpoch`)

**Definition:** A **connection epoch** is a logical session identifier that increments on each WebSocket reconnect. It serves as a correlation boundary: messages sent within the same epoch share ordering guarantees; messages across epochs are independent.

**Semantics:**
- `connEpoch` starts at 0 on firmware boot.
- Each **successful WebSocket connect** increments `connEpoch` (atomically).
- Each **disconnect/reconnect cycle** creates a new epoch.
- Epoch is **not** reset on parameter/state changes (only on connection events).

**Usage in traces:**
- Every JSONL event includes `connEpoch: int`.
- Every ITF state snapshot includes `connEpoch: int` in abstract state.
- Correlation keys (session tokens, message IDs) are scoped per epoch.

---

## Ordering Guarantees

### Within a single connection epoch:

**WebSocket (TCP) provides:**
- **In-order delivery** of messages on a single TCP connection.
- **At-most-once delivery** (messages are not duplicated by TCP).
- **Connection-level atomicity**: messages are delivered in the order sent.

**Our application protocol adds:**
- **Message soup semantics**: Within an epoch, any message may arrive at any time (nondeterministic order), but TCP ensures **no out-of-order delivery** and **no duplicates from TCP**.
- **State updates are idempotent**: Reapplying the same parameter value is safe (no side effects).

### Across connection epochs:

**No ordering guarantees:**
- Messages from epoch `N` and epoch `N+1` are **completely independent**.
- State from epoch `N` may be stale when epoch `N+1` begins.
- The Node (Tab5) must **re-sync state** on each new epoch via `getStatus` or await `status` message.

**Idempotency across epochs:**
- Parameter updates (`parameters.set`, `zone.setEffect`, etc.) are safe to replay across epochs (no side effects if already applied).
- Effect changes (`effects.setCurrent`) are safe to re-apply (effect activation is idempotent).

---

## Duplication Rules

### Application-level retries:

**Tab5 (Node) may retry:**
- Parameter change commands if no acknowledgment received (rate-limited to 50ms per parameter).
- `getStatus` requests if connection was unstable.
- **Rule:** Retries include the same `connEpoch` as the original attempt.

**v2 (Hub) does not retry:**
- Hub is server-only; it responds to client requests but does not initiate retries.

### Network-level replay (TCP retransmission):

**Detected by:**
- Same message payload + same `connEpoch` + same direction (send/recv) + timestamp difference < RTT threshold.

**Treatment:**
- **Duplicate detection**: Use `messageId` (if present) or `(type, payload, connEpoch)` hash.
- **Safe to ignore**: Idempotent updates (parameters, effects, zones) can be safely ignored if already applied.

---

## Time Model

### Monotonic Local Timestamps (Primary)

**Definition:**
- Each device maintains a **monotonic clock** (milliseconds since boot).
- Timestamps are **local to each device** and **not synchronized** across devices.
- Format: `ts_ms: uint32_t` (milliseconds since device boot).

**Usage:**
- All JSONL events include `ts_ms` (local monotonic time).
- ITF traces preserve per-device timestamps (Hub `ts_ms` vs Node `ts_ms` are **not comparable**).
- Conformance checking uses **relative ordering** within a single device's trace, not absolute wall-clock time.

### Wall Clock (Optional, for human readability)

**Definition:**
- Optional `ts_wall: string` (ISO 8601) for human-readable trace inspection.
- **Never used** for conformance logic (only for debugging/display).

**Limitation:**
- Wall clock is unreliable on embedded devices (no NTP sync by default).
- Conformance must not depend on wall-clock time.

---

## Abstract State Model

The Choreo spec operates on **abstract state**, not raw firmware state. This abstraction maps firmware events to spec-observable state:

### Node (Tab5) Abstract State:

```quint
type NodeState = {
  connState: "DISCONNECTED" | "CONNECTING" | "CONNECTED" | "ERROR",
  connEpoch: int,
  handshakeComplete: bool,        // True after receiving first "status" message
  lastAppliedParams: Map[str, int],  // Last known parameter values (from server)
  pendingCommands: List[Message],     // Commands sent but not yet acknowledged
  lastStatusTs: int                   // Last status message timestamp (ts_ms)
}
```

### Hub (v2) Abstract State:

```quint
type HubState = {
  connState: Map[Node, "DISCONNECTED" | "CONNECTING" | "CONNECTED"],
  connEpoch: Map[Node, int],       // Per-node epoch tracking
  activeParams: Map[str, int],      // Current parameter values
  lastBroadcastTs: int              // Last status broadcast timestamp (ts_ms)
}
```

---

## Event → State Transition Rules

### JSONL Event Types:

1. **`ws.connect`**: Node attempts connection
   - **State effect**: `connState := "CONNECTING"`, `connEpoch := connEpoch + 1`

2. **`ws.connected`**: WebSocket handshake completes
   - **State effect**: `connState := "CONNECTED"`

3. **`msg.recv`**: Message received (Hub or Node)
   - **State effect**: Add to `pendingMsgs` (message soup), update state fields if applicable

4. **`msg.send`**: Message sent (Hub or Node)
   - **State effect**: Add to `pendingCommands` (for Node), update state if immediate effect

5. **`msg.ack`**: Implicit acknowledgment (e.g., `status` response to `getStatus`)
   - **State effect**: Remove from `pendingCommands`, update `lastAppliedParams`

6. **`ws.disconnect`**: Connection lost
   - **State effect**: `connState := "DISCONNECTED"`, clear `pendingCommands`

7. **`timer.fire`**: Timeout event (reconnect backoff, keepalive timeout)
   - **State effect**: Trigger reconnection or state reset

---

## Conformance Checking Assumptions

**Fault model assumptions:**
- **Network may drop messages** (within or across epochs).
- **Network may reorder messages** (across epochs only; within epoch, TCP guarantees order).
- **Application may retry** (idempotent updates are safe to replay).
- **Connection may reconnect** (new epoch, state re-sync required).

**Invariants to check:**
1. **NoEarlyApply**: Node never applies parameters before `handshakeComplete == true`.
2. **AckScopedToEpoch**: Acknowledgments only reference commands from the same `connEpoch`.
3. **IdempotentUpdates**: Reapplying the same parameter value (same type, same value, any epoch) is safe.

---

## Trace Format Requirements

**JSONL Event Schema** (minimal):
```json
{"ts_ms": 12345, "connEpoch": 0, "type": "ws.connect", "direction": "client", "node": "Tab5"}
{"ts_ms": 12350, "connEpoch": 0, "type": "msg.send", "direction": "client", "msg": {"type": "getStatus"}}
{"ts_ms": 12355, "connEpoch": 0, "type": "msg.recv", "direction": "client", "msg": {"type": "status", ...}}
```

**ITF State Schema** (derived from events):
```json
{
  "states": [
    {"state": {"connState": "DISCONNECTED", "connEpoch": 0, "handshakeComplete": false, ...}},
    {"state": {"connState": "CONNECTING", "connEpoch": 0, "handshakeComplete": false, ...}},
    {"state": {"connState": "CONNECTED", "connEpoch": 0, "handshakeComplete": true, ...}}
  ]
}
```

---

**Version:** 1.0  
**Next Update:** After Phase 2 (Protocol Map) validation
