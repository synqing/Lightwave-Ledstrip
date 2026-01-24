# LightwaveOS v2 Source Directory

The `src/` directory contains the complete firmware implementation for LightwaveOS v2, an ESP32-S3 LED control system for dual 160-LED WS2812 strips (320 total). It features an actor-based architecture, 90+ visual effects, audio-reactive capabilities, multi-zone composition, and web-based control.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Directory Structure](#directory-structure)
3. [Subsystem Reference](#subsystem-reference)
4. [Build Instructions](#build-instructions)
5. [Feature Flags](#feature-flags)
6. [Memory Budget](#memory-budget)
7. [Related Documentation](#related-documentation)

---

## Architecture Overview

```
                            ┌────────────────────────────────────────────┐
                            │              Web Interface                  │
                            │    REST API + WebSocket (port 80/ws)        │
                            └─────────────────────┬──────────────────────┘
                                                  │
                ┌─────────────────────────────────┼─────────────────────────────────┐
                │                                 │                                 │
                ▼                                 ▼                                 ▼
┌───────────────────────────┐   ┌───────────────────────────┐   ┌───────────────────────────┐
│      WiFiManager          │   │       WebServer           │   │   SyncManagerActor        │
│   (network/WiFiManager)   │   │   (network/WebServer)     │   │   (sync/SyncManagerActor) │
│   - WiFi state machine    │   │   - REST v1 API           │   │   - ESP-NOW mesh          │
│   - FreeRTOS EventGroup   │   │   - WebSocket gateway     │   │   - Peer discovery        │
│   - mDNS (lightwaveos)    │   │   - LED frame streaming   │   │   - Leader election       │
└───────────────────────────┘   └─────────────┬─────────────┘   └───────────────────────────┘
                                              │
                ┌─────────────────────────────┼─────────────────────────────┐
                │                             │                             │
                ▼                             ▼                             ▼
┌───────────────────────────┐   ┌───────────────────────────┐   ┌───────────────────────────┐
│      ActorSystem          │   │     ShowDirectorActor     │   │       AudioActor          │
│   (core/actors)           │   │   (core/actors)           │   │   (audio/AudioActor)      │
│   - Message bus           │   │   - Choreographed shows   │   │   - I2S capture (Core 0)  │
│   - Cross-core dispatch   │   │   - Cue scheduling        │   │   - Goertzel analysis     │
│   - Singleton registry    │   │   - Parameter sweeps      │   │   - MusicalGrid PLL       │
└───────────────────────────┘   └───────────────────────────┘   └───────────────────────────┘
                │                             │                             │
                └─────────────────────────────┼─────────────────────────────┘
                                              │
                                              ▼
                            ┌────────────────────────────────────────────┐
                            │            RendererActor (Core 1)          │
                            │                                            │
                            │  ┌──────────────────────────────────────┐  │
                            │  │           PatternRegistry            │  │
                            │  │   (90+ effects with metadata)        │  │
                            │  └──────────────────────────────────────┘  │
                            │                     │                      │
                            │  ┌──────────────────┼──────────────────┐  │
                            │  │                  ▼                  │  │
                            │  │  ┌──────────────────────────────┐  │  │
                            │  │  │       ZoneComposer           │  │  │
                            │  │  │   (1-4 independent zones)    │  │  │
                            │  │  └──────────────────────────────┘  │  │
                            │  │                  │                  │  │
                            │  │  ┌───────────────┴───────────────┐  │  │
                            │  │  ▼                               ▼  │  │
                            │  │ Zone 0          ...           Zone 3│  │
                            │  │ (IEffect)                   (IEffect)│  │
                            │  └──────────────────────────────────────┘  │
                            │                     │                      │
                            │  ┌──────────────────▼──────────────────┐  │
                            │  │        TransitionEngine             │  │
                            │  │   (12 types: fade, wipe, dissolve)  │  │
                            │  └──────────────────────────────────────┘  │
                            │                     │                      │
                            │  ┌──────────────────▼──────────────────┐  │
                            │  │           FastLedDriver             │  │
                            │  │   (WS2812 RMT, 320 LEDs @ 120 FPS)  │  │
                            │  └──────────────────────────────────────┘  │
                            └────────────────────────────────────────────┘
                                              │
                                              ▼
                            ┌────────────────────────────────────────────┐
                            │          Physical LED Strips               │
                            │   Strip 1 (GPIO 4, 160 LEDs, leds[0-159]) │
                            │   Strip 2 (GPIO 5, 160 LEDs, leds[160-319])│
                            │           CENTER ORIGIN: LED 79/80         │
                            └────────────────────────────────────────────┘
```

---

## Directory Structure

| Directory | Purpose | Key Files |
|-----------|---------|-----------|
| [`audio/`](#audio) | Real-time audio analysis and beat tracking | `AudioActor.h`, `GoertzelAnalyzer.h`, `contracts/` |
| [`config/`](#config) | Compile-time configuration | `features.h`, `audio_config.h`, `network_config.h` |
| [`core/`](#core) | Actor system, state management, persistence | `actors/`, `state/`, `persistence/`, `shows/` |
| [`effects/`](#effects) | 90+ visual effects with zones and transitions | `ieffect/`, `zones/`, `transitions/`, `enhancement/` |
| [`hal/`](#hal) | Hardware abstraction layer | `led/FastLedDriver.h`, `led/ILedDriver.h` |
| [`hardware/`](#hardware) | Physical I/O drivers | `EncoderManager.h`, `PerformanceMonitor.h` |
| [`network/`](#network) | WebServer, WiFi, API handlers | `WebServer.h`, `WiFiManager.h`, `webserver/` |
| [`palettes/`](#palettes) | Color palette definitions | `Palettes.h`, `CrameriPalettes.h` |
| [`plugins/`](#plugins) | Effect plugin architecture | `api/IEffect.h`, `runtime/LegacyEffectAdapter.h` |
| [`sync/`](#sync) | Multi-device synchronization | `SyncManagerActor.h`, `PeerManager.h` |
| [`utils/`](#utils) | Shared utilities | `Log.h`, `LockFreeQueue.h` |
| [`validation/`](#validation) | Effect validation framework | `EffectValidationMacros.h`, `ValidationFrameEncoder.h` |

---

## Subsystem Reference

### audio/

Real-time audio capture and frequency analysis for audio-reactive effects.

**Architecture**: Runs on Core 0 with I2S DMA capture, publishes to Core 1 via lock-free buffers.

| Component | Description | RAM |
|-----------|-------------|-----|
| `AudioActor` | FreeRTOS task orchestrating capture and analysis | 4 KB stack |
| `GoertzelAnalyzer` | 8-band frequency analysis (60 Hz - 8 kHz) | 1 KB |
| `ControlBus` | Attack/release envelope smoothing | 256 B |
| `MusicalGrid` | PLL-style beat/tempo tracker | 128 B |
| `TempoTracker` | BPM estimation with confidence scoring | 256 B |
| `StyleDetector` | Music style classification (rhythmic/harmonic/melodic) | 128 B |

**Contracts** (`contracts/`):
- `AudioTime.h` - Sample-index monotonic clock
- `SnapshotBuffer.h` - Lock-free double-buffer for cross-core communication
- `ControlBus.h` - DSP signals container with smoothing
- `MusicalGrid.h` - Beat phase and BPM tracking

See: [`audio/README.md`](audio/README.md) for detailed documentation.

---

### config/

Compile-time configuration files.

| File | Purpose |
|------|---------|
| `features.h` | Feature flags for conditional compilation |
| `audio_config.h` | Audio sample rate, hop size, band configuration |
| `network_config.h` | WiFi credentials, ports, mDNS hostname |
| `network_config.h.template` | Template for network configuration |

---

### core/

Central architecture components.

#### actors/

Actor model implementation for FreeRTOS multi-core communication.

| Actor | Core | Purpose |
|-------|------|---------|
| `ActorSystem` | - | Singleton registry and message dispatch |
| `RendererActor` | 1 | LED rendering at 120 FPS |
| `ShowDirectorActor` | 1 | Choreographed show playback |
| `AudioActor` | 0 | Real-time audio analysis |

#### state/

CQRS-style state management.

| Component | Description |
|-----------|-------------|
| `ICommand.h` | Command interface for state mutations |
| `SystemState` | Global state container |

See: [`core/state/README.md`](core/state/README.md) for usage patterns.

#### persistence/

NVS storage for configuration persistence.

| Component | Description |
|-----------|-------------|
| `NVSManager` | Low-level NVS read/write operations |
| `ZoneConfigManager` | Zone configuration storage |
| `AudioTuningManager` | Audio sensitivity presets |

#### shows/

ShowDirector system for choreographed multi-minute light shows.

| Component | Description |
|-----------|-------------|
| `ShowTypes.h` | Data structures (ShowCue, ShowChapter, ShowDefinition) |
| `CueScheduler.h` | Time-sorted PROGMEM queue traversal |
| `ParameterSweeper.h` | Concurrent parameter interpolations |
| `BuiltinShows.h` | 10 pre-defined shows in PROGMEM |

See: [docs/shows/README.md](../../../docs/shows/README.md) for ShowDirector documentation.

#### narrative/

Narrative tempo and breathing cycle modulation.

| Component | Description |
|-----------|-------------|
| `NarrativeEngine` | Breathing cycle control for ambient effects |

#### system/

System monitoring and profiling.

| Component | Description |
|-----------|-------------|
| `StackMonitor` | FreeRTOS stack high-water mark tracking |
| `HeapMonitor` | Heap fragmentation detection |
| `MemoryLeakDetector` | Allocation tracking for debugging |
| `ValidationProfiler` | Effect timing validation |

---

### effects/

Visual effects library with 90+ effects organized by category.

#### Effect Categories

| Category | Count | Examples |
|----------|-------|----------|
| Core Effects | 15 | Fire, Ocean, Plasma, Confetti, Juggle |
| LGP Interference | 12 | Holographic, DiamondLattice, BoxWave |
| LGP Chromatic | 8 | ChromaticAberration, RGBPrism, ColorTemperature |
| LGP Quantum | 7 | QuantumEntanglement, Tunneling, TimeCrystal |
| LGP Organic | 8 | Bioluminescent, MycelialNetwork, DNA Helix |
| LGP Physics | 10 | GravitationalLensing, DopplerShift, SolitonWaves |
| LGP Geometric | 6 | HexagonalGrid, ConcentricRings, Sierpinski |
| LGP Perlin | 12 | PerlinVeil, PerlinCaustics, ShockLines |
| Audio-Reactive | 8 | BeatPulse, BassBreath, ChordGlow, SpectrumBars |

**LGP** = Light Guide Plate effects optimized for acrylic waveguide interference.

#### Subdirectories

| Directory | Purpose |
|-----------|---------|
| `ieffect/` | Individual effect implementations (90 effects) |
| `zones/` | ZoneComposer for multi-zone rendering |
| `transitions/` | TransitionEngine (12 transition types) |
| `enhancement/` | ColorEngine and MotionEngine post-processing |
| `utils/` | FastLED optimizations and helpers |

#### IEffect Interface

All effects implement the `IEffect` interface from `plugins/api/IEffect.h`:

```cpp
class IEffect {
public:
    virtual void init(const EffectContext& ctx) = 0;
    virtual void render(const EffectContext& ctx, CRGB* leds, uint16_t numLeds) = 0;
    virtual const char* name() const = 0;
    virtual EffectCategory category() const = 0;
};
```

---

### hal/

Hardware Abstraction Layer for LED drivers.

| Component | Description |
|-----------|-------------|
| `ILedDriver.h` | Abstract LED driver interface |
| `FastLedDriver.h` | FastLED + RMT implementation |
| `LedDriverConfig.h` | Pin mappings and timing configuration |

**Pin Configuration**:
- Strip 1: GPIO 4 (160 LEDs)
- Strip 2: GPIO 5 (160 LEDs)
- Color order: GRB
- Data rate: 800 kHz (WS2812B)

---

### hardware/

Physical I/O drivers.

| Component | Description |
|-----------|-------------|
| `EncoderManager.h` | M5Stack ROTATE8 I2C encoder (8 rotary encoders) |
| `PerformanceMonitor.h` | FPS tracking, frame timing, CPU usage |

---

### network/

Web server and WiFi management.

#### Core Components

| Component | Description | Lines |
|-----------|-------------|-------|
| `WebServer.h/cpp` | REST API + WebSocket server | 2,600+ |
| `WiFiManager.h/cpp` | WiFi state machine with EventGroup | 1,400+ |
| `ApiResponse.h` | Standardized JSON response formatting |
| `RequestValidator.h` | Input validation and sanitization |
| `SubscriptionManager.h` | WebSocket event subscriptions |

#### webserver/

Modular API route handlers.

| Directory | Purpose |
|-----------|---------|
| `handlers/` | REST API endpoint handlers (Device, Effect, Zone, etc.) |
| `ws/` | WebSocket command handlers |
| `V1ApiRoutes.h` | API v1 route registration |
| `WsGateway.h` | WebSocket connection management |
| `WsCommandRouter.h` | WebSocket command dispatch |
| `LedStreamBroadcaster.h` | Real-time LED frame streaming |
| `RateLimiter.h` | Request rate limiting (20 req/sec HTTP, 50 msg/sec WS) |

**API Endpoints**: See [docs/api/API_V1.md](../../../docs/api/API_V1.md) for complete reference.

---

### palettes/

Color palette definitions.

| File | Description |
|------|-------------|
| `Palettes.h` | Main palette index and accessors |
| `Palettes_MasterData.cpp` | PROGMEM palette data |
| `ColorspacePalettes.h` | Perceptually uniform palettes (LAB, Oklab) |
| `CrameriPalettes.h` | Scientific color maps (Crameri collection) |

---

### plugins/

Effect plugin architecture for extensibility.

#### api/

| Component | Description |
|-----------|-------------|
| `IEffect.h` | Core effect interface |
| `EffectContext.h` | Runtime context (time, audio, parameters) |
| `BehaviorSelection.h` | Effect behavior flags |

#### runtime/

| Component | Description |
|-----------|-------------|
| `LegacyEffectAdapter.h` | Wraps v1 function-based effects as IEffect |

---

### sync/

Multi-device synchronization via ESP-NOW.

| Component | Description |
|-----------|-------------|
| `SyncManagerActor.h` | Main synchronization orchestrator |
| `PeerDiscovery.h` | UDP broadcast for device discovery |
| `PeerManager.h` | Connected peer tracking |
| `LeaderElection.h` | Bully algorithm for leader selection |
| `CommandSerializer.h` | Binary command encoding for ESP-NOW |
| `StateSerializer.h` | Full state snapshot encoding |
| `ConflictResolver.h` | Last-write-wins conflict resolution |
| `SyncProtocol.h` | Protocol version and message types |
| `DeviceUUID.h` | Unique device identification |

---

### utils/

Shared utility components.

| Component | Description |
|-----------|-------------|
| `Log.h` | Unified logging with colored output and levels |
| `LockFreeQueue.h` | Lock-free queue for cross-core messaging |

**Logging Levels**: 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug

```cpp
#define LW_LOG_TAG "MyModule"
#include "utils/Log.h"

LW_LOGI("Initialized");           // [I][MyModule] Initialized
LW_LOGW("Warning: %d", count);    // [W][MyModule] Warning: 42
LW_LOGE("Error occurred");        // [E][MyModule] Error occurred
```

---

### validation/

Effect validation and debugging framework.

| Component | Description |
|-----------|-------------|
| `EffectValidationMacros.h` | Macros for runtime effect validation |
| `EffectValidationMetrics.h` | Performance and correctness metrics |
| `ValidationFrameEncoder.h` | Binary encoding for validation streaming |

Enable with `FEATURE_EFFECT_VALIDATION=1`.

---

## Build Instructions

### Default Build (no WiFi)

```bash
cd firmware/v2
pio run -t upload
```

### WiFi-Enabled Build

```bash
pio run -e esp32dev_wifi -t upload
```

### Audio-Enabled Build

```bash
pio run -e esp32dev_audio -t upload
```

### Monitor Serial Output

```bash
pio device monitor -b 115200
```

### Clean Build

```bash
pio run -t clean && pio run -e esp32dev_wifi -t upload
```

### Run Native Tests

```bash
pio test -e native
```

---

## Feature Flags

Feature flags are defined in `config/features.h` and can be overridden via `platformio.ini`.

### Core Features

| Flag | Default | Description |
|------|---------|-------------|
| `FEATURE_ACTOR_SYSTEM` | 1 | Actor model architecture |
| `FEATURE_PLUGIN_RUNTIME` | 1 | Effect plugin system |
| `FEATURE_CQRS_STATE` | 1 | CQRS state management |
| `FEATURE_HAL_ABSTRACTION` | 1 | Hardware abstraction layer |

### Network Features

| Flag | Default | Description |
|------|---------|-------------|
| `FEATURE_WEB_SERVER` | 0 | REST API and WebSocket server |
| `FEATURE_MULTI_DEVICE` | 0 | Multi-device synchronization |

### Optional Features

| Flag | Default | Description |
|------|---------|-------------|
| `FEATURE_SERIAL_MENU` | 1 | Serial command interface |
| `FEATURE_PERFORMANCE_MONITOR` | 1 | FPS and memory tracking |
| `FEATURE_ZONE_SYSTEM` | 1 | Multi-zone composition |
| `FEATURE_TRANSITIONS` | 1 | Transition engine |
| `FEATURE_AUDIO_SYNC` | 0 | Audio-reactive effects |
| `FEATURE_OTA_UPDATE` | 1 | Over-the-air firmware updates |

### Audio Features

| Flag | Default | Description | CPU Cost |
|------|---------|-------------|----------|
| `FEATURE_AUDIO_SYNC` | 0 | Master audio enable | - |
| `FEATURE_MUSICAL_SALIENCY` | `AUDIO_SYNC` | Harmonic/rhythmic novelty | ~80 us/hop |
| `FEATURE_STYLE_DETECTION` | `AUDIO_SYNC` | Music style classification | ~60 us/hop |
| `FEATURE_AUDIO_OA` | 1 | Overlap-add sliding window | ~20 us/hop |
| `FEATURE_AUDIO_BENCHMARK` | 0 | Pipeline metrics collection | ~10 us/hop |

### Enhancement Features

| Flag | Default | Description |
|------|---------|-------------|
| `FEATURE_ENHANCEMENT_ENGINES` | 1 | Color and motion post-processing |
| `FEATURE_COLOR_ENGINE` | 1 | Cross-palette blending, diffusion |
| `FEATURE_MOTION_ENGINE` | 1 | Phase offset, auto-rotation |
| `FEATURE_PATTERN_REGISTRY` | 1 | Pattern taxonomy and metadata |

### Debug Features

| Flag | Default | Description |
|------|---------|-------------|
| `FEATURE_UNIFIED_LOGGING` | 1 | Colored log output |
| `FEATURE_MEMORY_DEBUG` | 0 | Allocation tracking |
| `FEATURE_EFFECT_PROFILER` | 0 | Per-effect timing |
| `FEATURE_EFFECT_VALIDATION` | 0 | Real-time validation streaming |
| `FEATURE_MABUTRACE` | 0 | Perfetto timeline tracing |

---

## Memory Budget

### RAM Usage

| Component | Size | Notes |
|-----------|------|-------|
| LED Buffers | 960 B | `CRGB leds[320]` |
| Transition Buffer | 960 B | Double-buffered transitions |
| ZoneComposer | 1.2 KB | Per-zone temp buffers |
| Audio Subsystem | 2 KB | Goertzel + ControlBus + MusicalGrid |
| WebServer | 8 KB | AsyncWebServer + WebSocket |
| WiFiManager | 2 KB | State machine + EventGroup |
| **Total Static** | **~15 KB** | |

### Flash Usage

| Component | Size | Notes |
|-----------|------|-------|
| Application | ~350 KB | Compiled firmware |
| SPIFFS | 1 MB | Web interface files |
| NVS | 20 KB | Configuration storage |
| OTA Partition | 1.5 MB | Firmware update staging |

### Available Resources (ESP32-S3)

| Resource | Total | Used | Available |
|----------|-------|------|-----------|
| SRAM | 384 KB | ~60 KB | ~324 KB |
| Flash | 8 MB | ~2 MB | ~6 MB |
| PSRAM | 8 MB | 0 | 8 MB (available) |

---

## Related Documentation

### Architecture

- [CLAUDE.md](../../../CLAUDE.md) - Project overview and constraints
- [AGENTS.md](../../../AGENTS.md) - Agent coordination patterns
- [docs/architecture/](../../../docs/architecture/) - System design documents

### API Reference

- [docs/api/API_V1.md](../../../docs/api/API_V1.md) - REST API v1 specification
- [docs/api/WEBSOCKET.md](../../../docs/api/WEBSOCKET.md) - WebSocket protocol

### Subsystem Documentation

- [audio/README.md](audio/README.md) - Audio subsystem details
- [core/state/README.md](core/state/README.md) - State management patterns
- [docs/shows/README.md](../../../docs/shows/README.md) - ShowDirector system

### Development Guides

- [docs/debugging/](../../../docs/debugging/) - Debugging techniques
- [docs/testing/](../../../docs/testing/) - Test harness documentation

---

*Document Version: 1.0*
*Last Updated: January 2026*
*LightwaveOS v2 Firmware*
