# LightwaveOS v2 (ESP32‑S3) — Minimum Control Surface Artefacts (Grounded in Repo Code)

**Scope:** `firmware/v2/` renderer firmware.  
**Purpose:** Provide the “minimum artefacts” needed for safe agent work: ingress catalogue, state ownership, mediator graph, and output surfaces.

---

## 1) Inputs (ingress surfaces)

### 1.1 Serial (human-in-the-loop)
- **Where:** `firmware/v2/src/main.cpp` (serial loop + command parsing)
- **What it can change:** effect selection, palette, speed, brightness, debug domains, capture/validation, show playback.

### 1.2 REST API (HTTP)
- **Where:** `firmware/v2/src/network/WebServer.*` + route handlers under `firmware/v2/src/network/webserver/handlers/*`
- **Docs:** `docs/api/api-v1.md`
- **Characteristic:** request handlers must never mutate renderer buffers directly; they push changes via ActorSystem commands.

### 1.3 WebSocket (real-time)
- **Where:** `firmware/v2/src/network/webserver/ws/*`
  - e.g. `WsPaletteCommands.cpp`, `WsColorCommands.cpp`, etc.
- **Characteristic:** WS is the main “control plane” for Tab5 + web dashboards.

### 1.4 Saved state (NVS)
- **Where:** `firmware/v2/src/core/persistence/*`:
  - `NVSManager.*`
  - `ZoneConfigManager.*`
- **Loaded during boot** (in `main.cpp`): zone layout + system state restored before live operation.

### 1.5 Audio (I2S capture → DSP)
- **Where:** `firmware/v2/src/audio/*`
- **Export surface:** lock-free `SnapshotBuffer<ControlBusFrame>` read by Renderer on Core 1.

---

## 2) Mediators (the “control plane plumbing”)

### 2.1 ActorSystem (Core 0)
- **Where:** `firmware/v2/src/core/actors/ActorSystem.*`
- **Role:** lifecycle + convenience command façade.
- **Backpressure:** rejects commands when Renderer queue utilization >= 90%.

### 2.2 RendererActor (Core 1, hot path)
- **Where:** `firmware/v2/src/core/actors/RendererActor.*`
- **Role:** single source of truth for rendering:
  - effect registry (`IEffect*`), current effect ID
  - LED buffers
  - transitions
  - zone mode integration
  - audio snapshot reads

### 2.3 ZoneComposer
- **Where:** `firmware/v2/src/effects/zones/ZoneComposer.*`
- **Role:** buffer-proxy multi-zone render + composition.

### 2.4 TransitionEngine
- **Where:** `firmware/v2/src/effects/transitions/*`
- **Role:** 3-buffer transition blending on render thread.

### 2.5 ShowDirectorActor + NarrativeEngine
- **Where:**
  - `firmware/v2/src/core/actors/ShowDirectorActor.*`
  - `firmware/v2/src/core/shows/*`
  - `firmware/v2/src/core/narrative/*`
- **Role:** 20Hz cue scheduling + parameter sweeps; modulates narrative tension.

### 2.6 WebServer (ingress/egress only)
- **Where:** `firmware/v2/src/network/WebServer.*`
- **Important implementation detail:** WebServer maintains a **CachedRendererState** refreshed in `update()` so request handlers don’t do unsafe cross-core calls.

---

## 3) Outputs (what the system emits)

### 3.1 LED output
- Unified 320 buffer → strip buffers → FastLED.show.
- **Owner:** RendererActor only.

### 3.2 Status / telemetry
- Periodic WS status broadcasts and on-demand REST queries.
- Optional binary streams (LED frames, audio frames, logs) depending on feature flags.

---

## 4) Authoritative state ownership (who owns truth)

**Runtime truth:**
- **RendererActor**: current effect, params, transitions, effect registry.
- **ZoneComposer**: zone layout + per-zone state (effect/speed/palette/blend).

**Persisted truth:**
- **ZoneConfigManager + NVS**: zone layout + system state (effect/brightness/speed/palette).

**Ingress-only / not authoritative:**
- WebServer / dashboards / Tab5.

---

## 5) Known drift vectors (now pinned)

1) **Centre-point duplication** (real drift vector):
- `effects/CoreEffects.h`: `CENTER_LEFT=79`, `CENTER_RIGHT=80`
- `core/actors/RendererActor.h`: `LedConfig::CENTER_POINT=79`
- `effects/transitions/TransitionEngine.h`: `CENTER_POINT=79`
- `plugins/api/EffectContext.h`: centre-related helpers/fields

2) **Palette-range drift** (previously real; partially fixed in code as of this repo state):
- Master palette system: `palettes/Palettes_Master.h` → `MASTER_PALETTE_COUNT=75`
- Persistence and CQRS/state must validate against that count.

---

## 6) Feature flag surface (what compiles)

- `firmware/v2/src/config/features.h` defines defaults, but the primary build env enables features via `firmware/v2/platformio.ini`.
- Critical toggles:
  - `FEATURE_WEB_SERVER`
  - `FEATURE_AUDIO_SYNC`
  - `FEATURE_EFFECT_VALIDATION`
  - `FEATURE_TRANSITIONS`
  - `FEATURE_ZONE_SYSTEM`

