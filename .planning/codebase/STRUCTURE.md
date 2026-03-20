---
abstract: "Directory layout for 5-project monorepo: firmware-v3 (ESP32-S3 C++, 141K LOC), lightwave-ios-v2 (SwiftUI, 12K LOC), tab5-encoder (M5Stack C++, LVGL), k1-composer (web debug tool), lightwave-dashboard (web app). Key files, naming patterns, where to add new code."
---

# Codebase Structure

**Analysis Date:** 2026-03-21

## Directory Layout

```
Lightwave-Ledstrip/                    Monorepo root
├── firmware-v3/                       ESP32-S3 firmware (C++, FreeRTOS, Arduino/PlatformIO)
│   ├── src/                           Main source (141K LOC)
│   │   ├── effects/                   350+ LED visual effects (52.9K LOC)
│   │   ├── audio/                     I2S capture, DSP backends, beat tracking (15.5K LOC)
│   │   ├── network/                   REST API, WebSocket, UDP streaming (27.3K LOC)
│   │   ├── core/                      Actor system, state management, persistence (14.6K LOC)
│   │   ├── codec/                     JSON/binary serialization (9.0K LOC)
│   │   ├── hal/                       Hardware abstraction (4.3K LOC)
│   │   ├── sync/                      Multi-device synchronization (3.7K LOC)
│   │   ├── plugins/                   Plugin interface, registration (2.4K LOC)
│   │   ├── config/                    Build flags, hardware config (2.6K LOC)
│   │   ├── utils/                     Logging, math, helpers
│   │   ├── main.cpp                   Primary entry point (3.6K LOC)
│   │   └── test_strip_hw.cpp          Hardware test entry point
│   ├── docs/                          Technical documentation
│   │   ├── api/                       REST API reference (api-v1.md, 2.1K lines)
│   │   ├── audio-visual/              Audio reactivity principles
│   │   ├── reference/                 Pre-extracted maps (codebase-map.md, fsm-reference.md)
│   │   └── CQRS_STATE_ARCHITECTURE.md CQRS design and state management
│   ├── test/                          Unit tests (native builds)
│   ├── platformio.ini                 39 build environments (ESV11, PipelineCore, test, trace)
│   └── README.md
│
├── lightwave-ios-v2/                  iOS app (SwiftUI, 12K LOC, iOS 17+)
│   ├── LightwaveOS/                   Xcode project target
│   │   ├── App/                       Entry point (LightwaveOSApp.swift)
│   │   ├── Components/                Reusable UI (sliders, cards, controls)
│   │   ├── ViewModels/                @Observable state (App, Zone, Audio, Parameters, etc.)
│   │   ├── Views/                     SwiftUI screens organized by tab/section
│   │   │   ├── Tabs/                  PlayTab, ZonesTab, AudioTab, DeviceTab
│   │   │   ├── Play/                  Effect/palette selection, parameter controls
│   │   │   ├── Zones/                 Multi-zone composition, blending
│   │   │   ├── Audio/                 BPM display, audio metrics, sensitivity controls
│   │   │   └── Device/                Settings, presets, color correction
│   │   ├── Models/                    Codable data structures (PaletteMetadata, SystemState, etc.)
│   │   ├── Network/                   RESTClient, WebSocketService, UDPStreamReceiver, DeviceDiscovery
│   │   ├── Theme/                     DesignTokens (colors, typography, spacing)
│   │   ├── Utilities/                 PaletteLUT, CatmullRom interpolation, Rive helpers
│   │   └── Resources/                 Assets, fonts, localization
│   ├── LightwaveOSTests/              Unit tests
│   ├── LightwaveOSUITests/            UI tests
│   ├── docs/                          Design specs, API audit, Rive integration
│   ├── project.yml                    XcodeGen configuration
│   └── README.md
│
├── tab5-encoder/                      M5Stack Tab5 controller (C++, PlatformIO)
│   ├── src/                           Main source
│   │   ├── main.cpp                   Arduino entry point
│   │   ├── config/                    Build configuration, feature flags
│   │   ├── hal/                       Hardware abstraction (encoder, display, I2C)
│   │   ├── network/                   WiFi, REST client to K1 AP
│   │   ├── ui/                        LVGL menu tree, parameter sliders
│   │   ├── input/                     Encoder debounce and event handling
│   │   ├── parameters/                Parameter state, ranges
│   │   ├── presets/                   Preset save/load
│   │   ├── storage/                   NVS persistence
│   │   ├── zones/                     Zone configuration
│   │   ├── hardware/                  Pin definitions, peripheral init
│   │   ├── fonts/                     LVGL font data
│   │   └── utils/                     Logging, helpers
│   ├── docs/                          Design docs, API reference
│   ├── platformio.ini                 Build environments
│   └── README.md
│
├── k1-composer/                       Web debug/compositor tool (Node.js/WASM)
│   ├── src/                           Source (TypeScript/React)
│   │   └── code-catalog/              Effect metadata catalog (generated from firmware)
│   ├── proxy/                         CORS proxy for dev
│   ├── diagnostic/                    Debug utilities
│   ├── wasm/                          WebAssembly modules (if any)
│   └── scripts/                       Build and generation scripts
│
├── lightwave-dashboard/               Web dashboard (React/TypeScript)
│   └── node_modules/                  Dependencies
│
├── tools/                             Evaluation and capture tools (Python)
│   ├── led_capture/                   Frame capture + FPS sweep (serial + Python)
│   │   ├── led_capture.py             Main capture tool
│   │   └── .venv/                     Python venv (numpy, Pillow, pyserial, websockets, imageio)
│   ├── edgemixer_eval/                EdgeMixer colour harmony evaluation
│   └── eval_results/                  Results from effect evals
│
├── docs/                              Project-level documentation
│   ├── WORKFLOW_ROUTING.md            Session routing table (tool + skill dispatch)
│   ├── design/                        Design decisions, ADRs
│   ├── cron/                          Scheduled monitoring tasks
│   └── (other reference docs)
│
├── instructions/                      Governance and naming policy
│   ├── NAMING_POLICY.md               Effect/preset naming conventions
│   ├── REPO_GOVERNANCE.md             Commit discipline, branch strategy
│   └── changelog/                     Historical release notes
│
├── research/                          Research and exploration output
├── _archive/                          Superseded code and historical artifacts
├── .claude/                           Agent working directory (plans, handoffs, temp files)
├── .github/                           CI/CD workflows
├── .planning/                         GSD output (codebase docs, phase plans, verification)
│   └── codebase/                      This directory — architecture reference files
├── .code-review-graph/                AST graph database (auto-gitignored)
├── scripts/                           Setup and validation scripts
└── README.md                          Project overview
```

## Directory Purposes

**firmware-v3/src/effects/ (52.9K LOC, 350+ effects):**
- Purpose: Visual effect implementations
- Contains: `IEffect` subclasses in `ieffect/`, enhancement processors (EdgeMixer, SmoothingEngine, ColorCorrection), transition engine, zone composition
- Key files:
  - `ieffect/` — 350+ effect .h/.cpp pairs (alphabetical: `AsteriumEffect.cpp`, `LGPHolographicEffect.cpp`, etc.)
  - `enhancement/EdgeMixer.h` — colour harmony modes (Monochromatic, Analogous, Triadic, Tetradic, Complementary)
  - `enhancement/SmoothingEngine.h` — temporal parameter smoothing (decay, hysteresis)
  - `zones/ZoneComposer.h` — multi-zone blending with blend modes
  - `transitions/TransitionEngine.h` — 12 animated transition types
  - `CoreEffects.h` — registry and factory for built-in effects

**firmware-v3/src/audio/ (15.5K LOC):**
- Purpose: Microphone capture, DSP, beat tracking, audio-reactive state
- Contains: I2S driver, FFT/Goertzel backends, beat tracker, tempo tracker, audio tuning
- Key files:
  - `AudioActor.h/cpp` — Core 0 task, I2S DMA, DSP pipeline orchestration
  - `backends/esv11/` — Production ESV11 backend (64-bin Goertzel)
  - `backends/pipelinecore/` — Alternative FFT backend (broken for beat tracking)
  - `contracts/ControlBus.h` — Snapshot struct (bands, chroma, beat, tempo, RMS)
  - `pipeline/BeatTracker.h` — Onset detection, beat estimation
  - `pipeline/TempoTracker.h` — Tempo (BPM) estimation

**firmware-v3/src/network/ (27.3K LOC):**
- Purpose: REST API, WebSocket real-time control, UDP streaming
- Contains: Web server setup, route handlers, WebSocket gateway, UDP streamer
- Key files:
  - `WebServer.h/cpp` — Async HTTP server, route dispatcher
  - `webserver/v1_routes/` — REST endpoint implementations (effect, palette, parameter, zone, transition, audio, preset, show)
  - `WebSocket.h/WsGateway.h` — WebSocket connection handling, message routing
  - `UdpStreamer.h` — 960B LED frame + metrics telemetry over UDP
  - `WiFiManager.h` — AP-only mode (K1 hard constraint), credential management

**firmware-v3/src/core/ (14.6K LOC):**
- Purpose: Actor system, state management, persistence, show scheduling
- Contains: Actor base classes, message bus, CQRS state store, show system, NVS persistence
- Key files:
  - `actors/ActorSystem.h/cpp` — System lifecycle, actor spawning
  - `actors/RendererActor.h/cpp` — Core 1 rendering task, effect/zone orchestration
  - `actors/ShowDirectorActor.h` — Show playback, cue scheduling
  - `state/StateStore.h/cpp` — CQRS double-buffered state, command dispatch
  - `state/Commands.h` — 20+ command implementations (SetEffect, SetBrightness, ZoneEnable, etc.)
  - `state/SystemState.h` — Immutable state struct (~100 bytes)
  - `bus/MessageBus.h` — Lock-free publish, 16-byte messages
  - `shows/CueScheduler.h` — Timeline-based effect sequencing
  - `persistence/NVSManager.h` — Key-value storage, preset/zone save/load

**firmware-v3/src/codec/ (9.0K LOC):**
- Purpose: JSON/binary serialization for effects, shows, presets
- Contains: Codec interfaces, specific implementations for effect data
- Used by: Persistence, network handlers

**firmware-v3/src/hal/ (4.3K LOC):**
- Purpose: Hardware abstraction for LED output, display, I2S audio
- Contains: FastLED driver, RMT configuration, display (RM690B0 QSPI), I2S interface
- Key files:
  - `led/FastLedDriver.h` — WS2812 LED control via FastLED
  - `esp32s3/LedDriver_S3_RMT.h` — RMT channel configuration (MAX_CHANNELS=4 for parallel dual-strip)
  - `display/DisplayActor.h` — Optional AMOLED display controller

**firmware-v3/src/config/ (2.6K LOC):**
- Purpose: Build-time configuration, hardware definitions, feature flags
- Key files:
  - `hardware_config.h` — LED counts, zone sizes, centre point (79/80), pin definitions
  - `features.h` — Feature gates (FEATURE_STACK_PROFILING, FEATURE_TRANSITIONS, FEATURE_ZONES, FEATURE_AUDIO_SYNC)
  - `effect_ids.h` — Auto-generated effect ID constants (from `gen_effect_ids.py`)

**lightwave-ios-v2/LightwaveOS/ (12K LOC, SwiftUI):**
- Purpose: iOS companion app for parameter control and LED preview
- Architecture: MVVM with `@Observable` ViewModels on `@MainActor`
- Key directories:
  - `App/` — Main entry point (@main attribute)
  - `ViewModels/` — AppViewModel (connection, device), ZoneViewModel, AudioViewModel, ParametersViewModel
  - `Views/` — SwiftUI screens: PlayTab (hero LED + controls), ZonesTab (multi-zone), AudioTab (BPM + metrics), DeviceTab (settings)
  - `Components/` — Reusable controls (LWSlider, BeatPulse, LWButton, ConnectionDot)
  - `Network/` — RESTClient (REST calls, debounce), WebSocketService (subscriptions), UDPStreamReceiver (LED frames), DeviceDiscoveryService (Bonjour)
  - `Models/` — Codable data: PaletteMetadata, SystemState, AudioMetricsFrame, ZoneConfig, TransitionConfig
  - `Theme/` — DesignTokens (colors, typography, spacing, animations)

**tab5-encoder/src/ (M5Stack Tab5, C++):**
- Purpose: Physical rotary encoder interface for K1 control
- Key directories:
  - `main.cpp` — Arduino setup/loop entry
  - `hal/` — Encoder hardware interface (I2C), display driver
  - `network/` — WiFi connection to K1 AP, REST client for parameter POST
  - `ui/` — LVGL menu hierarchy, parameter sliders
  - `input/` — Encoder debounce and event handling
  - `parameters/` — Local parameter state with ranges
  - `storage/` — NVS persistence for encoder settings
  - `zones/` — Zone configuration UI

**k1-composer/ (Web debug tool):**
- Purpose: Browser-based compositor for testing effects, visualizing audio
- Contains: TypeScript/React source, effect catalog JSON, diagnostic utilities
- Key files:
  - `src/code-catalog/effects.json` — Effect metadata (auto-generated from firmware)
  - `proxy/` — CORS proxy for localhost testing
  - `diagnostic/` — Debug helpers

## Key File Locations

**Entry Points:**
- `firmware-v3/src/main.cpp` — ESP32-S3 firmware boot and loop
- `lightwave-ios-v2/LightwaveOS/App/LightwaveOSApp.swift` — iOS app (@main)
- `tab5-encoder/src/main.cpp` — M5Stack Tab5 firmware boot and loop

**Configuration:**
- `firmware-v3/platformio.ini` — Build environments (39 total: production, test, trace, benchmark)
- `firmware-v3/src/config/hardware_config.h` — LED counts, zones, centre point, GPIO pins
- `firmware-v3/src/config/features.h` — Feature gates (compile-time flags)
- `lightwave-ios-v2/project.yml` — XcodeGen project definition

**Core Logic:**
- `firmware-v3/src/core/actors/RendererActor.h/cpp` — 120 FPS render loop
- `firmware-v3/src/core/actors/ActorSystem.h/cpp` — Task coordination
- `firmware-v3/src/core/state/StateStore.h/cpp` — CQRS state management
- `firmware-v3/src/audio/AudioActor.h/cpp` — I2S capture and DSP
- `firmware-v3/src/audio/contracts/ControlBus.h` — Audio state snapshot
- `firmware-v3/src/network/WebServer.h` — REST API dispatcher
- `firmware-v3/src/network/WsGateway.h` — WebSocket handling
- `lightwave-ios-v2/LightwaveOS/ViewModels/AppViewModel.swift` — iOS connection state machine
- `lightwave-ios-v2/LightwaveOS/Network/RESTClient.swift` — REST calls with debounce

**Testing:**
- `firmware-v3/test/` — Unit tests, run with `pio run -e native_test`
- `lightwave-ios-v2/LightwaveOSTests/` — Swift unit tests
- `lightwave-ios-v2/LightwaveOSUITests/` — SwiftUI component tests

## Naming Conventions

**Files:**
- C++ source: `PascalCaseFeature.cpp` (e.g., `RendererActor.cpp`, `EdgeMixer.cpp`)
- Headers: `PascalCase.h` (e.g., `IEffect.h`, `ControlBus.h`)
- Python: `snake_case.py` (e.g., `led_capture.py`, `gen_effect_ids.py`)
- Swift: `PascalCase.swift` (e.g., `AppViewModel.swift`, `RESTClient.swift`)
- Test files: `[ClassName]Test.cpp` or `[ClassName]Tests.swift`

**Directories:**
- `snake_case` for directories (e.g., `led_capture/`, `audio-visual/`)
- Exception: `PascalCase` for package-like dirs in Swift (e.g., `ViewModels/`, `Components/`)

**C++ Identifiers:**
- Classes: `PascalCase` (e.g., `RendererActor`, `ControlBus`)
- Functions: `camelCase` (e.g., `render()`, `dispatch()`, `getState()`)
- Member variables: `m_camelCase` (e.g., `m_buffer`, `m_activeIndex`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_EFFECTS`, `DEFAULT_BRIGHTNESS`)
- Macros: `LW_UPPER_SNAKE_CASE` (e.g., `LW_LOG_TAG`, `FEATURE_STACK_PROFILING`)

**Swift Identifiers:**
- Classes: `PascalCase` (e.g., `AppViewModel`, `RESTClient`)
- Functions: `camelCase` (e.g., `connect()`, `updateStatus()`)
- Variables: `camelCase` (e.g., `isConnected`, `ledBuffer`)
- Constants: `UPPER_SNAKE_CASE` or `camelCase` (both acceptable)

**Effect Naming (firmware):**
- Pattern: `[Category][Descriptor]Effect` (e.g., `BeatPulseEffect`, `LGPHolographicEffect`, `EsAnalogRefEffect`)
- Files: `[Category][Descriptor]Effect.h/cpp`
- Categories: `Lg` (Light Guide), `Es` (Experimental), `Sb` (Sensory Bridge)

**REST API Endpoints:**
- Pattern: `/api/v1/{resource}/{action}` (e.g., `/api/v1/effects`, `/api/v1/effects/{id}`, `/api/v1/zones/list`)
- Methods: POST for mutations, GET for queries
- WebSocket: Message format `{"type": "message.type", "data": {...}}`

## Where to Add New Code

**New Effect (LED Visual):**
- Primary code: `firmware-v3/src/effects/ieffect/MyNewEffect.h/cpp`
  - Inherit from `IEffect`, implement `render(RenderContext&)`
  - Name: `[Category][Descriptor]Effect` (e.g., `MyCustomBurstEffect`)
- Register: Add to `firmware-v3/src/effects/CoreEffects.cpp` (factory)
- Tests: `firmware-v3/test/effects/MyNewEffectTest.cpp` (optional)
- Constraints:
  - Render must complete < 2ms
  - No heap allocation, no `String`, no logging in `render()`
  - Must handle `ctx.zoneId < kMaxZones` else fallback to global (0xFF)
  - Centre-origin required: LED 79/80 outward

**New Audio Feature (DSP analysis, beat detection):**
- Primary code: `firmware-v3/src/audio/[Feature].h/cpp`
- Update: `firmware-v3/src/audio/AudioActor.h/cpp` to invoke new feature
- Output to: `firmware-v3/src/audio/contracts/ControlBus.h` (add field if needed)
- Tests: `firmware-v3/test/audio/[Feature]Test.cpp`

**New REST Endpoint:**
- Primary code: `firmware-v3/src/network/webserver/v1_routes/[Resource]Routes.cpp`
- Register: `firmware-v3/src/network/WebServer.cpp` (route dispatcher)
- Documentation: Add to `firmware-v3/docs/api/api-v1.md`

**New iOS Feature (UI, control):**
- Primary code: `lightwave-ios-v2/LightwaveOS/Views/[Feature].swift` or `lightwave-ios-v2/LightwaveOS/ViewModels/[Feature]ViewModel.swift`
- Models: Add to `lightwave-ios-v2/LightwaveOS/Models/` if new data type
- Network: Update `lightwave-ios-v2/LightwaveOS/Network/RESTClient.swift` or `WebSocketService.swift` if new API call
- Tests: `lightwave-ios-v2/LightwaveOSTests/[Feature]Tests.swift`
- Constraints:
  - All ViewModels: `@MainActor @Observable class`
  - Network services: `actor` (thread-safe)
  - Parameter sliders: 150ms debounce minimum before REST/WS
  - British English in strings and comments

**New Shared Utility (used across modules):**
- Firmware: `firmware-v3/src/utils/[Feature].h/cpp` (e.g., `firmware-v3/src/utils/Log.h`)
- iOS: `lightwave-ios-v2/LightwaveOS/Utilities/[Feature].swift` (e.g., `Utilities/PaletteLUT.swift`)

**New Build Environment (PlatformIO):**
- Primary code: `firmware-v3/platformio.ini`
- Pattern: `[env:name]` section with `platform`, `board`, `framework`, `build_flags`, feature gates
- Copy from similar: `[env:esp32dev_audio_esv11]` for production builds

## Special Directories

**`firmware-v3/src/effects/ieffect/` (350+ effect implementations):**
- Purpose: Individual effect source files (one per effect)
- Pattern: `EffectNameEffect.h` + `EffectNameEffect.cpp`
- Generated: No (hand-authored)
- Committed: Yes

**`firmware-v3/.pio/build/`:**
- Purpose: PlatformIO build artifacts (object files, binaries)
- Generated: Yes (by `pio run`)
- Committed: No (in `.gitignore`)
- Notes: Shared across worktrees via `.worktreeinclude`

**`lightwave-ios-v2/LightwaveOS.xcodeproj/`:**
- Purpose: Xcode project metadata
- Generated: Yes (by `xcodegen generate` from `project.yml`)
- Committed: No (regenerate from YAML)

**`.planning/codebase/`:**
- Purpose: GSD output (this directory) — architecture reference documents
- Generated: Yes (by `/gsd:map-codebase` agent)
- Committed: Yes

**`.code-review-graph/`:**
- Purpose: Tree-sitter AST graph database (SQLite)
- Generated: Yes (by code-review-graph MCP on first use)
- Committed: No (auto-gitignored)

---

*Structure analysis: 2026-03-21*

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:map-codebase | Created STRUCTURE.md with directory layout, key files, naming conventions, where to add code |
