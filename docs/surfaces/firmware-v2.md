# LightwaveOS v2 (ESP32-S3) — Control Surface Map (Granular)
**Scope:** `firmware/v2/` only. This document maps the **control surface** of the v2 firmware: boot, ownership boundaries, the render pipeline, effect registration, centre-origin constraints, zones, transitions, narrative shows, audio, networking, persistence, and validation.

> **Hard invariants (non-negotiable):**
> - **Centre origin only**: effects must originate at LEDs **79/80** and propagate outward (or converge inward).
> - **No rainbows**: no hue-wheel / rainbow cycling.
> - **No heap allocation in render hot paths**: avoid `new/malloc`, `std::vector` growth, `String` concatenation in per-frame loops.
> - **British English** in comments/docs (centre, colour, initialise).

---

## 1) Where to start (boot + wiring)

### 1.1 Entry point and boot order
- **File:** `firmware/v2/src/main.cpp`
- **What to extract:**
  - Boot sequence ordering (what is initialised before what, and why).
  - Feature-flag gated blocks (e.g. WebServer, Audio).
  - Who owns global singletons (RendererActor, ZoneComposer, PluginManager, WebServer).

### 1.2 Actor system creation and core assignment
- **Files:** `firmware/v2/src/core/actors/ActorSystem.*`
- **What to extract:**
  - Actor creation order (RendererActor first, then ShowDirector, then AudioActor under `FEATURE_AUDIO_SYNC`).
  - Task/core placement: renderer is the high-FPS core workload (typically Core 1).
  - How actors are started and stopped; what constitutes “system running”.

---

## 2) Render pipeline: from “current state” to LEDs

### 2.1 RendererActor responsibilities (single point of render truth)
- **Files:** `firmware/v2/src/core/actors/RendererActor.*`
- **Understand and document:**
  - Render loop timing (target 120 FPS) and frame budget constraints.
  - How the active effect is selected (effect registry + current ID).
  - Where the final `leds[320]` (or equivalent) buffer is written and flushed to hardware.
  - Thread-safety assumptions (RendererActor typically owns render buffers; network/UI threads must not mutate them directly).

### 2.2 Led topology and “centre point”
- **Files:** `firmware/v2/src/config/hardware_config.h` (and related), `firmware/v2/src/effects/CoreEffects.h`
- **What to extract:**
  - `CENTER_LEFT` / `CENTER_RIGHT` (or equivalent) and any `ctx.centerPoint` semantics.
  - Mapping of two strips (0..159 each) into unified 320 buffer (if used).
  - Any helper functions used by effects to address “centre-out” coordinates.

---

## 3) Effect system: registration, interface, lifecycle

### 3.1 Effect interface contract
- **Files:** `firmware/v2/src/plugins/api/IEffect.*` and `EffectContext.*` (if present)
- **What to extract:**
  - The exact `IEffect` methods (init/update/render/metadata).
  - What `EffectContext` exposes: time, palette, parameters, audio snapshot, led buffer access, centre point.
  - Rules around allocation and per-frame determinism.

### 3.2 Effect registration surface
- **File:** `firmware/v2/src/effects/CoreEffects.cpp`
- **What to extract:**
  - Registration pattern (static instances, `renderer->registerEffect(id, &instance)`).
  - Expected effect count / bounds (`MAX_EFFECTS`, `EXPECTED_EFFECT_COUNT`, etc.).
  - Categorisation: core vs LGP families vs audio-reactive sets.

### 3.3 Centre-origin enforcement (where “policy” lives)
This repo has **three** enforcement layers; map which are “hard” vs “convention”:
- **Design constraints:** `README.md`, `CLAUDE.md`, `AGENTS.md`
- **Runtime helpers:** `CoreEffects.h`, `EffectContext` (centre constants/fields)
- **Validation:** `firmware/v2/src/validation/*` (and any effect validation macros)

**What to document:**
- Whether centre-origin is checked mechanically (unit tests/validators) or is by convention and code review.
- Any “centre origin” helper functions that all effects are expected to use.
- Any validator commands (`validate <id>`) that check memory deltas or render correctness.

---

## 4) Zones: multi-zone composition control surface

### 4.1 ZoneComposer responsibilities and buffer proxy model
- **Files:** `firmware/v2/src/effects/zones/ZoneComposer.*`
- **What to extract:**
  - Zone definition (how LED indices are sliced around centre).
  - Buffer proxy pattern: per-zone temporary buffers → composited into final buffer.
  - Zone count and presets; how presets map to segment definitions.
  - Any constraints about assuming LED counts (must respect config / buffer size).

### 4.2 Zone persistence and restoration
- **Files:** `firmware/v2/src/core/persistence/ZoneConfigManager.*`, `NVSManager.*`
- **What to extract:**
  - What zone fields persist (zone count, layout, per-zone effect, per-zone params).
  - Restore flow (when ZoneComposer loads saved state during boot).
  - “First boot defaults” behaviour.

---

## 5) Transitions: effect changes over time

### 5.1 Transition engine boundaries
- **Files:** `firmware/v2/src/effects/transitions/TransitionEngine.*`, `TransitionTypes.*`
- **What to extract:**
  - How transitions are triggered (command surface: serial, WS, REST).
  - Transition type catalogue and any parameters (duration, easing).
  - Interaction with zones: does transition blend the post-zone composite, or per-zone?
  - Where transition code runs (render thread) and performance implications.

---

## 6) Narrative shows: sequencing and “show director”

### 6.1 ShowDirectorActor and show definitions
- **Files:** `firmware/v2/src/core/actors/ShowDirectorActor.*`, `firmware/v2/src/core/shows/*`, `firmware/v2/src/core/narrative/*`
- **What to extract:**
  - How shows schedule effect changes and transitions.
  - What “narrative engine” means in practice (states/phases, triggers, timing).
  - Which actor owns show timing; how it communicates with RendererActor.

---

## 7) Audio subsystem (I2S → analysis → render-safe snapshot)

### 7.1 Audio core architecture
- **Files:** `firmware/v2/src/audio/README.md`, `AudioActor.*`, `AudioCapture.*`, `GoertzelAnalyzer.*`, `ControlBus.*`, `tempo/*`, `ChromaAnalyzer.*`
- **What to extract:**
  - Capture: sample rate, hop size, DMA behaviour, and any “bit shift” landmines.
  - Analysis: Goertzel band definitions and window size.
  - Smoothing: attack/release logic and derived features (flux, RMS).
  - Beat/tempo: how `isOnBeat`, BPM, beat phase are produced.
  - **Cross-core safety**: SnapshotBuffer/double-buffer and who reads/writes.

### 7.2 Audio-reactive policy constraints
- **Docs:** `docs/audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md` (and related)
- **What to extract:**
  - “No rigid frequency→visual bindings” requirement.
  - Musical saliency/style detection features and which effects use them.

---

## 8) Network control surface (REST + WebSocket)

### 8.1 WiFi state machine (protected file)
- **Files:** `firmware/v2/src/network/WiFiManager.*`
- **What to extract (and treat as protected):**
  - State machine diagram and transitions.
  - AP fallback behaviour.
  - The EventGroup bit-clearing landmine on connect transitions.
  - Which thread/core owns WiFi operations; how events are signalled.

### 8.2 WebServer API and routing
- **Files:** `firmware/v2/src/network/WebServer.*`
- **Docs:** `docs/api/*` (especially v1 and websocket behaviour)
- **What to extract:**
  - REST API v1 base paths and response schema.
  - WebSocket endpoint and message routing (command types).
  - Rate limiting and thread safety (HTTP callbacks vs WS callbacks).
  - How server commands map to actor messages (RendererActor/ZoneComposer/TransitionEngine).

### 8.3 WebSocket protocol surface (v2 firmware)
**Socket:** `ws://<device>/ws`

#### 8.3.1 Envelope rule (important)
Incoming WS messages include a top-level `"type"` field. The router strips it before handing to handlers:
- Router: `firmware/v2/src/network/webserver/WsCommandRouter.cpp`
- Behaviour: removes `"type"` from the JsonDocument, then dispatches.

**Implication:** handler codecs validate payload fields only; `"type"` is a pure envelope field.

#### 8.3.2 Broadcasted “status” payload (what Tab5 and dashboards should expect)
Produced by `WebServer::doBroadcastStatus()` in `firmware/v2/src/network/WebServer.cpp`.

**WS broadcast type:** `"status"`

**Fields (observed):**
- **Effect / global params**
  - `effectId` (uint8)
  - `effectName` (string; present if name known)
  - `brightness` (uint8)
  - `speed` (uint8)
  - `paletteId` (uint8)
  - `hue` (uint8)
  - `intensity` (uint8)
  - `saturation` (uint8)
  - `complexity` (uint8)
  - `variation` (uint8)
- **Performance / device**
  - `fps` (number)
  - `cpuPercent` (number)
  - `freeHeap` (number)
  - `uptime` (seconds)
- **Audio (when `FEATURE_AUDIO_SYNC`)**
  - `bpm` (float)
  - `mic` (float dB, derived from `rmsPreGain`)
  - `key` (string, may be `""`)

#### 8.3.3 Broadcasted zones payload(s)
Produced by `WebServer::broadcastZoneState()` and `broadcastSingleZoneState()` in `firmware/v2/src/network/WebServer.cpp`.

**WS broadcast type:** `"zones.list"`
- `enabled` (bool)
- `zoneCount` (uint8)
- `segments[]` (array of segment defs)
  - `zoneId`, `s1LeftStart`, `s1LeftEnd`, `s1RightStart`, `s1RightEnd`, `totalLeds`
- `zones[]` (array)
  - `id`
  - `enabled`
  - `effectId`, `effectName`
  - `brightness`, `speed`, `paletteId`
  - `blendMode`, `blendModeName`
  - **zone audio config fields**:
    - `tempoSync`, `beatModulation`, `tempoSpeedScale`, `beatDecay`, `audioBand`,
      `beatTriggerEnabled`, `beatTriggerInterval`
- `presets[]` (array of built-in zone presets)
  - `id`, `name`

**WS broadcast type:** `"zones.stateChanged"`
- `zoneId`
- `timestamp`
- `current{ enabled, effectId, effectName, brightness, speed, paletteId, blendMode, blendModeName }`

#### 8.3.4 WebSocket command catalogue (handlers present in v2)
These types are registered under `firmware/v2/src/network/webserver/ws/` via `WsCommandRouter::registerCommand(...)`.

**Device / legacy**
- `getStatus` (legacy) → triggers `status` broadcast and may emit a `pong` envelope for RTT.
- `device.getStatus` → returns `device.status`
- `device.getInfo` → returns `device.info`

**Effects & global parameters**
- `effects.getMetadata` → `effects.metadata`
- `effects.getCurrent` → `effects.current`
- `effects.list` → `effects.list`
- `effects.setCurrent` → `effects.changed` (optionally starts transition)
- `effects.parameters.get` → `effects.parameters`
- `effects.parameters.set` → `effects.parameters.changed` (queued/failed keys)
- `effects.getCategories` → `effects.categories`
- `effects.getByFamily` → `effects.byFamily`
- `parameters.get` → `parameters`
- `parameters.set` → `parameters.changed` (see `WsEffectsCommands.cpp`)

**Zones**
- `zone.enable` → broadcasts `zone.enabledChanged` + triggers zone broadcast
- `zone.setEffect` → `zones.effectChanged`
- `zone.setBrightness` → `zones.changed`
- `zone.setSpeed` → `zones.changed`
- `zone.setPalette` → `zone.paletteChanged`
- `zone.setBlend` → `zone.blendChanged`
- `zone.loadPreset` → triggers zone broadcast
- `zones.get` → `zones`
- `zones.list` → `zones.list` (response form)
- `zones.update` → `zones.changed`
- `zones.setLayout` → `zones.layoutChanged`
- `getZoneState` → triggers zone broadcast

**Transitions**
- `transition.trigger` (legacy) (starts transition; no structured response)
- `transition.getTypes` → `transitions.types`
- `transition.config` → `transitions.config`
- `transitions.list` → `transitions.list`
- `transitions.trigger` → `transition.started`

**Palettes**
- `palettes.list` → `palettes.list`
- `palettes.get` → `palettes.get`
- `palettes.set` → `palettes.set`

**Colour engine / correction**
- `color.getStatus` → `color.getStatus`
- `color.enableBlend` → `color.enableBlend`
- `color.setBlendPalettes` → `color.setBlendPalettes`
- `color.setBlendFactors` → `color.setBlendFactors`
- `color.enableRotation` → `color.enableRotation`
- `color.setRotationSpeed` → `color.setRotationSpeed`
- `color.enableDiffusion` → `color.enableDiffusion`
- `color.setDiffusionAmount` → `color.setDiffusionAmount`
- `colorCorrection.getConfig` → `colorCorrection.getConfig`
- `colorCorrection.setMode` → `colorCorrection.setMode`
- `colorCorrection.setConfig` → `colorCorrection.setConfig`
- `colorCorrection.save` → `colorCorrection.save`

**Audio (when enabled)**
- `audio.parameters.get` / `audio.parameters.set`
- `audio.subscribe` / `audio.unsubscribe`
- `audio.zone-agc.get` / `audio.zone-agc.set`
- `audio.spike-detection.get` / `audio.spike-detection.reset`

**Streaming**
- `ledStream.subscribe` / `ledStream.unsubscribe`
- `validation.subscribe` / `validation.unsubscribe`
- `benchmark.subscribe` / `benchmark.unsubscribe`
- `benchmark.start` / `benchmark.stop` / `benchmark.get`

**System / misc**
- `sys.capabilities`
- `batch`
- `plugins.list` / `plugins.stats` / `plugins.reload`
- `ota.check` / `ota.begin` / `ota.chunk` / `ota.abort` / `ota.verify`
- `narrative.getStatus` / `narrative.config`
- `motion.*` (phase/speed/momentum suite)
- `debug.audio.get` / `debug.audio.set`
- `auth.status` / `auth.rotate` (when auth enabled)
- `trinity.beat` / `trinity.macro` / `trinity.segment` / `trinity.sync`

---

### 8.4 REST API v1 surfaces (high-impact for controllers/dashboards)
Routes are registered in `firmware/v2/src/network/webserver/V1ApiRoutes.cpp`.

**Effects**
- `GET /api/v1/effects` (list)
- `GET /api/v1/effects/current`
- `POST /api/v1/effects/set` (set current effect)
- `PUT /api/v1/effects/current` (compat)
- `GET /api/v1/effects/metadata?id=N`
- `GET /api/v1/effects/parameters?id=N`
- `POST|PATCH /api/v1/effects/parameters` (set per-effect parameters)
- `GET /api/v1/effects/families`

**Global parameters**
- `GET /api/v1/parameters`
- `POST|PATCH /api/v1/parameters`

**Transitions**
- `GET /api/v1/transitions/types`
- `POST /api/v1/transitions/trigger`
- `GET /api/v1/transitions/config`
- `POST /api/v1/transitions/config`

**Zones**
- `GET /api/v1/zones`
- `POST /api/v1/zones/layout`
- `POST /api/v1/zones/enabled`
- `GET /api/v1/zones/config`
- `POST /api/v1/zones/config/save`
- `POST /api/v1/zones/config/load`
- `GET /api/v1/zones/timing`
- `POST /api/v1/zones/timing/reset`
- `POST /api/v1/zones/reorder`

---

## 9) State ownership and CQRS surface

### 9.1 Identify authoritative state owners
- **Likely owners:**
  - RendererActor for render/effect selection and render parameters.
  - ZoneComposer/ZoneConfigManager for zone layouts.
  - NVS for persisted snapshots (loaded at boot).
  - WebServer only as an ingress/egress layer (should not own truth).
- **Where to look:** `core/*`, `core/actors/*`, `core/persistence/*`, `plugins/*`

### 9.2 Commands vs queries
- **What to document:**
  - Where commands enter (serial, REST, WS).
  - Where state is read for status responses (WS “status” messages, REST “current state” endpoints).
  - Any “status broadcast cadence” and what payload it contains.

---

## 10) Validation, profiling, and safety tooling

### 10.1 Validation framework
- **Files:** `firmware/v2/src/validation/*`
- **What to extract:**
  - What validators exist (heap delta, frame timing, effect invariants).
  - How they are triggered (serial command, build flag, test harness).

### 10.2 Monitoring components
- **Files:** `firmware/v2/src/core/system/*` (StackMonitor, HeapMonitor, MemoryLeakDetector, ValidationProfiler)
- **What to extract:**
  - What metrics exist and where they’re logged.
  - Any periodic telemetry format (JSON lines etc.).

---

## 11) Practical “surfaces” checklist (what an agent should produce)

### 11.1 Command ingress surfaces
- **Serial**: commands that change effect/palette/speed/zones/transitions/diagnostics.
- **REST v1**: endpoints that set state and those that query state.
- **WebSocket**: message types (set current effect, set parameters, zone commands, transition trigger, list metadata).

### 11.2 State surfaces
- **Volatile** (RAM): current effect, live parameters, render buffers, audio snapshots.
- **Persisted** (NVS): system state, zone config, saved networks (if any), user presets.

### 11.3 Render surfaces
- **Per-frame hot paths** (must stay allocation-free): effect render, zone composition, transitions, LED flush.

---

## 12) Build environments and test surfaces
- **Main build:** `pio run -d firmware/v2 -e esp32dev_audio_esv11`
- **Native tests:** `pio run -d firmware/v2 -e native_test` (host testing surface)
- **What to document:** which subsystems are covered by host tests and which require hardware.

---

## 13) Cross-project linkage: v2 ↔ Tab5.encoder
Tab5.encoder is a **client** of v2’s control surface:
- It consumes v2 WebSocket messages (especially `status`, `effects.list`, `palettes.list`).
- It emits v2 control commands (effect selection, parameter updates, zone commands, transition/config commands).

The authoritative mapping table belongs in the Tab5.encoder document, but v2 should expose a stable, versioned command surface (prefer v1 style message schemas and consistent response envelopes).
