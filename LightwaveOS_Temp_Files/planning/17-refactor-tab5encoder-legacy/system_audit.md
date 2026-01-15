# System Audit: Tab5 Hub ↔ K1 Node ↔ Encoder Control Path

This audit covers the end-to-end control plane from Tab5 encoders → HubState → Hub broadcasts → K1 node receive → scheduled apply → renderer.

Scope: Phase 1 (HubState + modern protocol + batching + snapshot + applyAt scheduling).

## 1) Executive Summary

The primary reason the encoders appear “not to work” is a protocol/behaviour mismatch between the hub and node:

- The hub sends **control commands on WebSocket** using the JSON field `type` (e.g. `{"type":"effects.setCurrent",...}`), but the node WebSocket client parses only the field `t` and currently handles only `welcome` and `ota_update`. As a result, **the node receives the hub commands but does not apply them**.
- Zone control is additionally blocked because **node-runtime currently skips ZoneComposer initialisation**, so even a correct `zones.*` command cannot affect rendering.
- The hub still sends **UDP `LW_UDP_PARAM_DELTA`** at high cadence (contrary to the PRD), which can mask the WebSocket control-plane failure and can overload scheduling when time sync is not locked.

This directly conflicts with the PRD/Blueprint target system where parameters/zones are WebSocket-driven (50ms batched) and UDP is not used for parameter deltas.

## 2) Traceability to Requirements (PRD / Blueprint)

Key PRD requirements referenced:
- PRD §4.1 FR-1: WebSocket commands use modern `zones.*` and `colorCorrection.setConfig` only.
- PRD §4.1 FR-2: UDP reserved for audio metrics only (no parameter delta).
- PRD §4.1 FR-3: 50ms batching for parameter changes.
- PRD §4.1 FR-4: HubState singleton holds authoritative state.
- PRD §4.1 FR-5: State snapshot immediately after WELCOME handshake.
- PRD §4.1 FR-6: applyAt_us scheduling (hubNow + 30ms).

Blueprint references:
- Blueprint §1.2/§1.4: Target architecture (Encoder → HubState → HubHttpWs → nodes; UDP audio-only).
- Blueprint §2.3: Boundary rules (DO NOT send params via UDP; MUST send snapshot after WELCOME; MUST use forEachReady).

## 3) End-to-End Data Path (Current Code)

### 3.1 Tab5 Encoder → HubState (OK)

- `Tab5.encoder/src/main.cpp`: `DualEncoderService` invokes `ParameterHandler::onEncoderChanged()`.
- `Tab5.encoder/src/parameters/ParameterHandler.cpp`: maps encoder indices to `HubState` setters (global + zone).

This matches PRD FR-4 and the Blueprint target flow.

### 3.2 HubState → WebSocket (Partially OK)

- `Tab5.encoder/src/hub/hub_main.cpp`: batches every 50ms and calls:
  - `HubHttpWs::broadcastGlobalDelta(...)`
  - `HubHttpWs::sendZoneDelta(...)` (only for READY nodes)
- `Tab5.encoder/src/hub/net/hub_http_ws.cpp`:
  - WELCOME uses `t:"welcome"`
  - Control-plane commands use `type:"effects.setCurrent"`, `type:"parameters.set"`, `type:"zones.update"`
  - State snapshot uses `type:"state.snapshot"`

Batching + applyAt_us are implemented, consistent with PRD FR-3/FR-6.

### 3.3 HubState → UDP fanout (Not PRD-compliant)

- `Tab5.encoder/src/hub/net/hub_udp_fanout.cpp`: sends `LW_UDP_PARAM_DELTA` with `effectId/paletteId/brightness/speed/hue` continuously.

This violates PRD FR-2 and Blueprint §2.3 “DO NOT send parameters via UDP”.

### 3.4 Node Receive → Scheduler → RendererApply

#### UDP path (works, but should be phased out)
- `K1.LightwaveS3/src/node/net/node_udp_rx.cpp`: accepts `LW_UDP_PARAM_DELTA`, converts `applyAt_us` hub→local via time sync, enqueues `LW_CMD_SCENE_CHANGE` + `LW_CMD_PARAM_DELTA`.
- `K1.LightwaveS3/src/node/render/renderer_apply.cpp`: extracts due commands at render-frame boundary and applies to `NodeOrchestrator`.

#### WebSocket path (receives and now partially applies)
- `K1.LightwaveS3/src/node/net/node_ws_ctrl.cpp`: now parses both `doc["t"]` and `doc["type"]` and includes handlers for `welcome`, `ota_update`, and `state.snapshot` (global + zones) with `applyAt_us` scheduling.
- Legacy finding F-1 is therefore **INVALID** in the current reference code: the JSON schema mismatch has been fixed; the node no longer ignores messages that use `type`.

## 4) Findings (Root Causes)

### F-1: JSON schema mismatch (`t` vs `type`) blocks all hub control messages
- **Status:** INVALID (fixed in reference node implementation).
- Current behaviour: `NodeWsCtrl::handleMessage()` first checks `doc["t"]`, then falls back to `doc["type"]`, so both legacy and modern schemas are accepted. It also includes a `state.snapshot` handler that applies global and zone state using `applyAt_us` scheduling.
- Legacy impact statement retained here for traceability, but no longer reflects current code.

### F-2: Node-runtime skips ZoneComposer initialisation (zones cannot work)
- **Status:** CONFIRMED, PARTIALLY MITIGATED.
- Mitigation: in node-runtime builds, `K1.LightwaveS3/src/main.cpp` now initialises `ZoneComposer` and attaches it to the renderer even when NVS is skipped.
- Remaining gap: `ZoneComposer` is explicitly kept disabled via `zoneComposer.setEnabled(false)` in node-runtime, so hub-driven zone updates are structurally wired but effectively muted until an explicit enablement policy is implemented.

### F-3: Node health gating/fallback depends on show-UDP packets (conflicts with PRD “UDP audio-only”)
- **Status:** CONFIRMED (no mitigation applied yet).
- `K1.LightwaveS3/src/node/node_main.cpp` READY gate and fallback still use “recent UDP packets” as a liveness signal.
- If UDP is silenced when audio is inactive (PRD intent), the node will continue to appear degraded/failed even though the WS control-plane is healthy. This directly conflicts with the “UDP audio-only” requirement and must be corrected before Legacy Parity can be considered complete.

### F-4: Excessive debug output obscures signal (debuggability regression) and UDP PARAM_DELTA fanout semantics
- **Status:** MITIGATED, NOT RESOLVED.
- Debug output: noisy prints on hot network paths have been partially gated behind verbosity controls, but further tightening may still be desirable.
- UDP fanout: `Tab5.encoder/src/hub/net/hub_udp_fanout.cpp` now keeps `enabled_ = false` by default, and the CLI exposes `fanout on/off` controls. This means legacy `LW_UDP_PARAM_DELTA` packets are no longer sent in normal operation unless explicitly enabled for debugging.
- Remaining gap: when fanout is enabled, the hub still emits `LW_UDP_PARAM_DELTA` payloads instead of the PRD-compliant `LW_UDP_BEAT_TICK` audio-only packets. Until the payload/type is flipped and PARAM_DELTA is removed from normal operation, LP-PROTO-1 (“no parameter deltas via UDP”) remains unmet.

## 5) Compliance Matrix (High-Level)

- PRD FR-3 (50ms batching): **PASS** (`HubMain::loop()` batching).
- PRD FR-4 (HubState singleton): **PASS** (ParameterHandler writes to HubState).
- PRD FR-5 (snapshot after WELCOME): **PARTIAL** – hub sends `state.snapshot` and the node now has a handler, but end-to-end behaviour must be re-verified under the new `state.snapshot` implementation (including zone state and enablement).
- PRD FR-1 (modern zones.* only): **PASS on hub emit**, **FAIL end-to-end** – hub emits `zones.*` commands, node has ZoneComposer wired, but node-runtime keeps ZoneComposer disabled so zone updates do not yet affect rendering.
- PRD FR-2 (UDP audio-only): **PARTIAL** – UDP PARAM_DELTA fanout is disabled by default and effectively removed from normal operation, but the protocol still defines and can emit `LW_UDP_PARAM_DELTA` when fanout is deliberately enabled. Full compliance requires switching to `LW_UDP_BEAT_TICK` audio metrics only.
- PRD FR-6 (applyAt_us scheduling): **PASS on hub emit**, **PARTIAL end-to-end** – hub and node agree on `applyAt_us` for UDP and `state.snapshot`; additional verification is required for WS parameter/zone updates applied via the scheduler.

## 6) Recommended Fix Plan (Phase 1-aligned)

Priority order:

1. **Node WS control-plane support (must)**  
   - Accept both `t` and `type` in node WS parsing for compatibility.
   - Implement handlers for: `state.snapshot`, `effects.setCurrent`, `parameters.set`, `zones.update`.
   - Apply with `applyAt_us` scheduling (hub→local conversion when time sync locked; otherwise apply immediately).

2. **Enable ZoneComposer in node-runtime (must)**  
   - Initialise ZoneComposer without NVS persistence in node-runtime.
   - Attach it to the renderer so `zones.*` can affect rendering.

3. **Refactor node readiness/fallback to not require show-UDP (must)**  
   - Use time-sync UDP pongs (or WS keepalive cadence) as the liveness signal.
   - Ensure node remains READY when audio is inactive and no UDP BEAT_TICK is sent.

4. **Disable UDP PARAM_DELTA fanout (should, per PRD)**  
   - Keep UDP time sync as-is.
   - Phase 2: implement BEAT_TICK only when audio active.

5. **Gate noisy debug prints (should)**  
   - Make TS-UDP and WS event spam opt-in via a serial command or compile-time flag.

## 7) Verification Checklist (What “Working” Looks Like)

- Turning an encoder changes Tab5 HubState immediately and produces a single batched WS message within 50ms (PRD FR-3).
- Node serial shows it receives `type:"parameters.set"` / `type:"zones.update"` and applies within ~30–80ms (PRD FR-6, NFR-2).
- Zone encoders visibly change per-zone effects on the node (PRD FR-1 + zone requirements).
- Hub reboot: node reconnects (HELLO→WELCOME) and immediately applies the snapshot (PRD FR-5).
- With no audio active: UDP BEAT_TICK is silent, but the node remains READY (PRD FR-2 intent + node readiness fix).
