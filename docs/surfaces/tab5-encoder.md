# Tab5.encoder (ESP32-P4) — Control Surface Map (Granular)
**Scope:** `firmware/Tab5.encoder/` only. This document specifies the Tab5 controller firmware: build constraints, hardware, boot wiring, input → parameter model → UI → network synchronisation, presets, persistence, OTA, and failure modes.

Tab5.encoder is a **controller**, not the renderer. It manipulates the LightwaveOS v2 device via **WebSocket** (and optionally HTTP/OTA).

---

## 1) Identity, role, and contract with v2

### 1.1 What Tab5.encoder is
- **A dedicated embedded UI** (LVGL) for controlling LightwaveOS v2 parameters and zones.
- **Primary capabilities:**
  - 16-encoder input surface (2× M5ROTATE8) + touch
  - WebSocket client with bidirectional state sync (anti-snapback)
  - Presets and NVS persistence
  - Colour correction controls (gamma / mode / auto exposure / brown guardrail)

### 1.2 What Tab5.encoder is not
- Not the LED renderer.
- Not the authority for system truth; v2 remains the authoritative device state.

---

## 2) Hardware specification

### 2.1 Board
- **Device:** M5Stack Tab5
- **SoC:** ESP32-P4 (RISC-V)
- **Display:** 5" 800×480 (ILI9881C or ST7123 auto-detected)
- **UI:** LVGL 9.3.0 + M5Unified/M5GFX
- **Acceleration:** LGFX_PPA is used for fast blits/transforms

### 2.2 WiFi architecture
- **WiFi co-processor:** ESP32-C6 via SDIO (Tab5-specific)
- **Critical ordering:** `WiFi.setPins()` must run **before** `WiFi.begin()` or `M5.begin()`.
- **Pins defined in:** `src/config/Config.h`

### 2.3 Encoders
- **Hardware:** 2× M5ROTATE8 (8 encoders each = 16 total)
- **Addresses:**
  - Unit A: `0x42` (reprogrammed)
  - Unit B: `0x41` (factory default)
- **I2C buses/pins (current code):**
  - Unit A bus: SDA 53, SCL 54 (Grove Port.A external I2C)
  - Unit B bus: SDA 49, SCL 50 (secondary external bus)
  - Frequency: 100 kHz; timeout: 200 ms

> **Safety note:** internal Tab5 I2C is shared with display/touch/audio. Recovery logic must only target external bus(es).

---

## 3) Build system (hard constraints)

### 3.1 PlatformIO config and toolchain injection
- **PlatformIO config:** `firmware/Tab5.encoder/platformio.ini`
- **Pre-build hook (mandatory):** `firmware/Tab5.encoder/scripts/pio_pre.py`

### 3.2 Correct build invocation (mandatory)
Run from repository root (do not `cd` into the folder):

```bash
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -d firmware/Tab5.encoder
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem101 -d firmware/Tab5.encoder
pio device monitor -p /dev/cu.usbmodem101 -d firmware/Tab5.encoder -b 115200
```

---

## 4) Repo entrypoints (what to read first)

### 4.1 Boot + orchestration
- **File:** `firmware/Tab5.encoder/src/main.cpp`
- **Extract:**
  - global singleton ownership (`g_encoders`, `g_wifiManager`, `g_wsClient`, `g_paramHandler`, `g_ui`, preset machinery)
  - the main loop cadence (encoder poll, UI tick, network update)
  - screen-aware routing (Zone Composer screen intercepting encoder deltas)
  - list-fetch flow for effect/palette names (paged)

### 4.2 Configuration tables (source of truth)
- **Files:**
  - `firmware/Tab5.encoder/src/config/Config.h`
  - `firmware/Tab5.encoder/src/parameters/ParameterMap.*`
  - `firmware/Tab5.encoder/src/config/network_config.h`

---

## 5) Input surface

### 5.1 DualEncoderService (unified 16-encoder interface)
- **File:** `firmware/Tab5.encoder/src/input/DualEncoderService.h` (+ implementation details inlined)
- **Responsibilities:**
  - Poll both encoder units and unify into indices `0..15`
  - Normalise raw deltas using `DetentDebounce`
  - Handle button presses (default reset-to-default, unless ButtonHandler intercepts)
  - Provide LED feedback on encoder activity (flash) and status LEDs per unit

### 5.2 Encoder delta normalisation / constraints
- **Files:** `firmware/Tab5.encoder/src/input/EncoderProcessing.*`
- **What to understand:**
  - Which indices wrap vs clamp (effect/palette wrap; brightness clamps, etc.)
  - How detent bounce is handled (M5ROTATE8 quirk normalisation)

### 5.3 Button behaviours and mode toggles
- **Files:** `firmware/Tab5.encoder/src/input/ButtonHandler.*`, `ClickDetector.*`
- **What to understand:**
  - Speed↔Palette toggling for zone encoder pairs (9/11/13/15)
  - Preset click taxonomy (single/double/hold mapping)
  - Any “zone mode” vs “global mode” behaviour

### 5.4 Touch surface
- **Files:** `firmware/Tab5.encoder/src/input/TouchHandler.*`
- **What to understand:**
  - Long-press actions (reset, navigation to network config, etc.)
  - Touch-to-parameter mapping and UI feedback strategy

### 5.5 I2C recovery surface
- **Files:** `firmware/Tab5.encoder/src/input/I2CRecovery.*`
- **What to understand:**
  - Which bus is recoverable and the exact method (SCL toggling / Wire reinit)
  - Forbidden operations on internal I2C / peripheral resets

---

## 6) Parameter model (single source of truth)

### 6.1 Parameter index space (what “index” means)
Tab5 models **16 parameters** aligned with encoder indices:
- **Indices 0–7:** global LightwaveOS parameters
- **Indices 8–15:** zone parameters (effect + speed/palette per zone)

**Where defined:** `src/parameters/ParameterMap.*` and `src/config/Config.h`.

### 6.2 ParameterMap table (structural contract)
- **File:** `firmware/Tab5.encoder/src/parameters/ParameterMap.cpp`
- **Each entry defines:**
  - `encoderIndex` (0–15)
  - `statusField` (field name in v2’s WS status payload)
  - `wsCommandType` (command family)
  - `min/max/default`

### 6.3 Complete encoder → parameter → WebSocket mapping table (as implemented)
Source of truth: `firmware/Tab5.encoder/src/parameters/ParameterMap.cpp` (table) + `ParameterHandler::sendParameterChange(...)` (routing).

**Inbound status fields are taken from v2 WebSocket broadcast** `{"type":"status", ...}` and v2 zone broadcast `{"type":"zones.list", ...}`. (See `firmware/v2/src/network/WebServer.cpp`.)

#### 6.3.1 Global parameters (Unit A, indices 0–7)
| Enc idx | Name | Range | Default | Wrap/Clamp | Inbound status field | Outbound WS command |
|---:|---|---|---:|---|---|---|
| 0 | Effect | 0–99 (Tab5 table) | 0 | Wrap | `effectId` | `effects.setCurrent` `{ effectId }` |
| 1 | Palette | 0–74 | 0 | Wrap | `paletteId` | `parameters.set` `{ paletteId }` (Tab5 uses generic params surface) |
| 2 | Speed | 1–100 | 25 | Clamp | `speed` | `parameters.set` `{ speed }` |
| 3 | Mood | 0–255 | 0 | Clamp | `mood` | `parameters.set` `{ mood }` |
| 4 | FadeAmount | 0–255 | 0 | Clamp | `fadeAmount` | `parameters.set` `{ fadeAmount }` |
| 5 | Complexity | 0–255 | 128 | Clamp | `complexity` | `parameters.set` `{ complexity }` |
| 6 | Variation | 0–255 | 0 | Clamp | `variation` | `parameters.set` `{ variation }` |
| 7 | Brightness | 0–255 | 128 | Clamp | `brightness` | `parameters.set` `{ brightness }` |

**Notes:**
- Tab5’s `ParameterMap` currently hardcodes effect range `0..99`. v2 validates against live `renderer->getEffectCount()`. If v2 has >100 effects, Tab5 should consume `effects.list.pagination.total` (or a v2 capabilities field) and call `updateParameterMetadata(...)` to expand the max.
- v2’s `status` payload includes `hue`, `intensity`, `saturation`, `fps`, `cpuPercent`, `freeHeap`, `uptime`, plus `bpm/mic/key` when audio enabled. Tab5 may display a subset.

#### 6.3.2 Zone parameters (Unit B, indices 8–15)
Tab5 models zones as **per-zone effect + speed**, but the “speed” encoder can be toggled to drive **palette** instead. The toggle is owned by `ButtonHandler` and consulted in `ParameterHandler::sendParameterChange(...)`.

| Enc idx | Name | Range | Default | Wrap/Clamp | Inbound zone field | Outbound WS command |
|---:|---|---|---:|---|---|---|
| 8 | Zone0Effect | 0–99 | 0 | Wrap | `zones[].effectId` for zone 0 | `zone.setEffect` `{ zoneId:0, effectId }` |
| 9 | Zone0Speed/Palette | 1–100 | 25 | Clamp | `zones[].speed` (or `paletteId` when toggled) | `zone.setSpeed` `{ zoneId:0, speed }` **or** `zone.setPalette` `{ zoneId:0, paletteId }` |
| 10 | Zone1Effect | 0–99 | 0 | Wrap | `zones[].effectId` for zone 1 | `zone.setEffect` `{ zoneId:1, effectId }` |
| 11 | Zone1Speed/Palette | 1–100 | 25 | Clamp | `zones[].speed` / `paletteId` | `zone.setSpeed` **or** `zone.setPalette` |
| 12 | Zone2Effect | 0–99 | 0 | Wrap | `zones[].effectId` for zone 2 | `zone.setEffect` `{ zoneId:2, effectId }` |
| 13 | Zone2Speed/Palette | 1–100 | 25 | Clamp | `zones[].speed` / `paletteId` | `zone.setSpeed` **or** `zone.setPalette` |
| 14 | Zone3Effect | 0–99 | 0 | Wrap | `zones[].effectId` for zone 3 | `zone.setEffect` `{ zoneId:3, effectId }` |
| 15 | Zone3Speed/Palette | 1–100 | 25 | Clamp | `zones[].speed` / `paletteId` | `zone.setSpeed` **or** `zone.setPalette` |

**Zones enable + layout are separate commands** (not mapped to an encoder by default):
- `zone.enable` `{ enable: true|false }`
- `zones.setLayout` `{ zoneCount, segments[] }` (see v2 handler `WsZonesCommands.cpp`)

### 6.4 Anti-snapback (echo suppression) contract
Tab5 defends against “server echo snapping the encoder back” using a per-index holdoff:
- Implemented in `firmware/Tab5.encoder/src/parameters/ParameterHandler.*`
- Behaviour:
  - call `markLocalChange(index)` on local encoder changes
  - ignore inbound `status` updates for that field for ~1 second

**This aligns with v2:**
- v2 broadcasts `status` and `zones.list` periodically and after changes.
- The holdoff window prevents oscillation from near-simultaneous local send and server broadcast.

### 6.5 ParameterHandler (business logic & anti-snapback)
- **Files:** `firmware/Tab5.encoder/src/parameters/ParameterHandler.*`
- **Responsibilities:**
  - Apply encoder change: clamp → cache → display notify → send command (if connected)
  - Apply inbound status: update cache → update encoder values without callback
  - **Anti-snapback:** 1s holdoff per param after local change (ignore server echo)
  - Zone speed/palette dual-mode: consult ButtonHandler to choose `zone.setSpeed` vs `zone.setPalette`

---

## 7) Networking: WiFi + discovery + WebSocket protocol

### 7.1 Network config and fallback strategy
- **File:** `firmware/Tab5.encoder/src/config/network_config.h`
- **Contract surfaces:**
  - Primary SSID/pass via build flags (local `wifi_credentials.ini`)
  - Secondary SSID/pass for v2 AP fallback (`LightwaveOS-AP`)
  - Hostname: `lightwaveos.local`, port 80, WS path `/ws`
  - Fallback priority:
    1) compile-time `LIGHTWAVE_IP` (bypass mDNS)
    2) manual IP from NVS
    3) mDNS resolution
    4) configured fallback IP
  - NVS namespace `tab5net` and keys for manual IP toggles

### 7.2 WiFiManager (state machine surface)
- **Files:** `firmware/Tab5.encoder/src/network/WiFiManager.*`
- **Understand:**
  - States: DISCONNECTED → CONNECTING → CONNECTED → MDNS_RESOLVING → MDNS_RESOLVED
  - Backoff timing (`NetworkConfig::*`)
  - mDNS resolution strategy and retry cadence
  - Stub behaviour when WiFi is disabled (compile-time)

### 7.3 WebSocketClient (message contract surface)
- **Files:** `firmware/Tab5.encoder/src/network/WebSocketClient.*`
- **Outbound command families (explicit methods):**
  - `effects.setCurrent` (`sendEffectChange`)
  - `parameters.set` (brightness/speed/mood/fade/complexity/variation/palette)
  - zones:
    - `zone.setEffect`
    - `zone.setBrightness`
    - `zone.setSpeed`
    - `zone.setPalette`
    - `zone.setBlend`
    - `zones.setLayout`
  - metadata:
    - `effects.list` (paged)
    - `palettes.list` (paged)
  - status:
    - `getStatus`
    - `requestZonesState`
  - colour correction:
    - `colorCorrection.getConfig`
    - `colorCorrection.setConfig`
    - `colorCorrection.setMode`

**Transport properties:**
  - Non-blocking `update()` (calls third-party `_ws.loop()`; guarded with watchdog resets)
  - Exponential reconnect backoff (1s → 30s cap)
  - Per-parameter rate limit (50ms)
  - Send queue (16 slots) + stale timeout (drop >500ms)
  - Mutex protecting JSON buffer during send

### 7.4 Inbound message handling
- **Entry:** callback set via `WebSocketClient::onMessage(...)` in `main.cpp`
- **Key inbound types (observed in `main.cpp`):**
  - `status`: bulk state sync + audio footer metrics extraction (BPM/key/mic dB)
  - `effects.list`: paged list; caches names and updates UI labels
  - `palettes.list`: paged list; caches names and updates UI labels
  - `colorCorrection.getConfig` / `setConfig` / `setMode`: confirmation + UI sync
  - `error`: prints and flashes red LED feedback

### 7.5 Expected v2 broadcast payloads Tab5 depends on
From v2 firmware (`firmware/v2/src/network/WebServer.cpp`):
- **`type: "status"`**:
  - `effectId`, `effectName`, `brightness`, `speed`, `paletteId`,
    `mood`, `fadeAmount`, `complexity`, `variation` (plus `hue/intensity/saturation`)
  - audio (optional): `bpm`, `mic`, `key`
- **`type: "zones.list"`**:
  - `enabled`, `zoneCount`, `segments[]`, `zones[]` with per-zone:
    `effectId`, `effectName`, `brightness`, `speed`, `paletteId`, `blendMode`, `blendModeName`
    plus zone-audio fields (`tempoSync`, `beatModulation`, etc.)


---

## 8) UI system (LVGL)

### 8.1 LVGL bridge and config
- **Files:** `firmware/Tab5.encoder/src/ui/lvgl_bridge.*`, `firmware/Tab5.encoder/src/ui/lv_conf.h`
- **Understand:**
  - How M5GFX draws into LVGL (tick integration, display flush strategy)
  - Performance implications (PPA usage where relevant)

### 8.2 UI orchestration and screens
- **Files:** `firmware/Tab5.encoder/src/ui/DisplayUI.*`, `ConnectivityTab.*`, `ZoneComposerUI.*`
- **Understand:**
  - Screen model (`UIScreen` enum in UI layer) and how it changes routing behaviour.
  - How parameter values are displayed and highlighted on change.
  - How the UI requests network config editing (manual IP screen).

### 8.3 Widgets
- **Directory:** `firmware/Tab5.encoder/src/ui/widgets/*`
- **Key widgets:**
  - Header (connection state)
  - Gauges (radial parameter visualisation)
  - Preset bank/slot widgets
  - Action row (touch buttons for colour correction controls)

### 8.4 Theme and typography
- **Files:** `firmware/Tab5.encoder/src/ui/Theme.h`, `src/ui/fonts/*`, `src/fonts/*`
- **Understand:**
  - Colour palette, spacing, typefaces, “cyberpunk” styling decisions
  - Which fonts are used for which UI components

---

## 9) Presets and persistence

### 9.1 Preset capture and application
- **Files:** `firmware/Tab5.encoder/src/presets/PresetManager.*`
- **Understand:**
  - What is captured (global params, zone params, colour correction state)
  - Slot behaviour (8 slots)
  - Button click taxonomy (single/double/hold)

### 9.2 NVS storage (local controller-side persistence)
- **Files:** `firmware/Tab5.encoder/src/storage/NvsStorage.*`, `PresetStorage.*`, `PresetData.h`
- **Understand:**
  - Which keys are used, namespaces, and write debounce timing
  - Separation between “controller state” and “server state”

---

## 10) OTA update surface
- **Files:** `firmware/Tab5.encoder/src/network/OtaHandler.*`, `firmware/Tab5.encoder/upload_ota.sh`
- **Understand:**
  - Token-based auth (OTA_UPDATE_TOKEN)
  - HTTP endpoint path and payload expectations
  - How OTA updates interact with WebSocket connection (shutdown/restart behaviour)

---

## 11) Failure modes and guardrails (must document)

### 11.1 I2C failure modes
- Device missing (wrong address)
- Bus stuck (requires recovery)
- Partial availability (Unit A present, Unit B missing): graceful degradation expected

### 11.2 WebSocket “snapback” loops
- Local change echoed back as status and overwrites UI/encoder state.
- **Mitigation:** `ParameterHandler` per-index holdoff + explicit `markLocalChange`.

### 11.3 mDNS unreliability
- Fallback IP stack:
  - Compile-time override → NVS manual IP → mDNS → fallback IP.
- Document how to set each tier (build flags vs UI config).

### 11.4 Watchdog sensitivity
- Third-party `_ws.loop()` can block; code resets watchdog before/after and warns on slow loops.

---

## 12) Concrete “control surface” outputs the agent should produce

### 12.1 Parameter-to-command table (minimum artefact)
- Index `0..15`
- Name shown in UI
- Range (min/max/default)
- Wrap vs clamp
- v2 status field name (inbound)
- v2 command type (outbound)
- Any mode-dependent routing (zone speed vs palette)

### 12.2 Message-type catalogue
List every WS message type Tab5 expects and emits, with:
- required fields
- optional fields
- request/response pairing (requestId usage)
- error envelope expectations

### 12.3 UI routing diagram
Show when encoders:
- control global params
- control zones
- are intercepted by Zone Composer screen for delta-based editing

