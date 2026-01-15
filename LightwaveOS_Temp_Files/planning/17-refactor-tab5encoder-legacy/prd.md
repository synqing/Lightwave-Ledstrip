# Product Requirements Document: Tab5 Hub Protocol Refactor

> **Traceability Note:** This PRD uses section numbers (§1-§10) that are referenced by the Technical Blueprint and Task List. All user answers have been mapped to specific sections.

## §1. Overview & Vision

This refactor transforms Tab5.encoder from a legacy WebSocket **client** to a modern LightwaveOS v2 **hub server**, reversing the architectural relationship with K1 nodes. The Tab5 device will become the authoritative control hub managing 1-5 K1.LightwaveOS nodes, broadcasting commands via WebSocket control plane while reserving UDP exclusively for time-critical audio metrics (beat tracking, BPM, phase).

**Key Outcomes:**
- Clean protocol separation: WebSocket for reliable command control, UDP exclusively for audio metrics
- Centralized state management via HubState singleton ensuring consistent parameter synchronization
- Modern command naming (zones.*, colorCorrection.setConfig) eliminating legacy singular formats
- Deterministic multi-node synchronization using scheduled applyAt_us timestamps
- Robust session lifecycle with automatic state resynchronization on node reconnection

## §2. Problem Statement

The current Tab5.encoder implementation creates three critical architectural problems:

**Current Pain Points:**
- **Protocol Mismatch**: Legacy WebSocketClient sends outdated commands ("zone.setEffect") that K1 nodes silently ignore, causing mysterious failures since K1 only registers "zones.*" handlers
- **UDP Bandwidth Waste**: Sending PARAM_DELTA (brightness/speed/palette) at 100Hz consumes wireless bandwidth even when parameters haven't changed, reducing capacity for time-critical audio metrics
- **Architecture Confusion**: WebSocketClient.cpp (legacy client mode) coexists with hub/ infrastructure, causing ParameterHandler to send to WebSocketClient instead of HubState, preventing proper multi-node broadcast
- **State Inconsistency**: No centralized state management means reconnecting nodes display wrong effects until user touches an encoder, and late-joining nodes never receive current state
- **Synchronization Skew**: Immediate parameter application introduces per-node timing differences based on Wi-Fi jitter and WebSocket scheduling, breaking synchronized multi-node effects

## §3. Target Users

| User Type | Needs | Key Actions |
|-----------|-------|-------------|
| Firmware Developer | Maintain clean architecture separation between hub and client modes | Refactor ParameterHandler, deprecate WebSocketClient, implement HubState |
| System Integrator | Deploy multi-node LED installations with 1-5 K1 nodes per Tab5 hub | Configure fleet, verify synchronized effects, monitor node health |
| End User (Indirect) | Reliable LED effects synchronized across multiple nodes | Turn encoders, trigger effects, expect consistent visual response |

## §4. Core Requirements

### §4.1 Functional Requirements

| ID | Requirement | Priority | Source |
|----|-------------|----------|--------|
| FR-1 | Tab5 hub SHALL broadcast WebSocket commands using modern protocol (zones.*, colorCorrection.setConfig) ONLY | High | Q3, Q6 |
| FR-2 | Tab5 hub SHALL reserve UDP exclusively for audio metrics (BEAT_TICK message type with BPM/phase/flags) | High | Q7, Q9 |
| FR-3 | Tab5 hub SHALL batch parameter changes every 50ms before broadcasting via WebSocket | High | Q2, Q11 |
| FR-4 | Tab5 hub SHALL implement HubState singleton storing authoritative desired state for all global/zone parameters | High | Q12, Q13 |
| FR-5 | Tab5 hub SHALL resend complete state snapshot immediately after WELCOME handshake | High | Q14 |
| FR-6 | Tab5 hub SHALL schedule parameter broadcasts with applyAt_us timestamps (30ms ahead) | High | Q10 |
| FR-7 | Tab5 hub SHALL iterate HubRegistry::forEachReady() to broadcast to all connected nodes | Medium | Q8 |
| FR-8 | K1 nodes SHALL initiate WebSocket connection via HELLO→WELCOME handshake | High | Q1 |
| FR-9 | Tab5 hub SHALL maintain session state until node disconnects or keepalive timeout (LW_CLEANUP_TIMEOUT_MS) | Medium | Q4 |
| FR-10 | Tab5 hub SHALL support 1-5 K1 nodes simultaneously without multicast complexity | Medium | Q15 |

### §4.2 Non-Functional Requirements

| ID | Category | Requirement | Source |
|----|----------|-------------|--------|
| NFR-1 | Performance | UDP bandwidth SHALL be minimal, only transmitting when audio is active | Q9 |
| NFR-2 | Performance | WebSocket message rate SHALL be proportional to human interaction (50ms throttle), not encoder edge rate | Q11 |
| NFR-3 | Reliability | Parameter changes SHALL use WebSocket for ordering and reliability guarantees | Q7 |
| NFR-4 | Synchronization | Multi-node parameter application SHALL use deterministic applyAt_us scheduling to prevent timing skew | Q10 |
| NFR-5 | Compatibility | Tab5 SHALL strictly follow common/proto/ protocol definitions as single source of truth | Q5 |
| NFR-6 | Scalability | Architecture SHALL support up to 5 K1 nodes using simple forEachReady() fanout | Q15 |
| NFR-7 | Code Quality | All code/comments SHALL use British English spelling | Constraints |

### §4.3 Detailed Requirement Breakdown (76 Requirements)

The tables in §4.1–§4.2 are the **top-level** requirements. This section expands them into an implementation-ready catalogue of **76** detailed requirements (functional + non-functional), grouped under the same IDs.

| ID | Requirement | Source |
|----|-------------|--------|
| FR-1.1 | The Tab5 hub SHALL emit zone commands using `type` values that start with `zones.` (plural) and SHALL NOT emit `zone.` (singular). | Q3, Q6 |
| FR-1.2 | The Tab5 hub SHALL support `zones.setEffect` with fields `zoneId`, `effectId`, and `applyAt_us`. | Q3, Q6, Q10 |
| FR-1.3 | The Tab5 hub SHALL support `zones.setBrightness` with fields `zoneId`, `brightness`, and `applyAt_us`. | Q3, Q6, Q10 |
| FR-1.4 | The Tab5 hub SHALL support `zones.setSpeed` with fields `zoneId`, `speed`, and `applyAt_us`. | Q3, Q6, Q10 |
| FR-1.5 | The Tab5 hub SHALL support `zones.setPalette` with fields `zoneId`, `paletteId`, and `applyAt_us`. | Q3, Q6, Q10 |
| FR-1.6 | The Tab5 hub SHALL support `zones.setBlend` with fields `zoneId`, `blendMode`, and `applyAt_us`. | Q3, Q6, Q10 |
| FR-1.7 | The Tab5 hub SHALL support `zones.update` for combined zone updates (when multiple zone fields change within one batch window). | Q2, Q3, Q11 |
| FR-1.8 | The Tab5 hub SHALL use `colorCorrection.setConfig` (string literal) as the unified colour correction command name. | Q6 |
| FR-2.1 | The Tab5 hub SHALL use UDP for audio metrics ONLY and SHALL NOT send control/parameter deltas over UDP. | Q7 |
| FR-2.2 | The Tab5 hub SHALL send UDP packets of type `LW_UDP_BEAT_TICK` ONLY (no `LW_UDP_PARAM_DELTA` after refactor). | Q7 |
| FR-2.3 | The Tab5 hub SHALL send UDP BEAT_TICK packets at `LW_UDP_TICK_HZ` (100Hz) ONLY while audio is active. | Q9 |
| FR-2.4 | The Tab5 hub SHALL implement `hasActiveAudioMetrics()` gating such that silence produces zero UDP fanout. | Q9 |
| FR-2.5 | The Tab5 hub SHALL populate `lw_udp_hdr_t` fields (`proto`, `msgType`, `payloadLen`, `seq`, `tokenHash`, `hubNow_us`, `applyAt_us`) for every UDP packet it sends. | Q7, Q10 |
| FR-2.6 | The Tab5 hub SHALL write all UDP packet fields in network byte order (big-endian) as per `common/proto/udp_packets.h`. | Q5 |
| FR-2.7 | The Tab5 hub SHALL fan out identical BEAT_TICK payload values to all READY nodes on each tick. | Q8, Q9 |
| FR-2.8 | The Tab5 hub SHALL define an `AudioMetrics` interface/structure that provides BPM, phase, and flags (stubbed until audio integration exists). | Q7 |
| FR-3.1 | The Tab5 hub SHALL aggregate parameter changes and transmit WebSocket updates on a fixed 50ms cadence. | Q2, Q11 |
| FR-3.2 | The batching cadence SHALL be implemented as a timer/tick in the hub loop (not driven directly by encoder edges). | Q11 |
| FR-3.3 | The batching mechanism SHALL coalesce multiple encoder updates within a 50ms window into a single broadcast per parameter family. | Q2, Q11 |
| FR-3.4 | The Tab5 hub SHALL transmit no WebSocket parameter broadcasts if no parameters are dirty for the current 50ms window. | Q2 |
| FR-3.5 | The Tab5 hub SHALL batch both global parameter changes and zone parameter changes using the same 50ms window. | Q2, Q13 |
| FR-3.6 | The Tab5 hub SHALL apply batching uniformly across all connected nodes (single broadcast decision per window). | Q11, Q15 |
| FR-4.1 | The Tab5 hub SHALL maintain a single authoritative HubState singleton instance at runtime. | Q12 |
| FR-4.2 | HubState SHALL store global parameters: effect, brightness, speed, palette, hue, intensity, saturation, complexity, variation. | Q12 |
| FR-4.3 | HubState SHALL store desired zone settings per node (keyed by nodeId) for at least 4 zones. | Q13 |
| FR-4.4 | HubState SHALL expose setter methods for global and zone parameters that mark dirty flags. | Q12 |
| FR-4.5 | HubState SHALL expose snapshot creation methods for (a) changed fields and (b) full snapshot. | Q14 |
| FR-4.6 | HubState SHALL not depend on WebSocket client objects; it SHALL remain transport-agnostic and hub-owned. | Q12, Q13 |
| FR-4.7 | ParameterHandler SHALL update HubState instead of sending network messages directly. | Q12 |
| FR-4.8 | HubState SHALL permit state restoration for late-joining nodes (apply full snapshot after join). | Q14 |
| FR-5.1 | After receiving a valid HELLO, the Tab5 hub SHALL respond with WELCOME. | Q1 |
| FR-5.2 | Immediately after WELCOME, the Tab5 hub SHALL send a full state snapshot to the joining node. | Q14 |
| FR-5.3 | The state snapshot SHALL include all global parameters stored in HubState. | Q14 |
| FR-5.4 | The state snapshot SHALL include the joining node’s per-zone settings (for each supported zone). | Q14 |
| FR-5.5 | The state snapshot SHALL include `applyAt_us` (or a per-field schedule) so nodes apply deterministically. | Q10, Q14 |
| FR-5.6 | The Tab5 hub SHALL send the snapshot after every HELLO→WELCOME, including reconnections. | Q14 |
| FR-6.1 | The Tab5 hub SHALL compute `applyAt_us = hub_clock_now_us() + LW_APPLY_AHEAD_US` for scheduled updates. | Q10 |
| FR-6.2 | The Tab5 hub SHALL use a single `applyAt_us` value per 50ms batch window for all broadcasts to all nodes. | Q10 |
| FR-6.3 | The Tab5 hub SHALL include `applyAt_us` in every broadcasted parameter/effect/zone command message. | Q10 |
| FR-6.4 | UDP BEAT_TICK packets SHALL include `applyAt_us` in the UDP header consistent with the hub clock. | Q10 |
| FR-6.5 | The Tab5 hub SHALL treat `hubNow_us` as the authoritative time base for scheduling. | Q10 |
| FR-6.6 | The Tab5 hub SHALL use the configured `LW_APPLY_AHEAD_US` constant (30ms) without hardcoding a different offset. | Q10 |
| FR-7.1 | All hub→node WebSocket broadcasts SHALL iterate nodes via `HubRegistry::forEachReady()`. | Q8 |
| FR-7.2 | The hub SHALL NOT broadcast commands to nodes that are not in READY state. | Q8 |
| FR-7.3 | The hub SHALL fan out to all READY nodes without requiring multicast or broadcast at the network layer. | Q15 |
| FR-7.4 | The hub SHALL handle 1–5 nodes as a typical fleet size while remaining correct up to the configured maximum. | Q15 |
| FR-8.1 | K1 nodes SHALL initiate the WebSocket connection to the Tab5 hub (node-initiated topology). | Q1 |
| FR-8.2 | The hub SHALL wait for HELLO before assigning nodeId and issuing WELCOME. | Q1 |
| FR-8.3 | The HELLO→WELCOME flow SHALL establish the session token used for subsequent WS and UDP validation. | Q1 |
| FR-8.4 | The hub SHALL treat the WELCOME token as hub-generated and authoritative for that session. | Q1 |
| FR-9.1 | The hub SHALL maintain node session state until WebSocket disconnect or keepalive timeout. | Q4 |
| FR-9.2 | The hub SHALL declare a node DEGRADED when keepalive and/or UDP silence thresholds are exceeded. | Q4 |
| FR-9.3 | The hub SHALL declare a node LOST when `LW_KEEPALIVE_TIMEOUT_MS` is exceeded and schedule cleanup after `LW_CLEANUP_TIMEOUT_MS`. | Q4 |
| FR-9.4 | The hub SHALL keep per-node tokens and session information isolated between nodes. | Q4 |
| FR-9.5 | The hub SHALL support reconnection of the same physical node without requiring an encoder interaction to restore state. | Q14 |
| FR-9.6 | The hub SHALL ensure late joiners converge to the current state via snapshot without disrupting other nodes. | Q14 |
| FR-10.1 | The hub SHALL support at least one node and up to five nodes simultaneously as the primary target fleet. | Q15 |
| FR-10.2 | The hub SHALL not require discovery protocols or multicast for the primary fleet size. | Q15 |
| FR-10.3 | The hub SHALL broadcast commands to all READY nodes with predictable per-node overhead (simple loop fanout). | Q15 |
| FR-10.4 | The hub SHALL preserve HubRegistry lifecycle semantics (register/ready/lost/cleanup) while refactoring protocol behaviour. | Q1, Q4 |
| NFR-1.1 | When audio is inactive, UDP fanout bandwidth SHALL drop to (near) zero packets per second. | Q9 |
| NFR-1.2 | When audio is active, UDP fanout SHALL remain bounded to 100Hz per hub regardless of encoder activity. | Q9 |
| NFR-1.3 | UDP packet size SHALL remain minimal (28-byte header + 4-byte BEAT_TICK payload, excluding IP/UDP overhead). | Q7 |
| NFR-2.1 | The hub SHALL keep perceived encoder-to-visual latency under 100ms for typical interactions. | Q2, Q11 |
| NFR-2.2 | WebSocket broadcast rate SHALL be throttled by design (50ms) to prevent encoder jitter storms. | Q11 |
| NFR-3.1 | All parameter changes SHALL travel via WebSocket to preserve ordering and reduce silent failure modes. | Q7 |
| NFR-3.2 | The hub SHALL prefer additive protocol changes (new fields) over breaking changes to message shapes. | Q5 |
| NFR-4.1 | Multi-node applications of the same parameter change SHALL converge within 10ms skew under normal RF conditions. | Q10 |
| NFR-4.2 | applyAt_us scheduling SHALL be deterministic and repeatable across nodes (same `applyAt_us` per batch). | Q10 |
| NFR-4.3 | Scheduling behaviour SHALL be resilient to WebSocket queueing and Wi-Fi jitter by using future timestamps. | Q10 |
| NFR-5.1 | The hub and node SHALL share `LW_PROTO_VER` and network constants from `common/proto/proto_constants.h`. | Q5 |
| NFR-5.2 | UDP message type and packet layout SHALL match `common/proto/udp_packets.h` exactly. | Q5 |
| NFR-5.3 | Session token hashing SHALL use `lw_token_hash32()` from `common/proto/udp_packets.h` (or an identical FNV-1a implementation). | Q5 |
| NFR-6.1 | The architecture SHALL remain correct for 1–5 nodes without introducing multicast complexity. | Q15 |
| NFR-6.2 | The hub SHALL keep per-node broadcast work linear in node count and bounded by `LW_MAX_NODES`. | Q15 |
| NFR-7.1 | All new comments and documentation added as part of this refactor SHALL use British spelling (centre, colour, synchronise, initialise, etc.). | Constraints |

## §5. User Stories & Acceptance Criteria

### Epic 1: Protocol Contract Migration

#### Story §5.1: Modern Command Broadcasting
**As a** firmware developer
**I want** Tab5 to broadcast only modern protocol commands (zones.*, colorCorrection.setConfig)
**So that** K1 nodes reliably receive and process commands without silent failures

**Acceptance Criteria:**
- [ ] AC-1 (§5.1.1): All zone commands use plural format (zones.setEffect, zones.setBrightness, zones.setSpeed, zones.setPalette, zones.setBlend)
- [ ] AC-2 (§5.1.2): Colour correction uses unified colorCorrection.setConfig instead of separate setGamma/setAutoExposure/setBrownGuardrail
- [ ] AC-3 (§5.1.3): WebSocketClient.cpp legacy client code is deprecated or removed
- [ ] AC-4 (§5.1.4): HubHttpWs implements broadcast methods (broadcastEffectChange, broadcastParameterChange, broadcastZoneCommand)

#### Story §5.2: UDP Audio-Only Refactor
**As a** system architect
**I want** UDP reserved exclusively for time-critical audio metrics
**So that** wireless bandwidth is optimized and RF contention is minimized

**Acceptance Criteria:**
- [ ] AC-5 (§5.2.1): hub_udp_fanout.cpp buildPacket() uses LW_UDP_BEAT_TICK message type ONLY
- [ ] AC-6 (§5.2.2): UDP packets contain BPM, phase, and beat flags from audio analysis
- [ ] AC-7 (§5.2.3): UDP transmission occurs ONLY when audio is active (hasActiveAudioMetrics())
- [ ] AC-8 (§5.2.4): PARAM_DELTA broadcasting via UDP is completely removed
- [ ] AC-9 (§5.2.5): UDP BEAT_TICK packet fields use network byte order and correct sizes (28-byte header + 4-byte payload)

### Epic 2: Centralized State Management

#### Story §5.3: HubState Singleton Implementation
**As a** firmware developer
**I want** centralized authoritative state for all parameters
**So that** reconnecting nodes receive consistent state and multi-node broadcasts are synchronized

**Acceptance Criteria:**
- [ ] AC-10 (§5.3.1): HubState singleton class stores global parameters (effect, brightness, speed, palette, hue, intensity, saturation, complexity, variation)
- [ ] AC-11 (§5.3.2): HubState stores desired zone settings per zoneId (effect, brightness, speed, palette, blend mode)
- [ ] AC-12 (§5.3.3): ParameterHandler calls HubState setters instead of WebSocketClient methods
- [ ] AC-13 (§5.3.4): HubState triggers WebSocket broadcasts via HubHttpWs on parameter changes

#### Story §5.4: State Resynchronization on Reconnect
**As a** system integrator
**I want** nodes to immediately receive current state after reconnection
**So that** nodes display correct effects without waiting for user interaction

**Acceptance Criteria:**
- [ ] AC-14 (§5.4.1): HubHttpWs implements sendStateSnapshot() method using existing async WS patterns
- [ ] AC-15 (§5.4.2): State snapshot is sent immediately after WELCOME handshake completes
- [ ] AC-16 (§5.4.3): Snapshot includes all global parameters and zone settings
- [ ] AC-17 (§5.4.4): Nodes apply snapshot with deterministic applyAt_us timestamp

### Epic 3: Batched WebSocket Broadcasting

#### Story §5.5: 50ms Parameter Batching
**As a** firmware developer
**I want** parameter changes batched every 50ms before broadcasting
**So that** encoder jitter storms don't spam WebSocket messages

**Acceptance Criteria:**
- [ ] AC-18 (§5.5.1): Parameter change batching mechanism matches existing 50ms throttle in ParameterHandler
- [ ] AC-19 (§5.5.2): HubState accumulates dirty flags for changed parameters
- [ ] AC-20 (§5.5.3): Timer tick triggers broadcast of accumulated changes every 50ms
- [ ] AC-21 (§5.5.4): WebSocket load is proportional to human interaction, not encoder edge rate

#### Story §5.6: Scheduled Multi-Node Synchronization
**As a** system integrator
**I want** parameter broadcasts scheduled with applyAt_us timestamps
**So that** all nodes apply changes synchronously despite Wi-Fi jitter

**Acceptance Criteria:**
- [ ] AC-22 (§5.6.1): All WebSocket parameter broadcasts include applyAt_us = hubNow_us + LW_APPLY_AHEAD_US (30ms)
- [ ] AC-23 (§5.6.2): Broadcast uses HubRegistry::forEachReady() to iterate all connected nodes
- [ ] AC-24 (§5.6.3): Nodes receive commands within same beat window (verified in Phase 2 testing)
- [ ] AC-25 (§5.6.4): Deterministic timing prevents per-node skew from immediate application
- [ ] AC-26 (§5.6.5): A single applyAt_us value is used per batch and is identical across all nodes for the same broadcast window

## §6. User Experience Contract
*→ Extracted into Blueprint §1.5/1.6 User Journey diagrams*

### §6.1 User Visibility Specification

| ID | User Action | User Sees (Exact) | User Does NOT See | Timing |
|----|-------------|-------------------|-------------------|--------|
| V-1 | Power on K1 node | Node LED status indicator shows connection progress | HELLO/WELCOME handshake, token generation, state machine transitions | 2-5 seconds to READY state |
| V-2 | Turn encoder on Tab5 | All connected K1 nodes update effect/parameter synchronously | WebSocket batching, 50ms throttle, applyAt_us scheduling | <100ms perceived latency |
| V-3 | K1 node reconnects after WiFi dropout | Node resumes correct effect immediately upon reconnection | State snapshot transmission, PENDING→AUTHED→READY transitions | 3-7 seconds to restore state |
| V-4 | Audio music playing | LED effects react to beat/BPM across all nodes synchronously | UDP BEAT_TICK packets, audio metric extraction | <30ms audio-to-visual latency |
| V-5 | K1 node disconnects | Tab5 dashboard shows node as LOST after timeout | Session cleanup delay (LW_CLEANUP_TIMEOUT_MS = 10s) | 10-13 seconds to reflect LOST state |

### §6.2 Timing & Feedback Expectations

| ID | Event | Expected Timing | User Feedback | Failure Feedback |
|----|-------|-----------------|---------------|------------------|
| T-1 | Node boot to READY | 2-5 seconds | Dashboard shows READY state, node displays effect | Dashboard shows LOST/timeout error after 13s |
| T-2 | Encoder change to visual update | <100ms | Synchronized effect change across all nodes | Individual node may lag if RF congested |
| T-3 | Audio beat to visual reaction | <30ms | Beat-synchronized LED pulse/flash | Silent (UDP best-effort, no retries) |
| T-4 | Node reconnect to state restored | 3-7 seconds | Node displays current effect from hub | Node displays default/last-known until state snapshot received |
| T-5 | Parameter batch accumulation | 50ms window | Multiple rapid encoder changes appear as single update | (Internal, user sees final result) |

## §7. Artifact Ownership
*→ Extracted into Blueprint §2 System Boundaries*

### §7.1 Creation Responsibility

| ID | Artifact | Created By | When | App's Role |
|----|----------|------------|------|------------|
| O-1 | Session token (64-char string) | Tab5 hub | During WELCOME response after HELLO received | Create via HubRegistry::generateToken(), store in node_entry_t |
| O-2 | Token hash (32-bit FNV-1a) | Tab5 hub | Immediately after token generation | Create via lw_token_hash32(), include in UDP packets for validation |
| O-3 | Node state machine transitions | Tab5 hub | Throughout session lifecycle (PENDING→AUTHED→READY→LOST) | Create via HubRegistry methods (registerNode, markReady, markLost) |
| O-4 | WebSocket WELCOME message | Tab5 hub | After validating HELLO message | Create with nodeId, token, udpPort, hubEpoch_us |
| O-5 | WebSocket command broadcasts | Tab5 hub | After 50ms batch timer tick | Create via HubHttpWs broadcast methods iterating forEachReady() |
| O-6 | UDP BEAT_TICK packets | Tab5 hub | At 100Hz when audio is active | Create via HubUdpFanout::buildBeatPacket() with AudioMetrics |
| O-7 | State snapshot after reconnect | Tab5 hub | Immediately after WELCOME handshake | Create via HubHttpWs::sendStateSnapshot() with current HubState |

### §7.2 External System Dependencies

| ID | External System | What It Creates | How App Knows | Failure Handling |
|----|-----------------|-----------------|---------------|------------------|
| E-1 | K1 nodes | HELLO messages (MAC, firmware version, capabilities, topology) | WebSocket onWsEvent receives data | Timeout after LW_KEEPALIVE_TIMEOUT_MS (3.5s), transition to LOST |
| E-2 | K1 nodes | KEEPALIVE messages (RSSI, packet loss, drift, uptime) | WebSocket receives every LW_KEEPALIVE_PERIOD_MS (1s) | Missing keepalive triggers DEGRADED→LOST transition |
| E-3 | K1 nodes | TS_PING messages (sequence, t1 timestamp) | Dedicated UDP port 49154 receives ping | Hub sends PONG regardless, time sync lock failure tracked per-node |
| E-4 | K1 nodes | OTA_STATUS messages (state, progress percentage) | WebSocket receives during firmware update | Timeout after LW_OTA_NODE_TIMEOUT_S (180s), abort OTA |
| E-5 | Audio processor (future) | AudioMetrics (BPM, phase, energy, beat flags) | hasActiveAudioMetrics() returns true, getLatestAudioMetrics() | UDP fanout simply doesn't send when audio inactive |

### §7.3 Derived Ownership Rules
*These become Blueprint §2.3 Boundary Rules*

| Source | Rule | Rationale |
|--------|------|-----------|
| O-1, O-2 | Tab5 hub MUST NOT use tokens/hashes created by nodes | Hub is authoritative source for session authentication |
| E-1 | Tab5 hub MUST wait for HELLO before assigning nodeId | Node identity established by node's self-identification |
| O-3 | Tab5 hub MUST NOT allow nodes to self-transition states | State machine integrity requires centralized control |
| E-2 | Tab5 hub MUST NOT mark node READY without receiving KEEPALIVE | READY state requires proof of bidirectional communication |
| O-6, E-5 | Tab5 hub MUST NOT send UDP when audio processor inactive | UDP bandwidth reserved exclusively for active audio metrics |
| O-7, E-1 | Tab5 hub MUST send state snapshot after every HELLO→WELCOME | Prevents "reconnected node displays wrong effect" bug |

## §8. State Requirements
*→ Extracted into Blueprint §3 State Transition Specifications*

### §8.1 State Isolation

| ID | Requirement | Rationale | Enforcement |
|----|-------------|-----------|-------------|
| SI-1 | Global parameters (effect/brightness/speed) SHALL apply to all nodes via broadcast | Ensures consistent look across multi-node installations | HubState stores single global parameter set, broadcasts trigger forEachReady() |
| SI-2 | Zone parameters SHALL be stored per-node in HubState | Nodes may have different zone configurations | HubState maintains map<nodeId, ZoneSettings[4]> |
| SI-3 | Node session state SHALL be isolated in HubRegistry | Prevents cross-node authentication issues | Each node_entry_t has unique nodeId, token, tokenHash |
| SI-4 | UDP audio metrics SHALL be broadcast to all nodes identically | Synchronized beat tracking requires same BPM/phase | HubUdpFanout sends identical BEAT_TICK to all nodes |

### §8.2 State Lifecycle

| ID | State | Initial | Created When | Cleared When | Persists Across |
|----|-------|---------|--------------|--------------|------------------|
| SL-1 | Node session (node_entry_t) | Empty | HELLO received, registerNode() called | WS disconnect OR keepalive timeout + cleanup delay | Node reconnections (tracked via MAC) |
| SL-2 | Session token | Random 64-char | WELCOME generation | Node state transitions to LOST, cleanupLostNodes() | Session lifetime only |
| SL-3 | HubState global parameters | Boot defaults | Tab5 boot OR first parameter change | Never (runtime persistent) | Tab5 reboot resets to defaults |
| SL-4 | HubState zone settings | Per-node defaults | First zone command for nodeId | Never (runtime persistent) | Node disconnect/reconnect (hub remembers) |
| SL-5 | Parameter dirty flags | Clean (false) | Any HubState setter called | Every 50ms batch timer tick | Encoder changes within 50ms window |
| SL-6 | Audio metrics | Silent (inactive) | Audio processor detects beat | Audio silence detected | Active audio periods only |
| SL-7 | Node state machine | PENDING | HELLO received | Transition to LOST → cleanup | PENDING→AUTHED→READY→DEGRADED→LOST |

## §9. Technical Considerations
*→ Informs Blueprint §5, §6, §9, §11*

### §9.1 Architecture Decisions

**Decision 1: WebSocket Control Plane + UDP Audio-Only Dual-Plane Architecture**
- **Rationale**: Separates reliable ordered commands (WebSocket) from time-critical best-effort metrics (UDP). WebSocket provides JSON flexibility for complex commands, UDP provides minimal latency for beat tracking. Matches existing common/proto/ design.
- **Trade-offs**: Complexity of maintaining two transport layers vs. bandwidth optimization and synchronization quality.

**Decision 2: HubState Singleton as Single Source of Truth**
- **Rationale**: Centralized state prevents inconsistencies between what Tab5 "thinks" parameters are and what nodes display. Enables state snapshot on reconnect and late-join synchronization.
- **Trade-offs**: Memory overhead storing zone settings per-node vs. robustness against reconnection bugs.

**Decision 3: 50ms Batching with Scheduled applyAt_us Timestamps**
- **Rationale**: Matches existing ParameterHandler throttle for consistent UX. Scheduled timestamps prevent Wi-Fi jitter from causing multi-node desynchronization.
- **Trade-offs**: 50ms latency vs. reduced message spam and deterministic synchronization.

**Decision 4: Node-Initiated Session (HELLO→WELCOME Handshake)**
- **Rationale**: Natural for embedded Wi-Fi topology, matches existing HubRegistry lifecycle, avoids hub network scanning complexity.
- **Trade-offs**: Nodes must know hub IP (typically fixed 192.168.4.1) vs. hub discovery flexibility.

### §9.2 Integration Points

**Integration with ParameterHandler**
- Current: ParameterHandler::sendParameterChange() calls WebSocketClient methods directly
- Refactored: ParameterHandler calls HubState setters (setGlobalEffect, setGlobalBrightness, etc.)
- HubState triggers HubHttpWs broadcast methods
- File: `Tab5.encoder/src/parameters/ParameterHandler.cpp:155-202`

**Integration with hub_udp_fanout**
- Current: hub_udp_fanout.cpp buildPacket() sends LW_UDP_PARAM_DELTA with effect/palette/brightness/speed
- Refactored: buildBeatPacket() sends LW_UDP_BEAT_TICK with AudioMetrics (BPM, phase, flags)
- Only sends when hasActiveAudioMetrics() returns true
- File: `Tab5.encoder/src/hub/net/hub_udp_fanout.cpp:91-110`

**Integration with hub_http_ws**
- Current: HubHttpWs has sendWelcome/sendTsPong/sendOtaUpdate for individual messages
- Refactored: Add broadcast methods (broadcastEffectChange, broadcastParameterChange, broadcastZoneCommand, sendStateSnapshot)
- Uses HubRegistry::forEachReady() iterator pattern
- File: `Tab5.encoder/src/hub/net/hub_http_ws.cpp`

**Integration with common/proto**
- All message formats strictly follow common/proto/ definitions
- WebSocket: ws_messages.h (lw_msg_hello_t, lw_msg_welcome_t, lw_msg_keepalive_t)
- UDP: udp_packets.h (lw_udp_hdr_t, lw_udp_beat_tick_t)
- Protocol constants: proto_constants.h (LW_PROTO_VER, ports, timeouts)
- Files: `K1.LightwaveOS/common/proto/`

### §9.3 Constraints

- **UDP Bandwidth**: MUST be minimal, audio metrics only (no parameters)
- **British English**: All code comments and documentation MUST use British spelling
- **Fleet Size**: Architecture MUST support 1-5 K1 nodes without multicast complexity
- **Protocol Contract**: MUST strictly follow common/proto/ as single source of truth
- **Backward Compatibility**: NO support for legacy zone.* singular commands (K1 nodes don't register handlers)
- **Time Sync**: MUST use dedicated UDP port 49154 separate from main UDP stream
- **FreeRTOS Core Assignment**: Visual pipeline MUST run on Core1 (Core0 dedicated to audio processing)

## §10. Success Metrics
*→ Informs Blueprint §10 Testing Strategy*

| ID | Metric | Target | Measurement Method |
|----|--------|--------|--------------------|
| M-1 | WebSocket command correctness | 100% modern protocol (zones.*, colorCorrection.setConfig) | Code inspection + packet capture verification |
| M-2 | UDP bandwidth utilization | >90% reduction when audio inactive | Packet capture comparing before/after refactor |
| M-3 | Multi-node synchronization accuracy | All nodes within same beat window (<10ms skew) | Phase 2 testing with 2-3 nodes + scope capture |
| M-4 | Reconnection state consistency | Node displays correct effect <3s after WELCOME | Phase 1 testing: disconnect/reconnect cycles |
| M-5 | Encoder responsiveness | <100ms perceived latency from encoder turn to visual update | Phase 1 testing: user interaction feel |
| M-6 | Fleet scalability | 5 nodes stable for >1 hour continuous operation | Phase 3 testing: 8-node stress test (exceeds target) |
| M-7 | Audio-to-visual latency | <30ms from beat detection to LED reaction | Phase 2 testing with audio analysis + scope |
| M-8 | Session lifecycle robustness | Zero reconnection bugs (wrong effects) in 100 reconnect cycles | Phase 1/2 regression testing |

---

## Appendix A: Answer Traceability

| User Answer (Summary) | Captured In |
|-----------------------|-------------|
| Q1: Node-initiated connection (HELLO→WELCOME) | §9.1 Decision 4, §7.1 O-1/O-4, §8.2 SL-1, FR-8 |
| Q2: Batch WebSocket changes every 50ms | §5.5, §9.1 Decision 3, §8.2 SL-5, FR-3 |
| Q3: Only support new zones.* commands | §5.1.1, §9.3 Constraints, FR-1 |
| Q4: Keep session alive until timeout | §5.9, §8.2 SL-1, FR-9 |
| Q5: Strict common/proto/ adherence | §9.2 Integration, §9.3 Constraints, FR-5, NFR-5 |
| Q6: New command format only (zones.*, colorCorrection) | §5.1, FR-1, M-1 |
| Q7: UDP audio metrics only (BEAT_TICK) | §5.2, §7.1 O-6, §9.2 Integration, FR-2, NFR-3 |
| Q8: Use forEachReady() for broadcast | §9.2 Integration HubHttpWs, FR-7 |
| Q9: UDP only when audio active | §5.2.3, §7.2 E-5, §8.2 SL-6, FR-2, NFR-1 |
| Q10: Scheduled applyAt_us for synchronization | §5.6, §9.1 Decision 3, FR-6, NFR-4 |
| Q11: Batch every 50ms matching throttle | §5.5, §9.1 Decision 3, FR-3, NFR-2 |
| Q12: HubState singleton for central state | §5.3, §9.1 Decision 2, §8.2 SL-3/SL-4, FR-4 |
| Q13: Hub stores zone settings, broadcasts updates | §5.3.2, §8.1 SI-2, §8.2 SL-4, FR-4 |
| Q14: Resend state immediately after WELCOME | §5.4, §7.1 O-7, §6.1 V-3, FR-5, M-4 |
| Q15: Fleet size 1-5 nodes typical | §9.3 Constraints, FR-10, M-6 |
| Q16: Phased testing (1→3→8 nodes), no backward compat | §10 Success Metrics, §9.3 Constraints |

**Validation:** ✅ All 16 user answers have been captured across §1-§10. No information lost.
