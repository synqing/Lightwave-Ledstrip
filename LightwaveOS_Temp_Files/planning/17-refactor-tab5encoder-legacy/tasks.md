# ⚠️ DEPRECATED ⚠️
> **This plan is OBSOLETE. Please refer to `planning/17-context-engineer-input/tasks.md` for the active "Legacy Parity Recovery" plan.**
> **The assumption that "K1 Node (Reference - No Changes Required)" was proven FALSE by forensic audit.**

# Task List: Tab5 Hub Protocol Refactor

## Quick Reference (Extracted from PRD & Blueprint)

### Ownership Rules
*Source: PRD §7 + Blueprint §2.1*

| Artifact | Created By | App's Role | DO NOT |
|----------|------------|------------|--------|
| Session token (64-char string) | Tab5 hub | CREATE via HubRegistry::generateToken() | Accept tokens created by nodes (enables spoofing) |
| Token hash (32-bit FNV-1a) | Tab5 hub | CREATE via lw_token_hash32() | Use hashes from nodes |
| Node state machine transitions | Tab5 hub | CREATE via HubRegistry methods | Allow nodes to self-transition states |
| WebSocket WELCOME message | Tab5 hub | CREATE after HELLO validation | Send WELCOME before validating HELLO |
| WebSocket command broadcasts | Tab5 hub | CREATE via HubHttpWs broadcast methods | Send to non-READY nodes |
| UDP BEAT_TICK packets | Tab5 hub | CREATE via HubUdpFanout::buildBeatPacket() | Send when audio inactive |
| State snapshot after reconnect | Tab5 hub | CREATE via HubHttpWs::sendStateSnapshot() | Skip state snapshot after HELLO→WELCOME |
| HELLO messages | K1 nodes | OBSERVE and respond with WELCOME | Hub initiates connection |
| KEEPALIVE messages | K1 nodes | OBSERVE and update metrics | Mark READY without receiving KEEPALIVE |
| TS_PING messages | K1 nodes | OBSERVE and respond with TS_PONG | (Existing, no changes) |
| OTA_STATUS messages | K1 nodes | OBSERVE and track progress | (Existing, no changes) |
| AudioMetrics | Audio processor (future) | OBSERVE via hasActiveAudioMetrics() | Create fake metrics when audio inactive |

### State Variables
*Source: PRD §8 + Blueprint §3*

| State Variable | Initial Value | Created When | Cleared When | Persists Across |
|----------------|---------------|--------------|--------------|------------------|
| Node session (node_entry_t) | Empty | HELLO received, registerNode() called | WS disconnect OR keepalive timeout + cleanup delay | Node reconnections (tracked via MAC) |
| Session token | Random 64-char | WELCOME generation | Node state transitions to LOST, cleanupLostNodes() | Session lifetime only |
| HubState global parameters | Boot defaults | Tab5 boot OR first parameter change | Never (runtime persistent) | Tab5 reboot resets to defaults |
| HubState zone settings | Per-node defaults | First zone command for nodeId | Never (runtime persistent) | Node disconnect/reconnect (hub remembers) |
| Parameter dirty flags | Clean (false) | Any HubState setter called | Every 50ms batch timer tick | Encoder changes within 50ms window |
| Audio metrics | Silent (inactive) | Audio processor detects beat | Audio silence detected | Active audio periods only |
| Node state machine | PENDING | HELLO received | Transition to LOST → cleanup | PENDING→AUTHED→READY→DEGRADED→LOST |

### Critical Boundaries
*Source: Blueprint §2.3*

- ❌ DO NOT: Accept tokens/hashes created by nodes (Hub is authoritative for session authentication)
- ❌ DO NOT: Assign nodeId before receiving HELLO (Node identity established by self-identification)
- ❌ DO NOT: Allow nodes to self-transition states (State machine integrity requires centralized control)
- ❌ DO NOT: Mark node READY without receiving KEEPALIVE (READY state requires proof of bidirectional communication)
- ❌ DO NOT: Send UDP when audio processor inactive (UDP bandwidth reserved exclusively for active audio metrics)
- ❌ DO NOT: Skip state snapshot after HELLO→WELCOME (Prevents "reconnected node displays wrong effect" bug)
- ❌ DO NOT: Send parameters via UDP (Parameters moved to WebSocket for reliability and ordering)
- ❌ DO NOT: Use legacy zone.* singular commands (K1 nodes only register zones.* plural handlers)
- ❌ DO NOT: Use separate color correction commands (K1 expects unified colorCorrection.setConfig)
- ❌ DO NOT: Bypass HubState for parameter changes (HubState is single source of truth)
- ❌ DO NOT: Send broadcasts to non-READY nodes (forEachReady() enforces this)
- ✅ MUST: Use scheduled applyAt_us timestamps for all parameter broadcasts (Prevents multi-node timing skew)
- ✅ MUST: Batch parameter changes every 50ms (Prevents encoder jitter storms spamming WebSocket)
- ✅ MUST: Send state snapshot immediately after WELCOME (Prevents stale state on reconnection)
- ✅ MUST: Iterate HubRegistry::forEachReady() for all broadcasts (Ensures only READY nodes receive)
- ✅ MUST: Calculate applyAt_us = hub_clock_now_us() + LW_APPLY_AHEAD_US (30ms lookahead for synchronization)
- ✅ MUST: Use exact protocol from common/proto/ (Shared protocol is single source of truth)

### User Visibility Rules
*Source: PRD §6*

| User Action | User Sees | User Does NOT See | Timing |
|-------------|-----------|-------------------|--------|
| Power on K1 node | Node LED status indicator shows connection progress | HELLO/WELCOME handshake, token generation, state machine transitions | 2-5 seconds to READY state |
| Turn encoder on Tab5 | All connected K1 nodes update effect/parameter synchronously | WebSocket batching, 50ms throttle, applyAt_us scheduling | <100ms perceived latency |
| K1 node reconnects after WiFi dropout | Node resumes correct effect immediately upon reconnection | State snapshot transmission, PENDING→AUTHED→READY transitions | 3-7 seconds to restore state |
| Audio music playing | LED effects react to beat/BPM across all nodes synchronously | UDP BEAT_TICK packets, audio metric extraction | <30ms audio-to-visual latency |
| K1 node disconnects | Tab5 dashboard shows node as LOST after timeout | Session cleanup delay (LW_CLEANUP_TIMEOUT_MS = 10s) | 10-13 seconds to reflect LOST state |

---

## Requirements Traceability

**Every requirement MUST map to at least one task. Nothing should be lost.**

| Source | Requirement | Mapped To Task |
|--------|-------------|----------------|
| FR-1 (PRD §4.1) | Tab5 hub broadcasts modern protocol commands only (zones.*, colorCorrection.setConfig) | Task 1.3, 1.4 |
| FR-2 (PRD §4.1) | UDP is reserved exclusively for BEAT_TICK audio metrics | Task 2.3, 2.4, 2.5, 2.6 |
| FR-3 (PRD §4.1) | Batch WebSocket parameter changes every 50ms | Task 1.6 |
| FR-4 (PRD §4.1) | HubState singleton stores authoritative global/zone state | Task 1.2, 1.5 |
| FR-5 (PRD §4.1) | Send a full state snapshot immediately after WELCOME | Task 1.7 |
| FR-6 (PRD §4.1) | Scheduled applyAt_us timestamps (30ms ahead) for synchronised multi-node apply | Task 1.4, 1.8 |
| FR-7 (PRD §4.1) | Broadcast via HubRegistry::forEachReady() fanout | Task 1.4, 2.4 |
| FR-8 (PRD §4.1) | Node-initiated HELLO→WELCOME handshake | Task 1.7 (existing), IC-1 |
| FR-9 (PRD §4.1) | Session lifecycle maintained until disconnect/timeout | (Existing HubRegistry) + verification in Task 3.2 |
| FR-10 (PRD §4.1) | Supports 1–5 nodes without multicast complexity | Verification in Task 3.4 |
| NFR-1 (PRD §4.2) | UDP bandwidth minimal; no packets when audio is inactive | Task 2.4 + verification in Task 3.5 |
| NFR-2 (PRD §4.2) | WebSocket message rate proportional to human interaction (50ms throttle) | Task 1.6 |
| NFR-3 (PRD §4.2) | Parameters use WebSocket ordering/reliability (not UDP deltas) | Task 1.4, 2.5 |
| NFR-4 (PRD §4.2) | Deterministic synchronisation via applyAt_us scheduling | Task 1.8 + verification in Task 3.3 |
| NFR-5 (PRD §4.2) | Strict adherence to `common/proto/` wire contracts | Task 1.1 + verification in Task 3.5 |
| NFR-6 (PRD §4.2) | Scales to 5 nodes via simple fanout | Verification in Task 3.4 |
| NFR-7 (PRD §4.2) | British English in new comments/docs | Task 4.7 + checklist |
| AC-1 (PRD §5.1.1) | All zone commands use plural format (zones.*) | Task 1.3, 1.4 |
| AC-2 (PRD §5.1.2) | Colour correction uses unified colorCorrection.setConfig | Task 1.3 |
| AC-3 (PRD §5.1.3) | WebSocketClient.cpp legacy client code deprecated/removed | Task 4.2 |
| AC-4 (PRD §5.1.4) | HubHttpWs implements broadcast methods | Task 1.4 |
| AC-5 (PRD §5.2.1) | hub_udp_fanout.cpp uses LW_UDP_BEAT_TICK ONLY | Task 2.3 |
| AC-6 (PRD §5.2.2) | UDP packets contain BPM, phase, beat flags | Task 2.2, 2.3 |
| AC-7 (PRD §5.2.3) | UDP transmission occurs ONLY when audio is active | Task 2.4 |
| AC-8 (PRD §5.2.4) | PARAM_DELTA broadcasting via UDP completely removed | Task 2.5 |
| AC-9 (PRD §5.2.5) | UDP uses network byte order + correct packet sizing | Task 2.6 |
| AC-10 (PRD §5.3.1) | HubState stores global parameters | Task 1.2 |
| AC-11 (PRD §5.3.2) | HubState stores zone settings (per zoneId) | Task 1.2 |
| AC-12 (PRD §5.3.3) | ParameterHandler calls HubState setters | Task 1.5 |
| AC-13 (PRD §5.3.4) | HubState triggers WebSocket broadcasts | Task 1.2, 1.4, 1.6 |
| AC-14 (PRD §5.4.1) | HubHttpWs implements sendStateSnapshot() | Task 1.7 |
| AC-15 (PRD §5.4.2) | Snapshot sent immediately after WELCOME | Task 1.7, IC-1 |
| AC-16 (PRD §5.4.3) | Snapshot includes all global + zone settings | Task 1.2, 1.7 |
| AC-17 (PRD §5.4.4) | Nodes apply snapshot with deterministic applyAt_us | Task 1.7 (K1 side existing) |
| AC-18 (PRD §5.5.1) | 50ms batching matches existing throttle | Task 1.6 |
| AC-19 (PRD §5.5.2) | HubState accumulates dirty flags | Task 1.2, 1.6 |
| AC-20 (PRD §5.5.3) | Timer tick triggers broadcast every 50ms | Task 1.6 |
| AC-21 (PRD §5.5.4) | WebSocket load proportional to human interaction | Task 1.6 |
| AC-22 (PRD §5.6.1) | Broadcasts include applyAt_us timestamps | Task 1.4, 1.8 |
| AC-23 (PRD §5.6.2) | Broadcast uses forEachReady() | Task 1.4 |
| AC-24 (PRD §5.6.3) | Nodes receive commands within same beat window | Verification in Task 3.3 |
| AC-25 (PRD §5.6.4) | Deterministic timing prevents per-node skew | Task 1.8 + verification in Task 3.3 |
| AC-26 (PRD §5.6.5) | Single applyAt_us per batch (identical across nodes) | Task 1.8 |
| V-1 (PRD §6.1) | Power on K1 node: visible connection progress | Verification in Task 3.2 |
| V-2 (PRD §6.1) | Turn encoder: all nodes update synchronously | Task 1.5 + verification in Task 3.2 |
| V-3 (PRD §6.1) | Node reconnects: correct effect restored without encoder touch | Task 1.7 + verification in Task 3.2 |
| V-4 (PRD §6.1) | Audio playing: beat reaction across nodes | Task 2.4 + verification in Task 3.3 |
| V-5 (PRD §6.1) | Node disconnects: dashboard shows LOST after timeout | (Existing HubRegistry) + verification in Task 3.2 |
| T-1 (PRD §6.2) | Node boot to READY: 2–5 seconds | Verification in Task 3.2 |
| T-2 (PRD §6.2) | Encoder change to visual update: <100ms | Verification in Task 3.2 |
| T-3 (PRD §6.2) | Audio beat to visual reaction: <30ms | Verification in Task 3.3 |
| T-4 (PRD §6.2) | Node reconnect to state restored: 3–7 seconds | Verification in Task 3.2 |
| T-5 (PRD §6.2) | Parameter batch accumulation: 50ms window | Task 1.6 + verification in Task 3.2 |
| O-1 (PRD §7.1) | Session token created by Tab5 | Task 1.7 (existing HubRegistry) |
| O-2 (PRD §7.1) | Token hash created by Tab5 | Task 1.7 (existing) |
| O-3 (PRD §7.1) | Node state machine transitions created by Tab5 | Task 1.7 (existing) |
| O-4 (PRD §7.1) | WebSocket WELCOME message created by Tab5 | Task 1.7 (existing sendWelcome) |
| O-5 (PRD §7.1) | WebSocket command broadcasts created by Tab5 | Task 1.4 |
| O-6 (PRD §7.1) | UDP BEAT_TICK packets created by Tab5 | Task 2.3, 2.4 |
| O-7 (PRD §7.1) | State snapshot created by Tab5 | Task 1.7, IC-1 |
| E-1 (PRD §7.2) | K1 nodes create HELLO messages | (Existing, no changes) |
| E-2 (PRD §7.2) | K1 nodes create KEEPALIVE messages | (Existing, no changes) |
| E-3 (PRD §7.2) | K1 nodes create TS_PING messages | (Existing, no changes) |
| E-4 (PRD §7.2) | K1 nodes create OTA_STATUS messages | (Existing, no changes) |
| E-5 (PRD §7.2) | Audio processor creates AudioMetrics | Task 2.2 |

---

## Overview

This task list implements the Tab5 Hub Protocol Refactor, transforming Tab5.encoder from a legacy WebSocket client to a modern LightwaveOS v2 hub server. The refactor eliminates protocol mismatches (zone.* → zones.*), reserves UDP exclusively for time-critical audio metrics (BEAT_TICK), and introduces centralized state management (HubState singleton) ensuring consistent parameter synchronization across 1-5 K1 nodes. Key outcomes: clean protocol separation (WebSocket control, UDP audio-only), deterministic multi-node synchronization (scheduled applyAt_us timestamps), and robust session lifecycle with automatic state resynchronization on reconnection. See PRD sections §1-§10 and Blueprint sections §1-§11 for complete requirements and technical specifications.

## Relevant Files

**Tab5 Hub (Primary Changes):**
- `Tab5.encoder/src/hub/state/HubState.h` – **NEW** - Singleton storing authoritative state
- `Tab5.encoder/src/hub/state/HubState.cpp` – **NEW** - Implementation with dirty flags
- `Tab5.encoder/src/hub/net/hub_http_ws.h` – **MODIFY** - Add broadcast methods
- `Tab5.encoder/src/hub/net/hub_http_ws.cpp` – **MODIFY** - Implement broadcasts + state snapshot
- `Tab5.encoder/src/hub/net/hub_udp_fanout.h` – **MODIFY** - Add audio metrics interface
- `Tab5.encoder/src/hub/net/hub_udp_fanout.cpp` – **MODIFY** - Refactor buildPacket to BEAT_TICK only
- `Tab5.encoder/src/hub/net/hub_registry.h` – **EXISTING** - Reference for session management
- `Tab5.encoder/src/hub/net/hub_registry.cpp` – **EXISTING** - Reference for forEachReady
- `Tab5.encoder/src/hub/hub_main.h` – **MODIFY** - Add 50ms batch timer
- `Tab5.encoder/src/hub/hub_main.cpp` – **MODIFY** - Integrate HubState + timer tick
- `Tab5.encoder/src/parameters/ParameterHandler.h` – **MODIFY** - Remove WebSocketClient dependency
- `Tab5.encoder/src/parameters/ParameterHandler.cpp` – **MODIFY** - Use HubState setters instead
- `Tab5.encoder/src/audio/AudioMetrics.h` – **NEW** - Audio processor interface (Phase 2)
- `Tab5.encoder/src/network/WebSocketClient.h` – **DEPRECATE** - Mark deprecated
- `Tab5.encoder/src/network/WebSocketClient.cpp` – **DEPRECATE** - Remove or mark deprecated

**K1 Node (Reference - No Changes Required):**
- `K1.LightwaveOS/common/proto/proto_constants.h` – **EXISTING** - Shared protocol constants
- `K1.LightwaveOS/common/proto/ws_messages.h` – **EXISTING** - WebSocket message structures
- `K1.LightwaveOS/common/proto/udp_packets.h` – **EXISTING** - UDP packet structures
- `K1.LightwaveS3/src/network/webserver/ws/WsZonesCommands.cpp` – **EXISTING** - zones.* handlers
- `K1.LightwaveS3/src/network/webserver/ws/WsColorCommands.cpp` – **EXISTING** - colorCorrection.setConfig handler

**Testing:**
- `Tab5.encoder/test/test_hubstate.cpp` – **NEW** - HubState unit tests
- `Tab5.encoder/test/test_integration.cpp` – **NEW** - Multi-node integration tests

**Documentation:**
- `planning/17-refactor-tab5encoder-legacy/prd.md` – **EXISTING** - Product Requirements
- `planning/17-refactor-tab5encoder-legacy/technical_blueprint.md` – **EXISTING** - Technical Specifications
- `ARCHITECTURE.md` – **UPDATE** - Hub/node architecture documentation

---

## Tasks

### 1.0 Protocol Alignment & HubState Implementation

**Pre-condition:**
- Tab5 has existing hub infrastructure (HubRegistry, HubHttpWs, HubUdpFanout) from previous work
- ParameterHandler currently calls WebSocketClient methods (legacy client mode)
- No centralized state management (parameters scattered across classes)
- Zone commands use singular "zone.*" format

#### Sub-Tasks:

- [ ] 1.1 Review context: PRD §5.1 (Modern Commands), §5.3 (HubState), §5.5 (Batching), Blueprint §2 (Boundaries), §3 (State Transitions), §6 (Data Models)
  - **Relevant Sections:** PRD §4.1 FR-4 (HubState requirement), §8.2 SL-3/SL-4/SL-5 (state lifecycle), Blueprint §6.1 (HubState data model with dirty flags)
  - **Key Decisions:** HubState is single source of truth for all parameters; dirty flag mechanism enables 50ms batching; global parameters stored once, zone parameters stored per-nodeId
  - **Watch Out For:** §2.3 boundary rule "MUST NOT bypass HubState for parameter changes"; SL-3 "never cleared until Tab5 reboot"; SL-5 "dirty flags cleared every 50ms"

- [ ] 1.2 Create HubState singleton class (`Tab5.encoder/src/hub/state/HubState.h` and `HubState.cpp`)
  - **Input:** None (creates new singleton)
  - **Output:** HubState class with global parameter storage, per-node zone storage, dirty flag mechanism
  - **Ownership:** Tab5 hub CREATES authoritative state storage (PRD §7 O-5, §8 SI-1)
  - **State Change:** Creates new state storage (SL-3, SL-4, SL-5) initialized to boot defaults
  - **Implements:** PRD §5.3.1, §5.3.2, Blueprint §6.1
  - **Specific Actions:**
    - Define private member variables for 9 global parameters (effect, brightness, speed, palette, hue, intensity, saturation, complexity, variation)
    - Define `std::map<uint8_t, std::array<ZoneSettings, 4>> m_zoneSettings` for per-node zone storage
    - Define DirtyFlags bitfield structure (9 bits for global params)
    - Implement setGlobalEffect/Brightness/Speed/Palette/Hue/Intensity/Saturation/Complexity/Variation methods setting dirty flags
    - Implement setZoneEffect/Brightness/Speed/Palette/BlendMode(nodeId, zoneId, value) methods
    - Implement checkDirtyFlags() returning bool
    - Implement createSnapshot() returning ParameterSnapshot with changed params
    - Implement createFullSnapshot(nodeId) returning StateSnapshot with global + zones
    - Implement clearDirtyFlags() called after broadcast
    - Initialize global parameters to defaults in constructor

- [ ] 1.3 Update all command names to modern protocol (update HubHttpWs method signatures)
  - **Input:** Existing HubHttpWs class structure
  - **Output:** Method signatures using modern command names (zones.*, colorCorrection.setConfig)
  - **Implements:** PRD §5.1.1, §5.1.2, Blueprint §7.1
  - **Specific Actions:**
    - Plan broadcast method names: broadcastEffectChange, broadcastParameterChange, broadcastZoneCommand
    - Zone commands will use "zones.setEffect", "zones.setBrightness", "zones.setSpeed", "zones.setPalette", "zones.setBlend" (plural)
    - Color correction will use single "colorCorrection.setConfig" with unified parameters (gamma, autoExposure, brownGuardrail)
    - Note: Implementation in Task 1.4

- [ ] 1.4 Add broadcast methods to HubHttpWs (`Tab5.encoder/src/hub/net/hub_http_ws.cpp`)
  - **Input:** HubState snapshot (from createSnapshot), HubRegistry::forEachReady() iterator
  - **Output:** WebSocket JSON messages sent to all READY nodes with applyAt_us timestamps
  - **Ownership:** Tab5 hub CREATES command broadcasts (PRD §7.1 O-5)
  - **User Sees:** All connected K1 nodes update synchronously (<100ms from encoder turn per PRD §6.1 V-2)
  - **State Change:** No state change (broadcasts state from HubState)
  - **Integration:** Iterates HubRegistry::forEachReady(), calls AsyncWebSocket::text() per node
  - **Implements:** PRD §5.1.4, §5.6.1, §5.6.2, Blueprint §4.1
  - **Specific Actions:**
    - Implement `broadcastEffectChange(uint8_t effectId)`: JSON type "effects.setCurrent", include applyAt_us
    - Implement `broadcastParameterChange(ParameterSnapshot& snapshot)`: JSON type "parameters.set", include changed params + applyAt_us
    - Implement `broadcastZoneCommand(uint8_t zoneId, uint8_t effectId, const char* commandType)`: JSON type "zones.setEffect"|"zones.setBrightness"|etc., include applyAt_us
    - ALL broadcasts calculate: `uint64_t applyAt_us = hub_clock_now_us() + LW_APPLY_AHEAD_US` (30ms lookahead per §2.3 MUST rule)
    - ALL broadcasts use: `HubRegistry::forEachReady([&](node_entry_t* node) { AsyncWebSocket::text(node->client, json); })`
    - Enforce §2.3 boundary rule: "MUST use scheduled applyAt_us timestamps" and "MUST iterate forEachReady"

- [ ] 1.5 Refactor ParameterHandler to use HubState (`Tab5.encoder/src/parameters/ParameterHandler.cpp`)
  - **Input:** Encoder input events (brightness change, speed change, etc.)
  - **Output:** HubState setter calls (setGlobalBrightness, setGlobalSpeed, etc.)
  - **User Sees:** Tab5 dashboard updates immediately to show new value; all K1 nodes update within 100ms (PRD §6.1 V-2)
  - **State Change:** Sets HubState parameters (SL-3) and dirty flags (SL-5)
  - **Implements:** PRD §5.3.3, §5.3.4, Blueprint §3.3
  - **Specific Actions:**
    - Replace `WebSocketClient::sendBrightnessChange(value)` with `HubState::getInstance().setGlobalBrightness(value)`
    - Replace `WebSocketClient::sendSpeedChange(value)` with `HubState::getInstance().setGlobalSpeed(value)`
    - Replace `WebSocketClient::sendPaletteChange(value)` with `HubState::getInstance().setGlobalPalette(value)`
    - Replace `WebSocketClient::sendEffectChange(value)` with `HubState::getInstance().setGlobalEffect(value)`
    - Replace `WebSocketClient::sendZoneEffect(zoneId, effectId)` with `HubState::getInstance().setZoneEffect(nodeId, zoneId, effectId)` (note: nodeId may be "all" or current selection)
    - Remove all `#include "network/WebSocketClient.h"` references
    - Verify no direct WebSocket sends remain
    - Enforce §2.3 boundary rule: "MUST NOT bypass HubState for parameter changes"

- [ ] 1.6 Add 50ms batch timer to HubMain (`Tab5.encoder/src/hub/hub_main.cpp`)
  - **Input:** 50ms timer tick event
  - **Output:** HubHttpWs broadcast calls if any parameters changed
  - **State Change:** Clears dirty flags (SL-5) after broadcast
  - **Implements:** PRD §5.5.1, §5.5.2, §5.5.3, §5.5.4, Blueprint §3.4
  - **Specific Actions:**
    - Add `unsigned long lastBatchTime_ms = 0` to HubMain state
    - In HubMain::loop() or tick(), check: `if (millis() - lastBatchTime_ms >= 50)`
    - If true, call `HubState::getInstance().checkDirtyFlags()`
    - If dirty, call `ParameterSnapshot snapshot = HubState::getInstance().createSnapshot()`
    - Call `HubHttpWs::getInstance().broadcastParameterChange(snapshot)`
    - Call `HubState::getInstance().clearDirtyFlags()`
    - Update `lastBatchTime_ms = millis()`
    - Enforce §2.3 MUST rule: "Batch parameter changes every 50ms"

- [ ] 1.7 Implement state snapshot on reconnection (`HubHttpWs::handleHello` modification)
  - **Input:** HELLO message from K1 node (includes MAC, firmware version, capabilities, topology)
  - **Output:** WELCOME message + state snapshot sent to reconnecting node
  - **Ownership:** Tab5 hub CREATES state snapshot (PRD §7.1 O-7)
  - **User Sees:** Node resumes correct effect immediately upon reconnection (3-7s total, PRD §6.1 V-3)
  - **State Change:** No state change (sends snapshot of existing state from SL-3, SL-4)
  - **Integration:** Calls existing HubRegistry::registerNode(), sendWelcome(); NEW calls to HubState::createFullSnapshot() and sendStateSnapshot()
  - **Implements:** PRD §5.4.1, §5.4.2, §5.4.3, §5.4.4, Blueprint §3.6, §4.2
  - **Specific Actions:**
    - Locate existing `HubHttpWs::handleHello(client, helloMessage)` method
    - After existing `sendWelcome(client, nodeId)` call, add:
      ```cpp
      StateSnapshot snapshot = HubState::getInstance().createFullSnapshot(nodeId);
      sendStateSnapshot(client, snapshot);
      ```
    - Implement `sendStateSnapshot(AsyncWebSocketClient* client, StateSnapshot& snapshot)`:
      - Create JSON: `{"type": "state.snapshot", "global": {...}, "zones": [...]}`
      - Include all 9 global parameters from snapshot
      - Include all 4 zone settings for this nodeId
      - Include applyAt_us timestamp (scheduled: hub_clock_now_us() + LW_APPLY_AHEAD_US)
      - Send via `client->text(json)`
    - Enforce §2.3 MUST rule: "Send state snapshot after every HELLO→WELCOME"
    - Enforce §2.3 DO NOT: "Skip state snapshot after HELLO→WELCOME"

- [ ] 1.8 Enforce applyAt_us scheduling contract (single timestamp per batch)
  - **Input:** 50ms batch tick, per-command broadcast paths
  - **Output:** Consistent applyAt_us usage for all broadcasts (same value across all nodes for the same batch)
  - **User Sees:** Multi-node effects remain synchronised despite Wi-Fi jitter (<10ms skew target, PRD §10 M-3)
  - **State Change:** No state change (timing metadata only)
  - **Integration:** Shared helper used by HubHttpWs broadcasts and state snapshot send path
  - **Implements:** PRD §5.6.1, §5.6.5, Blueprint §4.1
  - **Specific Actions:**
    - Compute `applyAt_us = hub_clock_now_us() + LW_APPLY_AHEAD_US` once per batch tick
    - Pass the computed applyAt_us into all broadcast helpers (do not recompute per node)
    - Ensure state snapshot also includes applyAt_us using the same scheduling constant
    - Verify packet capture: 2–3 nodes receive identical applyAt_us for the same broadcast window

**Post-condition:**
- HubState singleton exists with global + per-node zone storage
- HubHttpWs has broadcast methods using modern command names (zones.*, colorCorrection.setConfig)
- ParameterHandler calls HubState setters, not WebSocketClient
- 50ms batch timer accumulates changes and triggers broadcasts
- State snapshot sent after every HELLO→WELCOME handshake
- All WebSocket broadcasts include scheduled applyAt_us timestamps

**Verification:**
- [ ] Single K1 node boots → receives WELCOME + state snapshot within 5s (T-1)
- [ ] Turn Tab5 encoder → node visual update within 100ms (T-2)
- [ ] Packet capture shows "zones.setEffect" (plural), not "zone.setEffect" (M-1)
- [ ] Node disconnect/reconnect → correct effect displayed within 3-7s without encoder touch (M-4, V-3)
- [ ] HubState unit tests pass: global parameter storage, zone parameter isolation, dirty flag mechanism, snapshot creation

---

### 2.0 UDP Audio-Only Refactor

**Pre-condition:**
- HubUdpFanout exists with legacy buildPacket() sending LW_UDP_PARAM_DELTA
- UDP currently sends parameters (effect, palette, brightness, speed) at 100Hz regardless of changes
- No audio processor integration (future integration point)

#### Sub-Tasks:

- [ ] 2.1 Review context: PRD §5.2 (UDP Audio-Only), §7.2 E-5 (AudioMetrics), Blueprint §4.3 (UDP Fanout Sequence), §6.3 (UDP Data Models)
  - **Relevant Sections:** PRD §4.1 FR-2 (UDP audio-only requirement), §8.2 SL-6 (audio metrics lifecycle), Blueprint §2.3 boundary rules for UDP
  - **Key Decisions:** UDP reserved exclusively for BEAT_TICK audio metrics; hasActiveAudioMetrics() gate prevents transmission when silent; AudioMetrics interface defined for future audio processor integration
  - **Watch Out For:** §2.3 DO NOT "Send UDP when audio processor inactive"; §2.3 DO NOT "Send parameters via UDP"; SL-6 "Audio metrics cleared when silence detected"

- [ ] 2.2 Create AudioMetrics interface (`Tab5.encoder/src/audio/AudioMetrics.h` - NEW file)
  - **Input:** None (defines interface for future audio processor)
  - **Output:** AudioMetrics.h with structure and stub implementation
  - **Ownership:** Tab5 hub will OBSERVE audio metrics from external audio processor (PRD §7.2 E-5)
  - **State Change:** Defines structure for SL-6 audio metrics state
  - **Implements:** PRD §5.2.2, Blueprint §6.3, E-5
  - **Specific Actions:**
    - Define struct `AudioMetrics { uint16_t bpm_x100; uint8_t phase; uint8_t flags; }`
    - Define `bool hasActiveAudioMetrics()` returning false (stub until audio processor integrated)
    - Define `AudioMetrics getLatestAudioMetrics()` returning zeroed struct (stub)
    - Add comments: "// TODO Phase 2: Integrate with audio processor"
    - Note: This is a stub for Phase 1; real implementation in Phase 2 after audio processor available

- [ ] 2.3 Refactor HubUdpFanout::buildPacket() to BEAT_TICK only (`Tab5.encoder/src/hub/net/hub_udp_fanout.cpp`)
  - **Input:** AudioMetrics from getLatestAudioMetrics()
  - **Output:** UDP packet with lw_udp_hdr_t (msgType=LW_UDP_BEAT_TICK) + lw_udp_beat_tick_t payload
  - **Ownership:** Tab5 hub CREATES UDP BEAT_TICK packets (PRD §7.1 O-6)
  - **User Sees:** LED effects react to beat/BPM across all nodes synchronously (<30ms latency, PRD §6.1 V-4)
  - **State Change:** No state change (broadcasts audio metrics when available per SL-6)
  - **Implements:** PRD §5.2.1, §5.2.2, §5.2.4, Blueprint §6.3
  - **Specific Actions:**
    - Locate existing `buildPacket()` method (currently lines 91-110 in hub_udp_fanout.cpp)
    - Change `lw_udp_hdr_t.msgType` from `LW_UDP_PARAM_DELTA` to `LW_UDP_BEAT_TICK`
    - Remove payload struct `lw_udp_param_delta_t` (effectId, paletteId, brightness, speed)
    - Replace with payload struct `lw_udp_beat_tick_t` from common/proto/udp_packets.h
    - Set payload fields from AudioMetrics: `payload.bpm_x100 = metrics.bpm_x100`, `payload.phase = metrics.phase`, `payload.flags = metrics.flags`
    - Update `payloadLen` in header to `sizeof(lw_udp_beat_tick_t)` (4 bytes vs. previous 8 bytes)
    - Verify packet total size: 28 bytes (header) + 4 bytes (payload) = 32 bytes
    - Enforce §2.3 DO NOT: "Send parameters via UDP"

- [ ] 2.4 Add audio activity check to HubUdpFanout::tick()
  - **Input:** 100Hz tick event, hasActiveAudioMetrics() query
  - **Output:** UDP packets sent ONLY when audio active; return early when silent
  - **User Sees:** LED effects react to music; NO unnecessary network traffic when silent (PRD §6.1 V-4)
  - **State Change:** No state change (queries SL-6 audio metrics state)
  - **Implements:** PRD §5.2.3, §4.2 NFR-1, Blueprint §4.3
  - **Specific Actions:**
    - Add at start of `tick()` method: `if (!hasActiveAudioMetrics()) { return; }`
    - Verify this enforces §2.3 MUST rule: "Only send UDP when audio active"
    - Verify this enforces §2.3 DO NOT: "Send UDP when audio processor inactive"
    - Result: UDP transmission stops immediately when audio silence detected (no airtime waste per NFR-1)

- [ ] 2.5 Remove PARAM_DELTA usage entirely (codebase cleanup)
  - **Input:** Codebase search results
  - **Output:** All PARAM_DELTA references removed or commented as deprecated
  - **Implements:** PRD §5.2.4
  - **Specific Actions:**
    - Run: `grep -r "LW_UDP_PARAM_DELTA" Tab5.encoder/src/`
    - Run: `grep -r "lw_udp_param_delta_t" Tab5.encoder/src/`
    - Verify no remaining code paths create PARAM_DELTA packets
    - Update comments: "// DEPRECATED: PARAM_DELTA removed per protocol refactor; parameters use WebSocket"
    - Note: K1 nodes may still have PARAM_DELTA handling code (backward compat), but Tab5 hub MUST NOT send

- [ ] 2.6 Validate UDP BEAT_TICK byte order and sizing
  - **Input:** Updated HubUdpFanout BEAT_TICK packet build path
  - **Output:** Confirmed BEAT_TICK header/payload sizes and network byte order correctness
  - **User Sees:** No visible change (prevents subtle drift/jitter bugs caused by malformed packets)
  - **State Change:** No state change (validation and correctness)
  - **Implements:** PRD §5.2.5 (AC-9), NFR-5
  - **Specific Actions:**
    - Verify `lw_udp_hdr_t` size is exactly 28 bytes and `lw_udp_beat_tick_t` is 4 bytes (per `common/proto/udp_packets.h`)
    - Ensure the hub uses the provided `lw_udp_hdr_hton()` helper (or equivalent) before sending
    - Verify `payloadLen == sizeof(lw_udp_beat_tick_t)` and packet total size is 32 bytes
    - Packet capture: confirm `msgType == LW_UDP_BEAT_TICK` and header fields decode correctly when using `lw_udp_hdr_ntoh()`

**Post-condition:**
- AudioMetrics interface defined (stub implementation for Phase 1)
- HubUdpFanout::buildPacket() creates LW_UDP_BEAT_TICK packets with BPM/phase/flags
- HubUdpFanout::tick() returns early if !hasActiveAudioMetrics()
- No PARAM_DELTA packets sent by Tab5 hub
- UDP bandwidth minimal when audio inactive (>90% reduction target)

**Verification:**
- [ ] 2-3 K1 nodes connected
- [ ] Audio stub returns inactive → UDP transmission stops (packet capture shows 0 packets)
- [ ] Mock hasActiveAudioMetrics() to return true → UDP packets sent at 100Hz
- [ ] Packet capture shows msgType=LW_UDP_BEAT_TICK (not PARAM_DELTA) (M-1)
- [ ] Turn encoder → NO UDP sent (parameters via WebSocket only, verifies §2.3 boundary)
- [ ] Packet capture during 10s window: 0 UDP packets when audio inactive (M-2 >90% reduction verified)

---

### 3.0 Testing & Validation

**Pre-condition:**
- Phase 1 complete: HubState implemented, modern protocol commands, state snapshot on reconnection
- Phase 2 complete: UDP audio-only refactor, PARAM_DELTA removed
- Tab5 firmware built and ready to flash
- K1 node firmware supports modern protocol (zones.*, colorCorrection.setConfig handlers registered)

#### Sub-Tasks:

- [ ] 3.1 Review context: PRD §10 (Success Metrics), Blueprint §10 (Testing Strategy), §8 (Implementation Phases)
  - **Relevant Sections:** PRD §5 acceptance criteria, §6 timing expectations, §10 metrics M-1 through M-8
  - **Key Decisions:** Phased rollout (1 node → 2-3 nodes → 8 nodes); zero tolerance for reconnection bugs (M-8); strict timing requirements (T-1 through T-5)
  - **Watch Out For:** Test MUST verify all acceptance criteria map to PRD §5.X.Y; timing measurements require oscilloscope/packet capture, not just visual inspection

- [ ] 3.2 Single-node end-to-end testing (Phase 1 validation)
  - **Input:** 1 Tab5 hub + 1 K1 node, test script or manual testing
  - **Output:** Test results verifying all Phase 1 acceptance criteria
  - **User Sees:** Node boots, connects, displays effects, reconnects correctly (V-1, V-2, V-3)
  - **Implements:** PRD §5.1 through §5.6 acceptance criteria, M-4, M-5, M-8
  - **Specific Actions:**
    - **Test 1: Node boot to READY (T-1, V-1)**
      - Power cycle K1 node
      - Measure time from boot to READY state (target: 2-5s)
      - Verify dashboard shows READY status
      - Verify node displays correct effect from state snapshot
    - **Test 2: Encoder responsiveness (T-2, V-2, M-5)**
      - Turn Tab5 brightness encoder
      - Measure time to visual update on K1 node (target: <100ms)
      - Verify user perceives immediate response
    - **Test 3: Zone command modern protocol (§5.1.1, M-1)**
      - Trigger zone effect change
      - Packet capture: verify JSON contains "zones.setEffect" (plural), not "zone.setEffect"
    - **Test 4: Reconnection state restoration (§5.4, M-4, V-3, T-4)**
      - Set specific effect/brightness on K1 node
      - Disconnect K1 WiFi (e.g., power cycle router or disable K1 WiFi)
      - Wait 5s, reconnect K1
      - Verify node displays correct effect within 3-7s WITHOUT touching encoder
      - Verify packet capture shows HELLO → WELCOME → state.snapshot
    - **Test 5: 100 reconnection cycles (M-8)**
      - Automated script: disconnect WiFi, wait 5s, reconnect; repeat 100x
      - Verify: zero "wrong effect displayed" bugs
      - Verify: effect correctness after every reconnection

- [ ] 3.3 Multi-node synchronization testing (Phase 2 validation with 2-3 nodes)
  - **Input:** 1 Tab5 hub + 2-3 K1 nodes, oscilloscope or high-speed camera
  - **Output:** Test results verifying multi-node synchronization within <10ms skew
  - **User Sees:** All nodes update effects synchronously (V-2)
  - **Implements:** PRD §5.6 acceptance criteria, M-3, M-7
  - **Specific Actions:**
    - **Test 1: Parameter broadcast synchronization (M-3, §5.6.3)**
      - Connect 2-3 K1 nodes to Tab5 hub
      - Turn Tab5 brightness encoder
      - Oscilloscope measurement: connect to LED outputs of all nodes
      - Measure time delta between brightness changes across nodes (target: <10ms skew)
      - Packet capture: verify all nodes receive message with SAME applyAt_us timestamp
      - Verify scheduled timing prevents Wi-Fi jitter from causing desynchronization
    - **Test 2: Audio beat tracking synchronization (M-7, T-3, V-4)**
      - Mock hasActiveAudioMetrics() to return true with test BPM (e.g., 120.00 = 12000)
      - Mock phase incrementing 0-255 per beat
      - Verify all nodes receive identical UDP BEAT_TICK packets (packet capture)
      - Oscilloscope: measure audio tone trigger to LED reaction (target: <30ms)
      - Verify synchronized beat pulse across all nodes (<10ms skew)

- [ ] 3.4 Stress testing (Phase 3 validation with 8 nodes, exceeds target of 5)
  - **Input:** 1 Tab5 hub + 8 K1 nodes, continuous operation script
  - **Output:** Stability metrics over >1 hour, RF contention analysis
  - **User Sees:** All nodes remain responsive, no crashes or disconnections (M-6)
  - **Implements:** PRD FR-10 (fleet scalability), M-6, NFR-6
  - **Specific Actions:**
    - **Test 1: Fleet scalability (M-6)**
      - Connect 8 K1 nodes simultaneously (exceeds target of 5 per FR-10)
      - Run continuous operation: encoder changes every 5s + mock audio every 10s
      - Monitor for 60+ minutes
      - Verify: no crashes, no disconnections, all nodes remain READY
      - Verify: HubRegistry::forEachReady() handles 8 nodes without performance degradation
    - **Test 2: RF contention monitoring**
      - Monitor RSSI, packet loss, drift metrics from KEEPALIVE messages
      - Verify: acceptable RF contention (RSSI > -70 dBm, packet loss <5%)
      - Identify any nodes entering DEGRADED state due to high drift or packet loss
    - **Test 3: Rapid encoder changes (§5.5.4, NFR-2)**
      - Turn encoder rapidly (10 changes in <50ms)
      - Packet capture: verify broadcasts throttled to 50ms intervals (not spamming)
      - Verify: WebSocket message rate proportional to human interaction, not encoder edge rate

- [ ] 3.5 Protocol compliance verification (M-1, M-2)
  - **Input:** Wireshark packet capture from all previous tests
  - **Output:** Compliance report confirming 100% modern protocol usage
  - **Implements:** PRD M-1 (protocol correctness), M-2 (UDP bandwidth reduction), Q3 (zones.* only), Q6 (modern format only), Q7 (UDP audio-only)
  - **Specific Actions:**
    - **Test 1: Modern command names (M-1)**
      - Wireshark filter: `websocket.payload.text contains "zone.setEffect"`
      - Expected result: 0 matches (legacy singular format MUST NOT appear)
      - Wireshark filter: `websocket.payload.text contains "zones.setEffect"`
      - Expected result: >0 matches (modern plural format verified)
      - Wireshark filter: `websocket.payload.text contains "colorCorrection.setConfig"`
      - Expected result: >0 matches if color correction tested (unified command verified)
    - **Test 2: UDP bandwidth reduction (M-2)**
      - Packet capture 10s window with audio inactive (hasActiveAudioMetrics = false)
      - Expected: 0 UDP packets sent (>90% reduction from legacy 100 packets/s = 1000 packets/10s)
      - Compare to legacy: packet capture with PARAM_DELTA would show 1000 packets in 10s
      - Verify: >90% bandwidth reduction achieved
    - **Test 3: UDP audio-only compliance (Q7)**
      - Packet capture filter: `udp.port == 49152`
      - Inspect all UDP packets: verify msgType=LW_UDP_BEAT_TICK (0x02), NOT LW_UDP_PARAM_DELTA (0x01)
      - Verify payload structure: lw_udp_beat_tick_t (4 bytes: bpm_x100, phase, flags)
      - Verify: no parameters (effect/palette/brightness/speed) in UDP payload

- [ ] 3.6 Automated reconnection regression (M-8)
  - **Input:** Single Tab5 hub + 1 K1 node, automated Wi-Fi toggle/reconnect harness
  - **Output:** 100 reconnection cycles with zero "wrong effect" failures
  - **User Sees:** Node always resumes correct effect after reconnect without encoder touch
  - **State Change:** Repeated node session lifecycle (SL-1/SL-7) exercised; HubState remains authoritative (SL-3/SL-4)
  - **Implements:** PRD M-8, V-3, T-4
  - **Specific Actions:**
    - Automate 100 cycles: disconnect node Wi-Fi → wait LOST → reconnect → observe WELCOME → observe state.snapshot
    - Assert the node’s displayed effect matches hub desired state within 3–7 seconds every time
    - Capture failures with timestamped logs + packet capture snippets (HELLO→WELCOME→state.snapshot sequence)

**Post-condition:**
- All PRD acceptance criteria verified (§5.1 through §5.6)
- All PRD success metrics achieved (M-1 through M-8)
- Protocol compliance confirmed (100% modern commands, UDP audio-only)
- Multi-node synchronization verified (<10ms skew)
- Fleet scalability demonstrated (8 nodes stable >1 hour, exceeds target)

**Verification:**
- [ ] M-1: 100% modern protocol commands (packet capture)
- [ ] M-2: >90% UDP bandwidth reduction when audio inactive
- [ ] M-3: Multi-node skew <10ms (oscilloscope)
- [ ] M-4: Node reconnection restores state <3s (stopwatch)
- [ ] M-5: Encoder responsiveness <100ms (human perception)
- [ ] M-6: 5 nodes stable >1 hour (exceeds with 8-node test)
- [ ] M-7: Audio-to-visual latency <30ms (oscilloscope)
- [ ] M-8: Zero reconnection bugs in 100 cycles (automated script)

---

### 4.0 Cleanup & Documentation

**Pre-condition:**
- All Phase 1, 2, 3 testing complete and passing
- Tab5 firmware deployed to production units
- K1 nodes operating correctly with modern protocol

#### Sub-Tasks:

- [ ] 4.1 Review context: PRD §9 (Technical Considerations), Blueprint §11 (Deployment)
  - **Relevant Sections:** Code cleanup requirements, British English constraint, documentation updates
  - **Key Decisions:** WebSocketClient.cpp deprecated (not removed, for safety); ARCHITECTURE.md updated with hub/node refactor details
  - **Watch Out For:** British English spelling throughout (centre, colour, synchronise per NFR-7)

- [ ] 4.2 Deprecate WebSocketClient.cpp legacy client code
  - **Input:** WebSocketClient.cpp and WebSocketClient.h files
  - **Output:** Files marked deprecated, removed from build, or commented with deprecation notices
  - **Implements:** PRD §5.1.3
  - **Specific Actions:**
    - Add deprecation comments at top of files:
      ```cpp
      // DEPRECATED: This legacy WebSocket client is no longer used.
      // Tab5 now operates as a hub server using HubHttpWs for broadcasts.
      // Replaced by: HubState + HubHttpWs + HubRegistry
      // Refactor completed: 2026-01-13
      ```
    - Verify `platformio.ini` already excludes from build: `build_src_filter = -<network/WebSocketClient.cpp>`
    - Do NOT delete files yet (keep for reference/rollback safety)
    - Remove `#include "network/WebSocketClient.h"` from all source files (verify with grep)

- [ ] 4.3 Update architecture documentation (`ARCHITECTURE.md`)
  - **Input:** Existing ARCHITECTURE.md, PRD, Blueprint
  - **Output:** Updated ARCHITECTURE.md with hub/node refactor details
  - **Implements:** Documentation requirement
  - **Specific Actions:**
    - Add section: "Hub/Node Architecture"
      - Describe Tab5 as hub server, K1 as nodes (client-initiated sessions)
      - Explain dual-plane architecture: WebSocket control plane + UDP audio-only stream plane
      - Diagram: Tab5 → WebSocket broadcasts → K1 nodes (1-5)
      - Diagram: Tab5 → UDP BEAT_TICK (100Hz when audio active) → K1 nodes
    - Add section: "Protocol Contracts"
      - Modern command naming: zones.* (plural), colorCorrection.setConfig (unified)
      - Legacy commands deprecated: zone.* (singular), separate colorCorrection commands
    - Add section: "State Management"
      - HubState singleton as single source of truth
      - 50ms parameter batching mechanism
      - State snapshot on reconnection (prevents stale state bug)
    - Add section: "Synchronization"
      - applyAt_us scheduled timestamps (30ms lookahead)
      - Multi-node deterministic rendering
    - Ensure British English throughout: centre, colour, synchronise

- [ ] 4.4 Update API documentation with modern command names
  - **Input:** Existing API docs (if any), Blueprint §7 API Specifications
  - **Output:** API docs reflecting modern protocol
  - **Implements:** Documentation requirement
  - **Specific Actions:**
    - Document WebSocket commands sent by Tab5 hub:
      - `zones.setEffect`, `zones.setBrightness`, `zones.setSpeed`, `zones.setPalette`, `zones.setBlend`
      - `colorCorrection.setConfig` (gamma, autoExposure, brownGuardrail unified)
      - `parameters.set` (global brightness/speed/palette with applyAt_us)
      - `effects.setCurrent` (global effect change with applyAt_us)
      - `state.snapshot` (full state resync after reconnection)
    - Document UDP stream: LW_UDP_BEAT_TICK only (BPM, phase, flags)
    - Mark deprecated: zone.* singular, PARAM_DELTA

- [ ] 4.5 Add state snapshot reconnection flow diagram to docs
  - **Input:** Blueprint §1.6 User Journey diagram
  - **Output:** Documentation diagram showing HELLO → WELCOME → state.snapshot flow
  - **Implements:** Documentation requirement
  - **Specific Actions:**
    - Create diagram: Node boot/reconnect → HELLO → Tab5 validates → WELCOME (token) → state.snapshot (global + zones) → Node applies → READY
    - Highlight: state snapshot prevents "wrong effect" bug
    - Timing annotations: 2-5s boot to READY, 3-7s reconnect to state restored

- [ ] 4.6 Document UDP audio-only constraint
  - **Input:** PRD §5.2, Blueprint §2.3 boundary rules
  - **Output:** Clear documentation of UDP usage policy
  - **Implements:** Documentation requirement
  - **Specific Actions:**
    - Document: "UDP port 49152 reserved EXCLUSIVELY for audio metrics (BEAT_TICK message type)"
    - Document: "Parameters (brightness/speed/palette/effect) use WebSocket control plane for reliability and ordering"
    - Document: "UDP transmission conditional on hasActiveAudioMetrics() to minimize RF contention"
    - Document: "PARAM_DELTA deprecated; legacy K1 nodes may still handle, but Tab5 hub MUST NOT send"

- [ ] 4.7 Code review and final cleanup
  - **Input:** All modified source files
  - **Output:** Clean, well-commented code adhering to constraints
  - **Implements:** Code quality requirement (NFR-7)
  - **Specific Actions:**
    - British English spelling review: grep for "center", "color", "synchronize"; replace with "centre", "colour", "synchronise"
    - Remove dead code paths:
      - Old zone.* singular command handling (if any in Tab5)
      - Old PARAM_DELTA packet creation (already removed in Task 2.3)
    - Ensure all comments reference PRD/Blueprint sections where relevant: "// Per PRD §5.4.2: state snapshot after WELCOME"
    - Run linter/formatter if available: `pio run -t format` or similar
    - Verify build: `cd firmware/Tab5.encoder && pio run -e tab5`

**Post-condition:**
- WebSocketClient.cpp marked deprecated, removed from build
- ARCHITECTURE.md updated with hub/node refactor, modern protocol, state management
- API documentation reflects modern command names (zones.*, colorCorrection.setConfig)
- State snapshot reconnection flow documented
- UDP audio-only constraint documented
- Codebase cleaned, British English enforced, dead code removed
- Final build passes

**Verification:**
- [ ] Grep `"center"` returns 0 matches (British "centre" used)
- [ ] Grep `"color"` returns 0 matches (British "colour" used)
- [ ] Grep `"synchronize"` returns 0 matches (British "synchronise" used)
- [ ] Grep `"zone\.setEffect"` (singular) in Tab5 code returns 0 matches
- [ ] ARCHITECTURE.md includes hub/node section, protocol section, synchronization section
- [ ] Build completes: `pio run -e tab5` exits 0

---

## Integration-Critical Tasks
*Source: Blueprint §4 - Integration Wiring*

These tasks have specific wiring requirements that must be followed exactly. Deviating from the specified sequence can cause bugs.

### IC-1: State Snapshot on Reconnection Sequence
*Maps to: Blueprint §4.2, PRD §5.4, §7.1 O-7, §8.2 SL-1/SL-3/SL-4*

**Critical Sequence:**
```
1. HubRegistry::registerNode(hello_data)  // REQUIRED FIRST: Creates session (SL-1), assigns nodeId, generates token (O-1)
2. HubHttpWs::sendWelcome(client, nodeId)  // REQUIRED SECOND: Sends WELCOME with nodeId, token, udpPort, hubEpoch_us (O-4)
3. HubState::createFullSnapshot(nodeId)  // REQUIRED THIRD: Gathers current authoritative state (SL-3, SL-4)
4. HubHttpWs::sendStateSnapshot(client, snapshot)  // REQUIRED FOURTH: Transmits snapshot to node (O-7)
```

**Ownership Rules (from PRD §7):**
- Session token created by Tab5 hub during registerNode() — Nodes must NOT create tokens (prevents spoofing)
- State snapshot created by Tab5 hub — Nodes RECEIVE and apply (hub is authoritative)
- Node creates HELLO message — Tab5 hub must WAIT for HELLO before assigning nodeId

**User Visibility (from PRD §6):**
- User sees: Node resumes correct effect immediately upon reconnection (3-7s total per V-3)
- User does NOT see: HELLO/WELCOME handshake, token generation, state machine transitions (V-3)

**State Changes (from Blueprint §3.1, §3.6):**
- Before: Previous node_entry_t may be in LOST state or already cleaned up; HubState contains current authoritative parameters (SL-3, SL-4)
- After: New node_entry_t created with state=PENDING (SL-7), session token generated (SL-2), state snapshot sent containing all global + zone parameters

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Skipping state snapshot after WELCOME**: Causes "reconnected node displays wrong effect" bug. Node uses stale/default state until encoder touched. (Violates O-7 boundary rule)
- ❌ **Sending state snapshot before WELCOME**: Node doesn't have session credentials yet, will reject snapshot. (Violates sequence order)
- ❌ **Using node-provided token instead of generating new one**: Enables session spoofing attacks. Hub is authoritative for tokens. (Violates O-1 boundary rule)
- ❌ **Assigning nodeId before receiving HELLO**: Causes nodeId conflicts, prevents MAC-based tracking. (Violates E-1 boundary rule)

**Verification:**
- [ ] Disconnect K1 node WiFi, reconnect → node displays correct effect within 3-7s without encoder touch
- [ ] Packet capture shows sequence: HELLO → WELCOME → state.snapshot
- [ ] State snapshot JSON includes all 9 global parameters + 4 zone settings
- [ ] 100 reconnection cycles → zero "wrong effect" bugs (M-8)

---

### IC-2: Parameter Change Broadcast Sequence
*Maps to: Blueprint §4.1, PRD §5.3, §5.5, §5.6, §8.2 SL-3/SL-5*

**Critical Sequence:**
```
1. HubState::setGlobal*(value)  // REQUIRED FIRST: Updates authoritative state (SL-3), sets dirty flag (SL-5)
2. (Wait 50ms batch window)  // REQUIRED DELAY: Accumulates multiple encoder changes
3. HubState::checkDirtyFlags()  // REQUIRED THIRD: Checks if any parameters changed
4. HubState::createSnapshot()  // REQUIRED FOURTH: Gathers changed parameters into snapshot
5. hub_clock_now_us() + LW_APPLY_AHEAD_US  // REQUIRED FIFTH: Calculate applyAt_us = hubNow + 30ms
6. HubRegistry::forEachReady(callback)  // REQUIRED SIXTH: Iterate all READY nodes only
7. AsyncWebSocket::text(json)  // REQUIRED SEVENTH: Send JSON to each READY node
8. HubState::clearDirtyFlags()  // REQUIRED EIGHTH: Clear dirty flags after broadcast
```

**Ownership Rules (from PRD §7):**
- HubState stores authoritative parameters — ParameterHandler must NOT bypass HubState (SI-1)
- Command broadcasts created by Tab5 hub — Nodes RECEIVE and apply
- Dirty flags managed by HubState — No external clearing until after broadcast

**User Visibility (from PRD §6):**
- User sees: All connected K1 nodes update synchronously (<100ms from encoder turn per V-2)
- User does NOT see: WebSocket batching, 50ms throttle, applyAt_us scheduling, forEachReady iteration (V-2)

**State Changes (from Blueprint §3.3, §3.4):**
- Before: HubState global parameter has previous value (e.g., brightness=128), dirty flags clean (SL-5)
- During: setGlobalBrightness(200) sets value + dirty flag; 50ms elapses with flag dirty
- After: Broadcast sent with new value (brightness=200), dirty flags cleared, HubState persists new value (SL-3 runtime persistent)

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Bypassing HubState and calling WebSocket directly**: Causes state inconsistency, breaks reconnection snapshot, defeats batching. (Violates "MUST NOT bypass HubState" boundary rule)
- ❌ **Broadcasting immediately without batching**: Encoder jitter storms spam WebSocket with rapid messages, breaks synchronization. (Violates "MUST batch every 50ms" boundary rule)
- ❌ **Omitting applyAt_us timestamp**: Wi-Fi jitter causes multi-node timing skew, breaks synchronized effects. (Violates "MUST use scheduled applyAt_us" boundary rule)
- ❌ **Broadcasting to non-READY nodes**: Nodes in PENDING/LOST states may not apply commands correctly, causes confusion. (Violates "MUST iterate forEachReady" boundary rule)

**Verification:**
- [ ] Turn encoder 10x rapidly (<50ms) → packet capture shows single broadcast (batching works)
- [ ] Broadcast JSON includes `"applyAt_us": <timestamp>` field (scheduled timing present)
- [ ] 2-3 nodes receive broadcast with SAME applyAt_us value (synchronization verified)
- [ ] Oscilloscope shows all nodes apply change within <10ms skew (M-3)

---

### IC-3: UDP Audio Metrics Fanout Sequence
*Maps to: Blueprint §4.3, PRD §5.2, §7.1 O-6, §7.2 E-5, §8.2 SL-6*

**Critical Sequence:**
```
1. hasActiveAudioMetrics()  // REQUIRED FIRST: Check audio activity before creating packet (SL-6)
   └─ RETURN EARLY if false  // CRITICAL: No UDP sent when silent (prevents bandwidth waste per NFR-1)
2. getLatestAudioMetrics()  // REQUIRED SECOND: Fetch audio processor data (E-5)
3. HubUdpFanout::buildBeatPacket(metrics)  // REQUIRED THIRD: Create BEAT_TICK packet (O-6), NOT PARAM_DELTA
4. HubRegistry::forEachReady(callback)  // REQUIRED FOURTH: Iterate all READY nodes (SL-7)
5. UDP::sendTo(node_ip, LW_UDP_PORT, packet)  // REQUIRED FIFTH: Best-effort UDP transmission to each node
```

**Ownership Rules (from PRD §7):**
- Audio metrics created by audio processor (external) — Tab5 hub must OBSERVE via hasActiveAudioMetrics/getLatestAudioMetrics (E-5)
- UDP BEAT_TICK packets created by Tab5 hub — Nodes RECEIVE best-effort (no retries)
- Audio activity detection — Hub must NOT fake metrics when audio inactive (prevents bandwidth waste)

**User Visibility (from PRD §6):**
- User sees: LED effects react to beat/BPM across all nodes synchronously (<30ms latency per V-4)
- User does NOT see: UDP BEAT_TICK packets, audio metric extraction, 100Hz transmission rate (V-4)

**State Changes (from Blueprint §3 - no direct state changes, queries SL-6):**
- Before: Audio metrics may be active (SL-6 hasActiveAudioMetrics=true) or silent (false)
- During: Query hasActiveAudioMetrics(); if true, fetch metrics and send UDP; if false, return early
- After: No state change (UDP is stateless broadcast); SL-6 audio state unchanged by transmission

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Sending UDP when audio inactive**: Wastes RF bandwidth, causes contention, violates UDP audio-only constraint. (Violates "DO NOT send UDP when audio inactive" boundary rule)
- ❌ **Sending parameters via UDP**: Parameters moved to WebSocket for reliability/ordering. UDP reserved EXCLUSIVELY for audio. (Violates "DO NOT send parameters via UDP" boundary rule)
- ❌ **Using PARAM_DELTA message type**: Legacy format deprecated. MUST use LW_UDP_BEAT_TICK. (Violates protocol alignment requirement)
- ❌ **Creating fake audio metrics when silent**: Defeats bandwidth optimization, causes nodes to react to non-existent audio. (Violates E-5 external system behavior)

**Verification:**
- [ ] hasActiveAudioMetrics() returns false → packet capture shows 0 UDP packets (early return works)
- [ ] Mock hasActiveAudioMetrics() to return true → packet capture shows 100 packets/second (100Hz verified)
- [ ] Packet capture: all UDP packets have msgType=LW_UDP_BEAT_TICK (0x02), NOT PARAM_DELTA (0x01)
- [ ] Packet payload: 4 bytes (bpm_x100, phase, flags), NOT 8 bytes (effectId, paletteId, brightness, speed)
- [ ] Audio inactive during 10s window → 0 UDP packets sent (M-2 >90% bandwidth reduction achieved)

---

## Validation Checklist

Before implementation, verify 1:1 mapping is complete:

### PRD Coverage
- [x] Every §5 acceptance criterion has a corresponding subtask (see Requirements Traceability table)
- [x] Every §6 visibility rule has "User Sees" in relevant subtask (V-1 through V-5 mapped to User Sees fields)
- [x] Every §7 ownership rule is in Quick Reference AND relevant subtask "Ownership" field (O-1 through O-7 mapped)
- [x] Every §8 state requirement has "State Change" in relevant subtask (SL-1 through SL-7, SI-1 through SI-4 mapped)

### Blueprint Coverage
- [x] Every §2 boundary rule is in Critical Boundaries AND enforced in tasks (14 DO NOTs + 6 MUSTs listed)
- [x] Every §3 state transition maps to Pre/Post conditions (§3.1 through §3.6 mapped to tasks 1.0, IC-1, IC-2)
- [x] Every §4 integration wiring maps to an Integration-Critical Task (§4.1→IC-2, §4.2→IC-1, §4.3→IC-3)

### Task Quality
- [x] First subtask of each parent references relevant docs (1.1, 2.1, 3.1, 4.1 reference PRD/Blueprint sections)
- [x] All subtasks are specific and actionable (not vague) (file paths, method names, specific actions provided)
- [x] All "Implements" fields trace back to PRD/Blueprint sections (every subtask has Implements field)