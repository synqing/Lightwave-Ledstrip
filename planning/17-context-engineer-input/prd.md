# Product Requirements Document: Legacy Parity Recovery

> **Traceability Note:** This PRD uses section numbers (§1-§10) that are referenced by the Technical Blueprint and Task List. All user answers have been mapped to specific sections.
> **RFC 2119:** The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in RFC 2119.

## §1. Overview & Vision
The "Legacy Parity Recovery" initiative aims to restore full interoperability between the active `firmware/K1.8encoderS3` Node and the legacy LightwaveOS Hub. Currently, protocol mismatches (schema keys, command names) and missing core components (ZoneComposer) prevent the Node from functioning correctly in the ecosystem. This feature will implement a robust "Dual Key" WebSocket protocol, command aliasing, and re-integrate the Zone Engine with proper persistence, ensuring the Node works seamlessly with both legacy and modern controllers.

**Key Outcomes:**
- **Full Protocol Parity:** Node accepts commands from legacy Hubs (using `t`) and modern clients (using `type`).
- **Seamless Control:** Legacy command names (`effects.setCurrent`) work identically to modern ones via aliasing.
- **Zone Functionality:** LED zones operate correctly, persisting state across reboots and Hub disconnects.
- **Robustness:** System gracefully handles malformed messages and unknown types without crashing or spamming logs.

## §2. Problem Statement
The active firmware (`K1.8encoderS3`) has diverged from the legacy protocol expected by the Hub, causing a complete failure of remote control and zone management.

**Current Pain Points:**
- **Control Failure:** Hub sends commands using `t` key, but Node only listens for `type`.
- **Command Rejection:** Hub sends `effects.setCurrent`, Node expects `effects.changed`.
- **Missing Zones:** `ZoneComposer` is not initialized, rendering zone-specific effects impossible.
- **Silent Failures:** Malformed or unknown messages are ignored without feedback, making debugging impossible.

## §3. Target Users

| User Type | Needs | Key Actions |
|-----------|-------|-------------|
| **Legacy Hub User** | Existing Hub continues to control new Nodes without firmware updates. | Change effects, update zones via Hub UI. |
| **System Integrator** | Diagnostics for connection issues; robust error handling. | Monitor Serial logs for protocol errors. |
| **End User** | Consistent behavior (lights don't blackout on Wi-Fi drop). | Power cycle device, expecting zones to persist. |

## §4. Core Requirements

### §4.1 Functional Requirements

| ID | Requirement | Priority | Source |
|----|-------------|----------|--------|
| FR-1 | **Dual Schema Support:** `WsMessageRouter` MUST accept both `t` (legacy) and `type` (modern) keys as the message discriminator. | High | Q1 |
| FR-2 | **Command Aliasing:** Legacy command strings (e.g., `effects.setCurrent`) MUST map to current handlers (e.g., `handleEffectsChanged`). | High | Q2 |
| FR-3 | **Zone Initialization:** `ZoneComposer` MUST be initialized in `main.cpp` and integrated with the Router. | High | Q3 |
| FR-4 | **Error Feedback:** Node MUST send a structured error response (JSON) to the sender for malformed/missing fields AND log to Serial. See Appendix B. | Medium | Q4 |
| FR-5 | **Disconnection Persistence:** Node MUST retain the last known Zone/Effect state when WebSocket disconnects (do not reset to black). | Medium | Q6 |
| FR-6 | **Control Arbitration:** "Last-One-Wins" policy for Hub vs. Local Encoder updates, using source tagging to prevent echo loops. | Medium | Q7 |
| FR-7 | **JSON Parsing:** Parsing MUST be strict on required fields but flexible/tolerant on extra fields or type coercion (e.g., string "128" -> int 128). | Medium | Q9 |
| FR-8 | **Parameter Mapping:** Use existing `ParameterMap` lookup table for string-to-ID conversion (no hardcoding). | Low | Q10 |
| FR-9 | **State Persistence:** Zone configuration MUST be stored in NVS (Preferences) with 500ms debounce and loaded on boot (Hybrid RAM+NVS). | Medium | Q11, Q12 |

### §4.2 Non-Functional Requirements

| ID | Category | Requirement | Source |
|----|----------|-------------|--------|
| NFR-1 | Robustness | Unknown message types MUST be logged to Serial with rate-limiting (Max 1 log/type/2s + suppression counter). | Q5 |
| NFR-2 | Security | System operates on "Local Trust" model (SoftAP/LAN) with upgrade path to API Keys. | Q8 |
| NFR-3 | Maintainability | Legacy aliases MUST be marked with `legacy_alias=true` for future deprecation tracking. | Q2 |
| NFR-4 | Performance | Parsing and routing MUST NOT block the main render loop for >1ms. | Standard |

## §5. User Stories & Acceptance Criteria

### Epic 1: Protocol Parity

#### Story §5.1: Legacy Command Support
**As a** Hub User
**I want** my existing Hub to control the new Node
**So that** I don't have to upgrade my entire ecosystem at once.

**Acceptance Criteria:**
- [ ] §5.1.1: Sending `{"t": "effects.setCurrent", ...}` triggers effect change.
- [ ] §5.1.2: Sending `{"type": "effects.changed", ...}` triggers effect change.
- [ ] §5.1.3: Sending mixed keys/commands works if valid.

#### Story §5.2: Error Visibility
**As a** System Integrator
**I want** clear feedback when I send bad data
**So that** I can fix my controller software.

**Acceptance Criteria:**
- [ ] §5.2.1: Sending invalid JSON returns `{"type": "error", "code": "BAD_REQUEST"}` via WS.
- [ ] §5.2.2: Serial monitor shows "Bad Request: [Detail]" line.

### Epic 2: Zone Management

#### Story §5.3: Zone Persistence
**As a** User
**I want** my zone settings to stay after a reboot
**So that** I don't have to re-configure my lights every day.

**Acceptance Criteria:**
- [ ] §5.3.1: Change zones, reboot device -> Zones remain as set.
- [ ] §5.3.2: Disconnect Hub -> Lights stay on (Last State).

## §6. User Experience Contract
*→ Extracted into Blueprint §1.5/1.6 User Journey diagrams*

### §6.1 User Visibility Specification

| ID | User Action | User Sees (Exact) | User Does NOT See | Timing |
|----|-------------|-------------------|-------------------|--------|
| V-1 | Send Invalid JSON | Error JSON response (WS) + Serial Log | Internal parser exception | Immediate |
| V-2 | Disconnect Hub | Lights stay ON (Last State) | "Disconnected" blackout | Immediate |
| V-3 | Reboot Device | Lights restore to saved Zone Config | Default/Factory Config | Boot (<2s) |

### §6.2 Timing & Feedback Expectations

| ID | Event | Expected Timing | User Feedback | Failure Feedback |
|----|-------|-----------------|---------------|------------------|
| T-1 | Command RX | <10ms latency | Effect changes | Error JSON |
| T-2 | NVS Save | Background (<100ms) | None (Transparent) | Serial Log Error |

## §7. Artifact Ownership
*→ Extracted into Blueprint §2 System Boundaries*

### §7.1 Creation Responsibility

| ID | Artifact | Created By | When | App's Role |
|----|----------|------------|------|------------|
| O-1 | `ZoneComposer` Instance | **App** (`main.cpp`) | Boot | Create & Manage |
| O-2 | Zone Config (NVS) | **App** | Zone Change | Create & Update |
| O-3 | WebSocket Error | **App** | Parse Fail | Create & Send |
| O-4 | Legacy Command | **External** (Hub) | User Action | Process (Alias) |

### §7.2 External System Dependencies

| ID | External System | What It Creates | How App Knows | Failure Handling |
|----|-----------------|-----------------|---------------|------------------|
| E-1 | Legacy Hub | `t` keyed messages | WebSocket RX | Send Error / Log |

### §7.3 Derived Ownership Rules
*These become Blueprint §2.3 Boundary Rules*

| Source | Rule | Rationale |
|--------|------|----------|
| O-1 | `WsMessageRouter` must NOT create `ZoneComposer` | `main.cpp` owns lifecycle (Q3) |
| O-4 | App must NOT reject `t` key | Legacy parity required (Q1) |

## §8. State Requirements
*→ Extracted into Blueprint §3 State Transition Specifications*

### §8.1 State Isolation

| ID | Requirement | Rationale | Enforcement |
|----|-------------|-----------|-------------|
| SI-1 | **Source Tagging:** Updates must be tagged with `sourceId`. | Prevent ping-pong loops (Q7). | Router adds tag on RX. |

### §8.2 State Lifecycle

| ID | State | Initial | Created When | Cleared When | Persists Across |
|----|-------|---------|--------------|--------------|------------------|
| SL-1 | `ZoneConfig` | NVS Load / Default | Boot | Factory Reset | Reboot (NVS) |
| SL-2 | `ZoneComposer` | Null | `setup()` | Power Off | Runtime Only |

## §9. Technical Considerations
*→ Informs Blueprint §5, §6, §9, §11*

### §9.1 Architecture Decisions
- **ZoneComposer in Main:** Instantiated in `main.cpp` to ensure existence independent of network state.
- **NVS Persistence:** Using ESP32 Preferences/NVS for robust config storage with debouncing.
- **Flexible JSON:** Using ArduinoJson's type coercion where safe, but validating keys.

### §9.2 Integration Points
- **WsMessageRouter:** Refactor `route()` to handle `t`/`type` logic.
- **ParameterMap:** Use existing lookup table for parameter ID resolution.

### §9.3 Constraints
- **Memory:** `ZoneComposer` must fit in available heap (check sizes).
- **Latency:** Routing logic must be lightweight to avoid stalling LED animation.

## §10. Success Metrics
*→ Informs Blueprint §10 Testing Strategy*

| ID | Metric | Target | Measurement Method |
|----|--------|--------|--------------------|
| M-1 | Legacy Command Success | 100% | Golden Trace Replay test (Exact Match) |
| M-2 | Reboot Retention | 100% | Manual Power Cycle test |
| M-3 | Error Rate (Valid Cmds) | 0% | Logs |

---

## Appendix A: Answer Traceability

| User Answer (Summary) | Captured In |
|-----------------------|-------------|
| Q1: Dual `t`/`type` support | §4.1 (FR-1) |
| Q2: Aliasing (`effects.setCurrent`) | §4.1 (FR-2) |
| Q3: ZoneComposer in `main.cpp` | §4.1 (FR-3), §7.1 (O-1) |
| Q4: Error response + Log | §4.1 (FR-4), §6.1 (V-1) |
| Q5: Log unknown (rate-limited) | §4.2 (NFR-1) |
| Q6: Keep last state on disconnect | §4.1 (FR-5), §6.1 (V-2) |
| Q7: Last-one-wins + anti-ping-pong | §4.1 (FR-6), §8.1 (SI-1) |
| Q8: Local Trust (upgrade path) | §4.2 (NFR-2) |
| Q9: Flexible parsing, strict required | §4.1 (FR-7) |
| Q10: Lookup table mapping | §4.1 (FR-8) |
| Q11: Hybrid Persistence (RAM+NVS) | §4.1 (FR-9), §8.2 (SL-1) |
| Q12: NVS/Preferences storage | §4.1 (FR-9) |

## Appendix B: Legacy Parity Contract v1

> This appendix defines the non‑negotiable conditions under which legacy behaviour is considered “parity complete”. These gates are intended to remove interpretation space: either the system passes the contract or it does not. Legacy paths MUST NOT be deprecated until these conditions are met.

### B.1 WebSocket Control Plane (LP‑WS)

| ID | Requirement | Rationale |
|----|-------------|-----------|
| LP‑WS‑1 | Node WebSocket parser MUST accept both `t` and `type` as discriminator fields. | Backwards compatibility with legacy clients and forward compatibility with modern ones. |
| LP‑WS‑2 | Node MUST implement handlers for: `welcome`, `ota_update`, `state.snapshot`, `effects.setCurrent`, `parameters.set`, `zones.update`. | Full control‑plane coverage. |
| LP‑WS‑3 | `state.snapshot` MUST be applied immediately after WELCOME handshake, restoring both global and zone state. | Reconnection must be lossless. |
| LP‑WS‑4 | Parameter and zone changes MUST apply within 80ms of WS message receipt under normal load. | Meets latency budget (NFR‑2). |

Verification (normative):
- LP‑WS‑1: send messages with `t` and `type` fields; both MUST be parsed and routed.
- LP‑WS‑2: send each command type; the corresponding handler MUST be observable via logs or state change.
- LP‑WS‑3: reboot hub and node under load; post‑WELCOME state MUST match hub snapshot.
- LP‑WS‑4: WS RX → render apply latency MUST remain ≤ 80ms in trace capture.

### B.2 Zone Compositor (LP‑ZONE)

| ID | Requirement | Rationale |
|----|-------------|-----------|
| LP‑ZONE‑1 | `ZoneComposer` MUST be initialised in node‑runtime builds. | Hub‑driven zones require an active compositor. |
| LP‑ZONE‑2 | `ZoneComposer` MUST be enabled after the first authoritative `state.snapshot` (or equivalent zone‑enable signal) while the hub session is active. | Prevents zones from being permanently muted while keeping boot defaults safe. |
| LP‑ZONE‑3 | `zones.update` and `state.snapshot` zone fields MUST affect rendering whenever `ZoneComposer` is enabled. | Observable behaviour must match legacy v2. |
| LP‑ZONE‑4 | Zone state MUST be restorable from `state.snapshot` on reconnect. | Reconnection must restore per‑zone configuration. |

Verification (normative):
- LP‑ZONE‑1: node‑runtime build MUST show `ZoneComposer` initialisation in logs.
- LP‑ZONE‑2: traces MUST show `state.snapshot` → `ZoneComposer enabled` sequencing for an authenticated session.
- LP‑ZONE‑3: sending `zones.update` after enablement MUST change only the targeted zone on the LEDs.
- LP‑ZONE‑4: configure zones, disconnect, reconnect; post‑reconnect visual state MUST match pre‑disconnect.

### B.3 Node Readiness (LP‑READY)

| ID | Requirement | Rationale |
|----|-------------|-----------|
| LP‑READY‑1 | Node READY state MUST NOT require UDP audio packets. | Aligns with “UDP audio‑only” semantics. |
| LP‑READY‑2 | READY SHOULD depend on Wi‑Fi connected, WS authenticated, and time‑sync locked. | Minimal viable liveness conditions. |
| LP‑READY‑3 | Node MAY use WS keepalive cadence as an additional liveness signal. | Optional resilience. |
| LP‑READY‑4 | Node MUST remain READY when audio is inactive and no UDP beat ticks are present. | Idle behaviour must remain correct. |

Verification (normative):
- LP‑READY‑1/4: with audio disabled (no UDP), node MUST stay READY and keep last state.
- LP‑READY‑2/3: when WS or time‑sync fails, node MUST leave READY; when they recover, node MUST re‑enter READY without requiring UDP.

### B.4 Protocol Semantics (LP‑PROTO)

| ID | Requirement | Rationale |
|----|-------------|-----------|
| LP‑PROTO‑1 | Hub MUST NOT send parameter deltas via UDP in normal operation. | Enforces “UDP audio‑only” contract. |
| LP‑PROTO‑2 | Hub MUST use WebSocket as the single source of truth for control‑plane messages. | Prevents split‑brain between WS and UDP. |
| LP‑PROTO‑3 | Hub MUST batch parameter changes at 50ms intervals. | Matches PRD FR‑3 batching requirement. |
| LP‑PROTO‑4 | Hub MUST include `applyAt_us` scheduling field in control‑plane messages. | Enables synchronised apply across nodes. |

Verification (normative):
- LP‑PROTO‑1: production traces MUST show no `LW_UDP_PARAM_DELTA` packets; any remaining path MUST be explicitly marked as debug‑only.
- LP‑PROTO‑2/3/4: WS capture MUST show batched messages at ~50ms cadence with `applyAt_us` present and consistent across nodes.

### B.5 Golden Trace Oracle and Comparator

Golden traces define the observable legacy baseline. A change that breaks a golden trace is not parity‑compatible.

**Trace capture format (normative example):**
```json
{
  "timestamp": 1234567890,
  "source": "Hub",
  "payload": {"t": "effects.setCurrent", "val": 1},
  "expected_state": {"effectId": 1},
  "strict": true
}
```

Comparator rules:
- **Strict mode:** `payload` and `expected_state` MUST both match the oracle.
- **Tolerant mode:** only `expected_state` must match; extra fields and benign log differences are allowed.
- **Pass criteria:** all strict traces MUST pass before any legacy path is deprecated; tolerant traces MUST pass before feature branches are considered parity‑compatible.
