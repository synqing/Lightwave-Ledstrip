---
abstract: "Multi-platform LED control ecosystem with ESP32-S3 firmware (actor model), iOS/Tab5 companion apps, and web tooling. Core pattern: audio-driven visual effects via CQRS state management and message-based actor coordination."
---

# LightwaveOS Architecture

**Analysis Date:** 2026-03-21

## Pattern Overview

**Overall:** Actor model with CQRS state management, event-driven asynchronous communication, and audio-reactive visual pipeline.

**Key Characteristics:**
- Firmware core: FreeRTOS tasks pinned to dual cores (Core 0: I/O, Core 1: rendering)
- State: Single immutable `SystemState` accessed via CQRS, double-buffered for lock-free reads
- Audio pipeline: I2S capture → ESV11/PipelineCore processing → `ControlBus` snapshots → effects → LEDs
- Network: REST API + WebSocket for real-time control, UDP for high-frequency LED/audio streaming
- Platforms: ESP32-S3 firmware (C++), iOS (Swift/SwiftUI), M5Stack Tab5 (C++ with LVGL)

## Layers

**Application Layer (Effects & Visualization):**
- Purpose: 350+ LED effect implementations, zone composition, transitions, narrative sequencing
- Location: `firmware-v3/src/effects/` (52,980 LOC), `firmware-v3/src/core/shows/` (show scheduling)
- Contains: `IEffect` interface implementations, enhancement processors (EdgeMixer, SmoothingEngine), zone blending, transition engine
- Depends on: `ControlBus` for audio state, `RenderContext` for per-frame data, persistence for effect metadata
- Used by: `RendererActor` for frame-by-frame rendering

**Audio Reactive Pipeline:**
- Purpose: Microphone capture → frequency analysis → beat/tempo tracking → musical intelligence extraction
- Location: `firmware-v3/src/audio/` (15,566 LOC)
- Contains:
  - I2S DMA interface and audio capture
  - Backend implementations: ESV11 (64-bin Goertzel, stable/production) and PipelineCore (FFT, broken for beat tracking)
  - Beat tracking, tempo estimation, noise calibration, silence gating
  - `ControlBus` snapshot production (8 octave bands, 12 chroma bins, RMS, beat events, onset detection)
- Depends on: Hardware I2S, FreeRTOS tasks
- Used by: Effects via snapshot reads, visualization mapping logic

**Control Bus (Shared Audio State):**
- Purpose: Thread-safe, immutable snapshot of audio analysis for all effects to consume
- Location: `firmware-v3/src/audio/contracts/ControlBus.h` (defines 30+ fields)
- Pattern: Snapshot-based access, mutex-protected writes by `AudioActor`, lock-free reads by renderer
- Fields: `bands[0..7]` (octave energy), `chroma[0..11]` (pitch classes), `rms`, `beat`, `onset`, `tempo`, `phase`, `confidence`
- Access: Effects call `controlBus.getControlBusFrameSnapshot()` instead of `getControlBusRef()` (deprecated, data race source)

**Rendering Engine (RendererActor):**
- Purpose: Core 1 task, 120 FPS frame generation, effect orchestration, zone composition
- Location: `firmware-v3/src/core/actors/RendererActor.h/cpp`
- Responsibilities:
  - Frame loop: reads effect/zone state, invokes `IEffect::render()` for each zone
  - Creates `RenderContext` with: `leds[]` buffer, `dt`, `zoneId`, `controlBus` snapshot, effect parameters
  - Manages transitions between effects
  - Blends zones via `ZoneComposer` and `BlendMode`
  - Hands off LED buffer to `FastLedDriver::show()`
- Constraints: Must complete < 2ms per frame, no heap allocation, no logging on critical path

**Network API Layer:**
- Purpose: REST endpoints, WebSocket real-time control, UDP streaming for LED/audio frames
- Location: `firmware-v3/src/network/` (27,369 LOC)
- REST routes: `firmware-v3/src/network/webserver/v1_routes/` (effect, palette, parameter, zone, transition, audio endpoints)
- WebSocket: `WsGateway`, `WsCommandRouter`, message type enum (19 message types)
- UDP: `UdpStreamer` for 960-byte LED frames + audio metrics
- Used by: iOS, Tab5, web dashboards

**Persistence Layer:**
- Purpose: NVS (Non-Volatile Storage) management, effect presets, zone configurations, WiFi credentials
- Location: `firmware-v3/src/core/persistence/` (2,700 LOC)
- Manages: `NVSManager` (key-value store), `ZoneConfigManager`, `EffectPresetManager`, WiFi credential encryption
- Accessed by: Network handlers for preset save/load, ShowDirectorActor for show persistence

**Actor System (FreeRTOS Tasks):**
- Purpose: Cross-core task coordination, pub/sub message bus
- Location: `firmware-v3/src/core/actors/` (5,400 LOC)
- Actors:
  - `AudioActor` (Core 0): I2S capture, DSP pipeline, `ControlBus` updates
  - `RendererActor` (Core 1): Effect rendering, LED output
  - `ShowDirectorActor`: Show playback, cue scheduling
  - `CommandActor`: Serial/REST command dispatch
  - `PluginManagerActor`: Runtime effect loading (if enabled)
- Message Bus: Singleton, lock-free publish, 16-byte fixed message format, 32 tracked types, 8 subscribers per type
- Used by: All subsystems for asynchronous coordination

**iOS Network & State (SwiftUI, Swift 6):**
- Purpose: Companion app for visualization control, device discovery, real-time parameter adjustment
- Location: `lightwave-ios-v2/` (12,200 LOC Swift)
- Architecture: MVVM with `@Observable` ViewModels on `@MainActor`
- Network services (all `actor` for thread safety):
  - `RESTClient`: REST calls with URL-based debouncing
  - `WebSocketService`: WebSocket connection, subscription management
  - `UDPStreamReceiver`: UDP listener for LED frames (primary for streaming)
  - `DeviceDiscoveryService`: Bonjour browsing for `_http._tcp` services
- State flow: ConnectionState (disconnected → discovering → connecting → connected) → WebSocket events → ViewModel updates
- Used by: Tab views (Play, Zones, Audio, Device) for parameter binding and status display

**Tab5 Encoder (M5Stack Tab5, C++ with LVGL):**
- Purpose: Physical encoder interface for rotary control, local effect/parameter adjustment
- Location: `tab5-encoder/` (PlatformIO project)
- Architecture: HAL layer (encoder input), UI layer (LVGL), network communication to K1 AP
- Key modules: `network/` (WiFi, REST client), `ui/` (menu tree, parameter sliders), `hal/` (I2C encoder, display)
- Used by: Physical interface for local control

## Data Flow

**Audio-to-Visual (Primary Reactive Path):**

1. Microphone → I2S DMA (Core 0, `AudioActor`)
2. ESV11 DSP: 64-bin Goertzel or FFT (backend selectable)
3. BeatTracker: Onset detection, tempo estimation (from Goertzel bins or FFT)
4. `ControlBus` double-buffer swap: Immutable snapshot with beat/tempo/chroma/bands
5. `RendererActor` reads snapshot (lock-free) on Core 1
6. Each effect's `render(RenderContext ctx)` reads `ctx.controlBus.bands[]`, `chroma[]`, `beat`, etc.
7. Effect modulates LED output in real time, respecting centre-origin topology (LEDs 79/80)
8. FastLED → RMT driver → WS2812 parallel dual-strip transmission (~5-6ms)
9. UDP frame streaming to iOS: 960B LED buffer + 32B audio metrics trailer

**Control Command Flow (REST/WebSocket):**

1. iOS sends REST request (e.g., `POST /api/v1/effects/{id}`) or WebSocket message
2. `WebServer::update()` dispatches to route handler (Core 0, async)
3. Handler validates, calls state mutation (StateStore command dispatch)
4. Command applied to `SystemState`, version incremented
5. Subscribers notified (effects, transitions, shows)
6. `RendererActor` polls state, changes effect on next frame
7. Transition engine begins if configured
8. WebSocket broadcasts status to all clients (new state)

**Show Playback (Scheduled Cues):**

1. `ShowDirectorActor` plays PROGMEM show bundle
2. `CueScheduler` checks timeline, fires ready cues (up to 4 per tick)
3. Cue types: EFFECT, PARAMETER_SWEEP, ZONE_CONFIG, TRANSITION, NARRATIVE, PALETTE, MARKER
4. Each cue calls StateStore command (e.g., SetEffectCommand)
5. State updated, `RendererActor` applies on next frame
6. Timeline advances (optional: sync to audio beat)

**State Management (CQRS):**

1. `StateStore` holds double-buffered `SystemState[2]` (active, inactive)
2. Command handler calls `command.apply(currentState)` → new immutable state
3. Write mutex protects swap
4. Readers call `getState()` → always get stable pointer to active buffer
5. State version field allows optimistic concurrency checks
6. All mutations are: command → validation → apply → version increment → subscriber notification

**Zone Composition:**

1. `RendererActor` loops zones (0..3)
2. For each zone: reads effect ID, palette, parameters
3. Creates zone-specific `RenderContext`: `zoneId`, `leds[40]` (per-zone 160-LED half of strip)
4. Calls effect `render(ctx)` — effect checks `ctx.zoneId < kMaxZones` else uses global (0xFF) fallback
5. `ZoneComposer` blends zone outputs with `BlendMode` (mix, add, multiply, etc.)
6. Final `leds[320]` sent to FastLED

**Error Handling:**

**Strategy:** Defensive isolation with fallback-to-safe defaults

**Patterns:**
- Out-of-bounds zone access: bounds-check `ctx.zoneId < kMaxZones`, fallback to global
- Audio pipeline failure: silence gate closes, effect receives zero bands
- Network loss: local effect playback continues, shows pause
- Memory pressure: heap shedding reduces effect complexity, no panic
- Serial/WiFi desync: acknowledges commands, re-sends state on next poll

## Cross-Cutting Concerns

**Logging:**
- Framework: `Log.h` with tag prefix (e.g., `#define LW_LOG_TAG "Main"`)
- Pattern: `lwLogInfo("Message %d", value)` or `lwLogError("Failed: %s", reason)`
- Disabled in render loop by default (`FEATURE_*` gates in `config/features.h`)

**Validation:**
- Commands: Each `ICommand` subclass implements `validate(const SystemState&)` before apply
- API input: JSON schema validation in handlers, range checks on numeric parameters
- Audio: Noise floor calibration, silence gate with hysteresis (open @ rms >= 0.02, close @ < 0.005)

**Authentication:**
- WiFi: Open network by design (K1 is AP, clients on same LAN)
- REST: No per-endpoint auth (assume trusted LAN), rate limiting by IP (20/sec HTTP, 50/sec WebSocket)
- OTA: Token-based verification, optional signed binary (see `OtaTokenManager`)

**Performance:**
- Real-time: Core 1 RendererActor never blocks, render < 2ms ceiling
- Networking: Core 0 WebServer, async handlers, no blocking I/O on critical path
- Memory: Static buffers, no heap in render, pre-allocated message queues
- Profiling: Optional `FEATURE_VALIDATION_PROFILING` gate for frame timing telemetry

## Key Abstractions

**IEffect (Plugin Interface):**
- Purpose: Standard interface for all visual effects
- Location: `firmware-v3/src/plugins/api/IEffect.h`
- Pattern: Virtual base class, 350+ implementations in `src/effects/ieffect/`
- Contract:
  ```cpp
  void render(RenderContext& ctx);  // Must complete < 2ms
  void onPaletteChange(uint8_t paletteId);  // Called on palette swap
  void onZoneChange(uint8_t zoneId);  // Zone-specific render
  ```
- Constraint: No `new`/`malloc`, no `String`, no logging in `render()`

**ControlBus (Audio State Snapshot):**
- Purpose: Immutable copy of audio analysis for effects to read
- Location: `firmware-v3/src/audio/contracts/ControlBus.h`
- Pattern: Struct with 30+ fields, snapshot via `getControlBusFrameSnapshot()`
- Fields used by effects: `bands[8]` (octaves), `chroma[12]` (pitch), `beat`, `onset`, `rms`, `tempo`, `phase`
- Lifetime: Valid for exactly one frame (~8.33ms), then replaced

**RenderContext (Per-Frame State):**
- Purpose: Transient context passed to every `effect.render()`
- Location: `firmware-v3/src/plugins/api/EffectContext.h`
- Contains: LED buffer, delta time, zone ID, control bus snapshot, effect parameters
- Used by: All effects, transitions, zone composition logic

**TransitionEngine (Visual Transitions):**
- Purpose: Animated wipe/fade/dissolve between effects
- Location: `firmware-v3/src/effects/transitions/TransitionEngine.h`
- Pattern: Two-state (IDLE, ACTIVE), 12 types (FADE, WIPE_IN/OUT, DISSOLVE, PHASE_SHIFT, PULSEWAVE, IMPLOSION, IRIS, NUCLEAR, STARGATE, KALEIDOSCOPE, MANDALA)
- Constraint: All transitions originate from centre (79/80) outward
- Duration: Configurable per type (800-3000ms default)

**ZoneComposer (Multi-Zone Blending):**
- Purpose: Combine 4 independent zone renders with blend modes
- Location: `firmware-v3/src/effects/zones/ZoneComposer.h`
- Pattern: Per-zone render → apply blend mode → composite to final output
- Blend modes: Mix (average), Add (brighten), Multiply (darken), Screen, Overlay
- Constraint: Centre-origin preserved per zone (each zone has sub-centre at 79/80 of its 160-LED segment)

**StateStore (CQRS Implementation):**
- Purpose: Single source of truth for all mutable state
- Location: `firmware-v3/src/core/state/StateStore.h/cpp`
- Pattern: Double-buffered immutable snapshots, write mutex, lock-free reads
- Command dispatch: `bool dispatch(const ICommand& cmd)`
- Subscription: Clients register callbacks for state changes
- Used by: All subsystems needing state read or mutation

**CueScheduler (Show Playback):**
- Purpose: Linear timeline-based effect sequencing
- Location: `firmware-v3/src/core/shows/CueScheduler.h`
- Pattern: PROGMEM cue list, tick-based dispatch, up to 4 cues per frame
- Cue types: EFFECT (change), PARAMETER_SWEEP (interpolate), ZONE_CONFIG, TRANSITION, NARRATIVE, PALETTE, MARKER
- Used by: ShowDirectorActor for show playback

## Entry Points

**Firmware (ESP32-S3):**
- Location: `firmware-v3/src/main.cpp` (3,693 LOC)
- Triggers: Arduino `setup()` / `loop()` entry point
- Responsibilities:
  1. Initialize ActorSystem (spawn AudioActor, RendererActor, ShowDirector, Command, PluginManager)
  2. Initialize network (WiFi AP, mDNS, WebServer, UdpStreamer)
  3. Restore persisted state (zones, presets, WiFi credentials)
  4. Load factory effects and shows
  5. Start boot animation
  6. Loop: dispatch serial commands, monitor heap, run health checks

**iOS App:**
- Location: `lightwave-ios-v2/LightwaveOS/App/LightwaveOSApp.swift` (@main)
- Triggers: App launch on iOS 17+
- Responsibilities:
  1. Create AppViewModel (ConnectionState, device list)
  2. Start DeviceDiscoveryService (Bonjour browser)
  3. Render TabView (Play | Zones | Audio | Device)
  4. Handle connection sheet, network errors

**Tab5 Encoder:**
- Location: `tab5-encoder/src/main.cpp`
- Triggers: Arduino setup/loop
- Responsibilities:
  1. Initialize encoder hardware (I2C)
  2. Initialize LVGL UI
  3. Initialize WiFi (connect to K1 AP)
  4. Loop: read encoder, debounce, send REST to K1, update UI

---

*Architecture analysis: 2026-03-21*

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:map-codebase | Created ARCHITECTURE.md with full pattern, layers, data flow, abstractions, entry points |
