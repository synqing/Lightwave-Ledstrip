# K1.LightwaveOS Production Engineering Specification
## Tab5 Hub + K1 Node Architecture

**Version**: 1.0  
**Created**: 2026-01-12  
**Status**: Engineering Spec (Ready for Implementation)  
**Mode**: ENGINEERING

---

## 0) Action-Item Map (with dependencies + blockers)

### Domain A — Protocol & timing foundations (BLOCKER for everything)

1. Define **frozen constants** + protocol versioning rules.
2. Define **WS control messages** (JSON schemas).
3. Define **UDP stream packet** (binary layout + validation rules).
4. Define **time sync** algorithm + lock/unlock conditions.
5. Define **applyAt scheduler contract** (render boundary apply; no blocking; bounded memory).

**Blockers:** none (pure spec + implementation).

### Domain B — Tab5 Hub networking bring-up (depends on A)

1. Hosted radio init (P4↔C6).
2. SoftAP + DHCP server up.
3. HTTP server + WebSocket endpoint `/ws`.
4. Node registry service on WS.
5. UDP fanout socket + tick loop.

**Blockers:** esp-hosted integration stability / correct firmware pairing.

### Domain C — K1 Node client bring-up (depends on A)

1. STA join + reconnect policy.
2. WS client connect → HELLO/WELCOME → session token.
3. Time sync pings until locked.
4. UDP receive pipeline → jitter/seq stats → enqueue scheduled events.
5. Render integration: apply scheduled events at frame boundary.

**Blockers:** ensuring networking tasks can't starve render task.

### Domain D — OTA end-to-end (depends on B + C)

1. Hub hosts `manifest.json` + `.bin` images.
2. Node performs pull-OTA with SHA verification and reports progress.
3. Hub does **rolling updates** (max concurrent = 1), waits for node to rejoin READY.
4. UI command surface (even minimal) to trigger/monitor OTA.

**Blockers:** partition scheme / OTA library differences in your build system.

### Domain E — Resilience + observability (depends on B + C)

1. Node fallback policy (hold last stable / idle when hub missing).
2. Metrics: loss, drift, RSSI, lastSeen, reconnect counts.
3. Hub dashboard endpoints `/health`, `/metrics` (optional but recommended).
4. Chaos checks: hub reboot, packet loss burst, node power cycle mid-show, OTA fail mid-download.

---

## 1) Frozen Constants (MUST NOT DRIFT)

```text
PROTO_VER = 1
HUB_IP = 192.168.4.1
WS_PATH = /ws
UDP_PORT = 49152
CTRL_HTTP_PORT = 80

KEEPALIVE_PERIOD_MS = 1000
KEEPALIVE_TIMEOUT_MS = 3500

TS_PING_PERIOD_MS = 200
TS_LOCK_SAMPLES_N = 10

UDP_TICK_HZ = 100
APPLY_AHEAD_US = 30000  // hub schedules in the future

DRIFT_DEGRADED_US = 3000
UDP_SILENCE_DEGRADED_MS = 200
UDP_SILENCE_FAIL_MS = 500

OTA_NODE_TIMEOUT_S = 180
OTA_MAX_CONCURRENT = 1   // rolling updates only
```

---

## 2) Top-Level System Architecture

### Tab5 = HUB (SoftAP Host + Conductor)

**Runs:**

* SoftAP + DHCP (single SSID for the system)
* HTTP + WebSocket server (control plane)
* UDP tick stream (stream plane)
* Node registry (multi-node health + session)
* Show clock (authoritative timebase + musical grid values)
* OTA host + dispatcher (rolling updates)

### K1 = NODE (STA Client + Deterministic Renderer)

**Runs:**

* WiFi STA join + reconnect
* WS client to hub (control plane)
* Time sync estimator
* UDP receiver pipeline (loss/seq/jitter)
* applyAt scheduler (bounded)
* Renderer (hard realtime, never blocks)
* OTA pull client
* Fallback policy

---

## 3) Codebase Layout (Target Modules)

Create a shared "common" package used by both firmwares:

```
/common
  /proto
    proto_constants.h
    ws_messages.h        // JSON keys + encode/decode helpers
    udp_packets.h        // packed structs + validation helpers
    crc32.h              // optional
  /clock
    monotonic.h
    timesync.h
    schedule_queue.h
  /util
    ringbuf.h            // bounded queues
    logging.h
```

### Tab5 firmware target modules

```
/Tab5.encoder
  /src
    /net
      hub_radio_hosted.*       // P4<->C6 init
      hub_softap_dhcp.*        // AP + leases
      hub_http_ws.*            // /health /ws /ota/*
      hub_udp_fanout.*         // tick loop + send
      hub_registry.*           // node table + timeouts
    /show
      hub_clock.*              // hubNow_us + bpm/phase/flags
    /ota
      hub_ota_repo.*           // manifest + bin serving
      hub_ota_dispatch.*       // rolling update state machine
    /diag
      hub_metrics.*
```

### K1 firmware target modules

```
/K1.LightwaveS3
  /src
    /net
      node_wifi_sta.*          // join + reconnect
      node_ws_ctrl.*           // HELLO/WELCOME/keepalive
      node_udp_rx.*            // decode/validate/seq stats
    /sync
      node_timesync.*          // offset estimator + lock
      node_scheduler.*         // applyAt queue + due extraction
    /render
      renderer_core.*          // existing renderer
      renderer_apply.*         // frame-boundary command apply
    /ota
      node_ota_client.*        // pull OTA + verify + reboot
    /failover
      node_fallback.*
    /diag
      node_metrics.*
```

---

## 4) Control Plane (WebSocket over HTTP)

### Endpoint

* Hub HTTP server on `http://192.168.4.1:80`
* WS endpoint: `ws://192.168.4.1/ws`

### Session Rules

* Node must WS connect, send HELLO.
* Hub replies WELCOME with **session token**.
* Node includes token in:
  * all WS messages (field `token`)
  * all UDP packets validation (token embedded or derived).

### Message: `HELLO` (Node → Hub)

```json
{
  "t": "hello",
  "proto": 1,
  "mac": "AA:BB:CC:DD:EE:FF",
  "fw": "k1-v2.X.Y",
  "caps": { "udp": true, "ota": true, "clock": true },
  "topo": { "leds": 320, "channels": 2 }
}
```

### Message: `WELCOME` (Hub → Node)

```json
{
  "t": "welcome",
  "proto": 1,
  "nodeId": 3,
  "token": "random_session_token",
  "udpPort": 49152,
  "hubEpoch_us": 1234567890
}
```

### Message: `KEEPALIVE` (Node → Hub) every 1s

```json
{
  "t": "ka",
  "nodeId": 3,
  "token": "…",
  "rssi": -52,
  "loss_pct": 0.7,
  "drift_us": 1100,
  "uptime_s": 834
}
```

### Messages: Time Sync (WS)

Node → Hub:

```json
{ "t":"ts_ping", "nodeId":3, "token":"…", "seq":17, "t1_us":123456789 }
```

Hub → Node:

```json
{ "t":"ts_pong", "nodeId":3, "seq":17, "t1_us":123456789, "t2_us":987654321 }
```

### Message: OTA command (Hub → Node)

```json
{
  "t":"ota_update",
  "nodeId":3,
  "token":"…",
  "version":"2.8.0",
  "url":"http://192.168.4.1/ota/k1/s3/2.8.0.bin",
  "sha256":"…"
}
```

### Message: OTA status (Node → Hub)

```json
{ "t":"ota_status", "nodeId":3, "token":"…", "state":"downloading", "pct":37 }
```

---

## 5) Stream Plane (UDP) — Binary Contract

### Hub sends UDP unicast to each Node IP (port 49152) at 100Hz.

### UDP Header (packed, fixed-size)

Use a packed struct (C/C++):

```c
#pragma pack(push, 1)
typedef struct {
  uint8_t  proto;        // = PROTO_VER
  uint8_t  msgType;      // enum
  uint16_t payloadLen;   // bytes
  uint32_t seq;          // increments every tick
  uint32_t tokenHash;    // 32-bit derived from session token (or CRC of token)
  uint64_t hubNow_us;    // authoritative time
  uint64_t applyAt_us;   // hubNow_us + APPLY_AHEAD_US
} lw_udp_hdr_t;
#pragma pack(pop)
```

### Payload Types (msgType)

* `0x01 PARAM_DELTA` (compact params for effects/palettes)
* `0x02 BEAT_TICK` (bpm/phase/downbeat flags)
* `0x03 SCENE_CHANGE` (effectId/paletteId hard switch)
* `0x04 HEARTBEAT` (optional)
* `0x05 RESERVED`

### Validation Rules (Node must implement)

Drop packet if any:

* `proto != PROTO_VER`
* `payloadLen` exceeds max
* `tokenHash` mismatch
* optional CRC mismatch (if you add CRC)

Sequence handling:

* If first packet: set `expected_seq = seq+1`
* If `seq == expected_seq`: accept, increment expected
* If `seq > expected_seq`: count loss (`seq-expected_seq`), accept, set expected = seq+1, mark degraded if gap large
* If `seq < expected_seq`: drop as duplicate/late

### Jitter Buffer & Scheduler

Node converts hub applyAt into local time:

* `localApplyAt_us = applyAt_us + offset_us` (offset estimated by time sync)

Enqueue into a bounded sorted queue keyed by `localApplyAt_us`.

---

## 6) Time Sync Algorithm (Node)

Node maintains:

* `offset_us` (hub→local mapping)
* `rtt_us` smoothed
* lock boolean

On receiving pong:

* `t1 = t1_us` (sent time in local)
* `t2 = hub time at hub send`
* `t3 = local receive time now`
* `RTT = t3 - t1`
* `offset_est = (t2 + RTT/2) - t3`
* Apply low-pass filter: `offset_us = LPF(offset_us, offset_est)`

Lock conditions:

* at least `TS_LOCK_SAMPLES_N` good samples
* RTT variance below threshold
* offset slope stable

Unlock conditions:

* drift runaway, RTT unstable, missed pongs, WS lost

---

## 7) applyAt Scheduler Contract (Node)

### Hard realtime rule

Renderer never waits on WS/UDP/scheduler. Scheduler interaction is:

* enqueue from UDP RX task (short critical section / lock-free)
* dequeue at **frame boundary** in render task (non-blocking)

### Bounded Queue (mandatory)

* fixed capacity (e.g. 64 events)
* overflow policy:
  * **coalesce** param deltas (keep newest)
  * drop newest if still full
* no heap growth

### Due extraction

At each frame start:

* while `nextApplyAt <= now`:
  * pop and apply to effect runtime state

---

## 8) Renderer Integration (Node)

Renderer must expose an "apply command" interface used by scheduler events:

```c
typedef enum {
  CMD_SCENE_CHANGE,
  CMD_PARAM_DELTA,
  CMD_BEAT_TICK
} lw_cmd_type_t;

typedef struct {
  lw_cmd_type_t type;
  uint16_t effectId;
  uint16_t paletteId;
  // plus compact param fields
} lw_cmd_t;
```

Render loop:

1. `pull_due_events(max_k)` (bounded)
2. `apply_cmds_to_state()` (no allocations)
3. `synthesize_frame()`
4. `output_leds()`

Fallback behavior if hub missing:

* hold last stable scene OR idle effect
* never random flicker

---

## 9) Hub Node Registry (Multi-Node)

Hub maintains `NodeTable`:

* key: `nodeId` (session), stable MAC identity
* fields:
  * `mac`, `ip`, `tokenHash`
  * `state`: `PENDING/AUTHED/READY/DEGRADED/LOST`
  * `lastSeen_us` (from keepalive)
  * `loss_pct`, `drift_us`, `rssi`
  * `fw`, `caps`, `topo`

State transitions:

* `HELLO` → PENDING
* `WELCOME sent` → AUTHED
* `node reports ready` (or hub infers from metrics) → READY
* degrade on: keepalive miss, drift>3000us, loss>2%, UDP silence
* LOST on: WS disconnect or keepalive timeout
* cleanup after 10s LOST

---

## 10) Hub Show Clock + UDP Fanout

Hub Show Clock provides:

* `hubNow_us` monotonic
* `bpm_x100`, `phase`, flags

UDP tick loop at 100Hz:

1. read `hubNow_us`
2. set `applyAt_us = hubNow_us + APPLY_AHEAD_US`
3. build payload (latest scene/params)
4. unicast send to each READY node IP
5. update per-node send counters

Tick overrun rule:

* if build+fanout exceeds tick budget:
  * increment `tick_overrun` metric
  * still send; do not block next tick (drop/skip if necessary)

---

## 11) OTA Architecture (Hub-hosted, Node-pull, Rolling)

### Hub endpoints

* `GET /ota/manifest.json`
* `GET /ota/k1/s3/<version>.bin`

Manifest format:

```json
{
  "k1": {
    "stable": { "version":"2.8.0", "url":"/ota/k1/s3/2.8.0.bin", "sha256":"…" }
  }
}
```

Hub OTA dispatcher policy:

* `OTA_MAX_CONCURRENT = 1`
* order nodes deterministically (by nodeId or MAC)
* for each node:
  1. send `ota_update`
  2. wait for status progress
  3. wait for node reboot + rejoin + READY
  4. proceed to next
* fail handling:
  * stop rollout on first failure unless user overrides

Node OTA client:

* precheck (dim/pause, power ok)
* download via HTTP
* sha256 verify
* write to OTA partition
* set boot partition
* reboot
* on boot: normal join + report new fw

---

## 12) Task Priorities (Hard Requirements)

### Node (K1 ESP32-S3)

1. `TASK_RENDER` (highest)
2. `TASK_UDP_RX` (high)
3. `TASK_WS_CTRL` (medium)
4. `TASK_TIMESYNC` (low/medium)
5. `TASK_OTA` (low)

### Hub (Tab5)

1. `TASK_UDP_FANOUT` (high)
2. `TASK_WS_SERVER` (medium)
3. `TASK_REGISTRY` (medium)
4. `TASK_SHOW_CLOCK` (medium)
5. `TASK_OTA_DISPATCH` (low/medium)
6. `TASK_METRICS` (low)

---

## 13) Acceptance Checks (Definition of Done)

### Wi-Fi / Discovery

* Hub starts SoftAP; phone joins; can fetch `/health`.
* 4 nodes join as STA, get DHCP, connect WS, complete HELLO/WELCOME.

### Sync / Scheduling

* All nodes lock time sync (>=10 good samples).
* Hub streams UDP at 100Hz; nodes schedule applyAt.
* With 4 nodes: repeated scene change 50x shows aligned switching (no visible desync).

### Resilience

* Hub reboot mid-show:
  * nodes enter fallback within 500ms UDP silence
  * nodes reconnect, relock time sync, resume show without manual intervention

### OTA

* Hub serves manifest + bin.
* Rolling update across 4 nodes:
  * one node at a time
  * each node reboots and rejoins READY
  * rollout stops on failure

### Performance

* Render frame-time jitter not materially worsened by networking (renderer never blocks).
* UDP RX and WS tasks do not starve render.

---

## 14) "Agent Execution Plan" (How a coding agent should implement)

Implement in this exact order:

1. Shared `/common` proto + timesync + scheduler primitives.
2. Hub: SoftAP + HTTP/WS server + `/health`.
3. Node: STA join + WS HELLO/WELCOME.
4. Node: time sync lock.
5. Hub: UDP tick sender (no payload complexity yet).
6. Node: UDP RX + seq/loss stats + schedule dummy events.
7. Render integration: apply dummy events at frame boundary.
8. Real payloads: scene change + param deltas.
9. OTA: hub endpoints + node OTA client + rolling dispatcher.
10. Remove all legacy "K1 host / Tab5 client discovery" paths (hard switch).

---

## 15) Network Topology

### IP Addressing

```
Hub (Tab5):    192.168.4.1/24 (Gateway + DHCP Server)
Node 1:        192.168.4.2/24 (Assigned by DHCP)
Node 2:        192.168.4.3/24
Node 3:        192.168.4.4/24
Node 4:        192.168.4.5/24
...
DHCP Range:    192.168.4.2 - 192.168.4.50
```

### Access Point Configuration

```cpp
// Hub AP Settings
#define HUB_AP_SSID "K1-LightwaveOS"
#define HUB_AP_PASSWORD "SpectraSynq"
#define HUB_AP_CHANNEL 6  // Fixed channel for stability
```

### Node Configuration

```cpp
// Node WiFi Settings (compile-time)
#define NODE_TARGET_SSID "K1-LightwaveOS"
#define NODE_TARGET_PASSWORD "SpectraSynq"
#define NODE_HUB_IP IPAddress(192, 168, 4, 1)
#define NODE_WS_URI "ws://192.168.4.1/ws"
```

---

## 16) Performance Requirements

### Hub (Tab5)

* **UDP tick loop**: 100Hz stable (10ms period)
  * Budget per tick: <5ms (build payload + 4x unicast send)
  * Overrun tracking: count and log, but don't block next tick
* **WS server**: Handle 4+ concurrent connections
* **Node registry**: Process keepalives within 10ms
* **Memory**: Bounded growth, no leaks

### Node (K1)

* **Renderer**: 120 FPS maintained (8.33ms frame budget)
* **UDP RX**: Process packet within 2ms (seq check + enqueue)
* **Time sync**: Lock within 5 seconds of WELCOME
* **Scheduler**: Extract due events within 1ms (at frame boundary)
* **Memory**: applyAt queue bounded to 64 events max

---

## 17) Failure Modes & Mitigations

### Hub Reboot

**Behavior:**
* Nodes detect UDP silence (>500ms)
* Nodes enter fallback (hold last scene)
* Hub restarts, nodes reconnect WS
* Nodes relock time sync
* Show resumes

**Requirement:** Resume within 2 seconds

### Node Power Cycle

**Behavior:**
* Hub marks node LOST after keepalive timeout
* Node reboots, joins WiFi, connects WS
* Time sync locks
* Hub marks node READY
* Show continues (other nodes unaffected)

**Requirement:** Rejoin within 10 seconds

### Packet Loss Burst

**Behavior:**
* Node tracks loss% via seq gaps
* If loss >2%: mark DEGRADED, report in keepalive
* If loss >10%: consider fallback
* Hub can adjust tick rate or payload size

**Requirement:** Graceful degradation, no crashes

### Time Sync Unlock

**Behavior:**
* Node detects drift runaway (>3ms) or RTT instability
* Unlock time sync, mark DEGRADED
* Continue sending ts_ping until relock
* applyAt scheduling disabled until relock

**Requirement:** Automatic recovery within 5 seconds

---

## 18) Testing Strategy

### Unit Tests (per module)

* Time sync: offset convergence, lock/unlock triggers
* Scheduler: bounded queue, overflow policy, due extraction
* UDP validation: proto version, seq gaps, token hash
* Node registry: state transitions, timeout handling

### Integration Tests

* 1 Hub + 1 Node: Full handshake → time sync → UDP stream
* 1 Hub + 4 Nodes: Multi-node registry, keepalive, metrics
* Scene changes: 50x rapid changes, verify alignment

### Chaos Tests

* Hub reboot during show
* Node power cycle mid-show
* Introduce 20% packet loss (tc/netem)
* OTA failure mid-download

### Performance Tests

* Hub: 100Hz tick stability with 4 nodes (measure overruns)
* Node: Render FPS maintained at 120 (measure drops)
* Memory: 24-hour soak test (check for leaks)

---

## 19) Metrics & Observability

### Hub Metrics (`/metrics` endpoint)

```json
{
  "nodes": {
    "total": 4,
    "ready": 3,
    "degraded": 1,
    "lost": 0
  },
  "udp": {
    "tick_hz": 99.8,
    "overruns": 12,
    "bytes_sent": 1234567
  },
  "ota": {
    "rolling_update": false,
    "last_version": "2.8.0"
  }
}
```

### Node Metrics (in KEEPALIVE)

```json
{
  "rssi": -52,
  "loss_pct": 0.7,
  "drift_us": 1100,
  "uptime_s": 834,
  "render_fps": 120,
  "heap_free": 180000
}
```

---

## 20) Migration from Legacy Architecture

### Remove from v2 (Node)

* WebServer + AsyncWebSocket
* All REST API endpoints (46+)
* AP mode management
* mDNS responder
* WiFiCredentialsStorage (multi-network NVS)

### Remove from Tab5 (Hub)

* WiFi client logic (STA mode)
* Multi-network fallback
* mDNS client
* WebSocket client to v2

### Add to Tab5 (Hub)

* SoftAP + DHCP server
* HTTP server (minimal: /health, /ws, /ota/*)
* WebSocket server
* UDP fanout loop
* Node registry
* OTA dispatcher

### Add to v2 (Node)

* WiFi STA (single network)
* WebSocket client
* Time sync client
* UDP receiver
* applyAt scheduler
* OTA pull client

---

## Conclusion

This architecture transforms the system from a fragile 1:1 WebSocket connection with multi-network failover hell into a **production-grade hub/spoke architecture** with:

* **Deterministic discovery** (nodes know hub SSID)
* **Multi-node support** (4+ nodes, registry-based)
* **Sub-millisecond synchronization** (time sync + applyAt)
* **Resilient failover** (automatic recovery from hub/node failures)
* **Safe OTA updates** (rolling, SHA-verified)

This is the architecture specification a coding agent can follow line-by-line: **modules**, **interfaces**, **message schemas**, **timers**, **state rules**, and **done criteria**.

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-12  
**Author:** User + AI Assistant  
**Status:** Ready for Implementation
