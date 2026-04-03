# Changelog

All notable changes to the LightwaveOS project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - ESP32-P4 Audio Pipeline & iOS App

### Added
- **firmware:** STM (Spectral-Temporal Modulation) dual-edge mode — per-LED spectral modulation for EdgeMixer
- **firmware:** STM 128-band spectral upgrade — mel filterbank + FFT pipeline
- **firmware:** STM snapshot REST endpoint with derived metrics
- **firmware:** STM real-time WebSocket binary stream + heap threshold fix
- **firmware:** STM_SPECTRAL_MAP EdgeMixer mode — per-LED spectral modulation bin mapping
- **tab5:** Zone Mode enable/disable button on ZONES sidebar panel with layout-before-enable sequence
- **tab5:** Zone count selector (1/2/3 zones) on grid slot 5, encoder-only (ENC-B 5)
- **tab5:** LED count display per zone on grid slot 6 (ENC-B 6)
- **tab5:** 3-zone centre-origin layout support (was limited to 2)
- **tab5:** Preset save/restore for EdgeMixer state (mode/spread/strength/spatial/temporal)
- **tab5:** Preset save/restore for colour correction mode (OFF/HSV/RGB/BOTH)
- **tab5:** Preset save/restore for auto-exposure target value
- **tab5:** Preset save/restore for per-zone blend modes
- **tab5:** Zone layout sent before zone.enable on preset recall (prevents K1 no-op)
- **tab5:** Current global effect auto-assigned to all zones on zone mode enable
- **tab5:** I2C recovery rewrite, PSRAM migration, UI and network hardening
- **tab5:** EdgeMixer mode cycling expanded to 9 modes (added STM DUAL, STM SPECTRAL)
- **zone-mixer:** AtomS3+PaHub physical controller — Phases 1-6 (input layer, display, I2C recovery, parameter mapping, LED feedback, echo suppression)
- **harness:** STM feasibility test harness for spectral-temporal modulation
- **scripts:** STM system test suite — zero-dependency offline tooling
- **docs:** STM 128-band spectral upgrade specification
- **docs:** STM endpoint, stream commands, and mode range updates
- **docs:** Tab5 I2C recovery research, memory audit, and implementation guides

### Fixed
- **tab5:** Zone effectId truncated from uint16_t to uint8_t — ZoneState.effectId and WsMessageRouter parsing both used uint8_t, losing high byte of K1's hex effect IDs (0x0100+)
- **tab5:** Zone effect encoder sent raw 0,1,2,3 — clamped to valid K1 range 0x0100-0x1F00 with cached name display
- **tab5:** Zone palette encoder had no upper bound — now wraps 0-74 (75 palettes)
- **tab5:** Zone blend encoder had no upper bound — now wraps 0-7 (8 modes)
- **tab5:** Zone encoder zoneId not clamped to zone count — caused "Invalid zoneId" flood when zone count reduced
- **tab5:** Zone param cards 5-6 accepted touch events and bubbled to mode row — LV_OBJ_FLAG_CLICKABLE and LV_OBJ_FLAG_EVENT_BUBBLE cleared
- **tab5:** Preset zone brightness hardcoded to 255 on save — now reads actual ZoneState.brightness
- **tab5:** Preset CC mode used live server state on recall instead of saved value
- **tab5:** Preset AE target hardcoded to 110 on recall instead of saved value
- **tab5:** Preset zone blend mode captured but never restored (missing sendZoneBlend call)
- **tab5:** EdgeMixer mode display showed "???" for modes 7-8 — added STM DUAL and STM SPECTRAL names
- **firmware:** Cross-core race conditions in ZoneComposer and WebSocket broadcast
- **firmware:** Lower heap-shed thresholds and fix probe condition
- **zone-mixer:** Phase 4 bug fixes — per-zone effects, zone layout, layout-before-enable
- **zone-mixer:** Safety audit fixes — WDT, I2C error handling, Arduino compatibility

### Changed
- **tab5:** Zone purge 4→3 — 1-indexed (Zone 1/2/3), max 3 zones, no Zone 0
- **tab5:** I2CRecovery stripped to error counter only (-835 lines)
- **tab5:** Purged 22 unused font assets — 79K LOC removed
- **tab5:** Encoder bring-up hardened, external I2C init restored
- **tab5:** Dashboard initialised before host connection
- **tab5:** Pinned validated display and encoder dependencies
- **firmware:** Zone purge — 1-indexed (Zone 1/2/3), max 3, no Zone 0

### Fixed (previous)
- **firmware:** REST EdgeMixer mode validation rejected Triadic (5) and Tetradic (6) — `V1ApiRoutes.cpp` validated `mode > 4` instead of `mode > 6`
- **firmware:** WS speed validation capped at 50 instead of 100 — `WsEffectsCodec.cpp` `decodeSetSpeed` and `parameters.set` used stale range (1-50) while REST and RendererActor use extended range (1-100)
- **firmware:** `ZoneConfigManager.h` `MAX_SPEED` was 50, misaligned with `RendererActor.h` and `ZonePresetManager.h` (both 100)
- **firmware:** SerialCLI `zs` command validated speed 1-50 instead of 1-100
- **docs:** `api-v1.md` EdgeMixer section documented 5 modes (0-4); now documents all 7 modes (0-6) including Triadic and Tetradic
- **docs:** `api-v1.md` effects pagination documented `start`/`count` params; actual implementation uses `page`/`limit`/`offset`
- **docs:** `api-v1.md` transition `toEffect` documented as uint8 (0-46); corrected to uint16
- **docs:** `api-v1.md` parameters section missing `hue`, `mood`, `fadeAmount` fields; added with correct ranges
- **docs:** `api-v1.md` device status example showed STA mode (`apMode: false`, `192.168.1.100`); corrected to AP-only reality
- **docs:** `api-v1.md` presets section falsely claimed handlers are stubs returning NOT_IMPLEMENTED; disclaimer removed
- **docs:** `api-v1.md` speed range documented as 1-50; corrected to 1-100
- **docs:** `k1-ws-contract.yaml` speed ranges updated from 1-50 to 1-100 for `setSpeed` and `parameters.set`
- **docs:** `k1-rest-contract.yaml` EdgeMixer section expanded with parameter ranges and all 7 mode descriptions

### Added
- **firmware:** Shared gradient rendering kernel — GradientRamp, GradientCoord, GradientTypes under `effects/gradient/`. Centre-origin, dual-edge, 8-stop ramps with clamp/repeat/mirror modes, linear/eased/hard-stop interpolation, stack-allocated (~35 bytes)
- **firmware:** Retrofitted LGPPerceptualBlendEffect with 3-stop eased gradient ramp — proves multi-stop palette composition
- **firmware:** LGPGradientField effect (0x1F00) — operator-surfaced gradient proof with 6 parameters: basis, repeatMode, interpolation, spread, phaseOffset, edgeAsymmetry. Dirty-flag ramp rebuild.
- **firmware:** Colour correction skip for LGP_GRADIENT_FIELD (gradient ramps require uncorrected output)
- **firmware:** K1 coordinate helpers — uCenter(), uSigned(), uLocal(), edgeId(), halfIndex(), writeCentrePairDual() with correct 79.5 midpoint
- **firmware:** Colour correction skip for gradient-sensitive effects (EID_LGP_PERCEPTUAL_BLEND)
- **docs:** Gradient system design doc — coordinate model, API reference, validation scenes, forbidden patterns
- **ios:** EdgeMixer card on Play tab — 7 colour harmony modes, spread/strength sliders, spatial mask and temporal modulation pickers
- **ios:** EdgeMixer WebSocket integration — `edge_mixer.get/set/save` commands with 150ms debounce and echo prevention
- **ios:** EdgeMixer status broadcast sync — passive multi-client state updates from periodic device status
- **firmware:** FFT onset detector — 1024-point spectral flux (Bello/Dixon/Boeck), activity-gated, ~400us/hop, zero heap [quarantined]
- **firmware:** Band-energy ratio detector — variance-adaptive percussion triggers on grouped bands (Patin/WLED) [WIP]
- **firmware:** First-class onset API surface — OnsetContext with 6 semantic channels (beat, downbeat, transient, kick, snare, hihat) [WIP]
- **firmware:** OnsetSemantics module — extracted onset channel tracking from RendererActor into testable free functions
- **firmware:** AudioReactivePolicy TriggerMode — 7 modes with metronome fallback for effect trigger routing
- **firmware:** Onset capture telemetry — CaptureStreamer v2 bytes [26:32] carry onsetEnv, onsetEvent, percussion triggers
- **firmware:** EsV11Backend sample history accessor — getSampleHistory()/getSampleHistoryLength() for raw PCM access
- **tools:** Onset capture tooling — led_capture.py onset parsing, analyze_beats.py onset statistics, 2 capture suite scripts
- **docs:** Onset detector spec, quarantine matrix, ADR, WLED/aubio/essentia research references
- **tools:** Integrated RTK v0.34.2 token compression proxy — compresses Bash command output (git, builds, file listings) before reaching LLM context, reducing token consumption by 50-80% on CLI operations

### Reverted
- **firmware:** LGPRadialRippleEffect restored to original — gradient retrofit replaced sin16() brightness with triangular ramp and collapsed hue range from 64 to 4 steps
- **firmware:** LGPChromaticShearEffect restored to original — gradient retrofit reduced centre dimming from 50% to 30% with different curve shape

### Changed
- **firmware:** Silence gate uses raw PCM RMS instead of post-AGC band average — fixes false gate reopening during silence
- **firmware:** Silence hysteresis reduced from 8000ms to 150ms — responsive musical drops with ~550ms silence-to-black
- **firmware:** AGC noise floor lowered from 0.01 to 0.001 — only disables during electrical silence, not quiet passages
- **firmware:** Waveform effects (SbK1Waveform, SbK1WaveformHybrid) — RMS brightness scaling, silentScale gating, accelerated trail decay
- **firmware:** LittleFS.begin(true) — auto-format on blank/corrupted partition instead of silent failure
- **firmware:** BeatPulseBloom, BeatPulseBreathe, RippleEsTuned migrated to onset API trigger routing [WIP]

### Added
- **firmware:** EdgeMixer two new colour harmony modes — Triadic (120° hue shift) and Tetradic (90° hue shift); spread controls saturation blend (0=full sat, 60=70% sat)
- **tab5:** EdgeMixer mode button cycles through all 7 modes including Triadic and Tetradic
- **ESP32-P4 Platform Support**: Full audio capture and LED control on Waveshare ESP32-P4-WIFI6
  - ES8311 audio codec integration via I2S standard driver
  - Dual WS2812 LED strips (320 LEDs total) via RMT peripheral
  - ESP-IDF v5.5.2 toolchain with RISC-V GCC
  - Build scripts: `build_with_idf55.sh`, `flash_and_monitor.sh`

### Changed
- **Audio Pipeline Cadence Alignment** (ESP32-P4): Fixed timing mismatch between hop rate and scheduler
  - Changed HOP_SIZE from 128 to 160 samples (8ms → 10ms) to match FreeRTOS 100Hz tick
  - Hop rate now perfectly aligned: 160 samples @ 16kHz = 10ms = 100 Hz = 1 tick
  - Eliminates DMA timeouts, watchdog triggers, and erratic capture rates
  - See `docs/AUDIO_PIPELINE_CADENCE_FIX.md` for full technical analysis

- **DC Blocker Coefficient**: Corrected from 0.9922 to 0.992176 using proper formula
  - Formula: R = exp(-2π × fc / fs) where fc=20Hz, fs=16000Hz
  - Ensures accurate DC removal without affecting audio content

- **ES8311 Microphone Gain**: Added explicit 24dB gain setting
  - Signal levels were ~0.2% of full scale (-54dB), now properly amplified
  - Available gains: 0dB to 42dB in 6dB steps

- **Spike Detection Improvements**:
  - Added noise floor check (0.005 threshold) to skip detection on quiet signals
  - Raised warning threshold from 5.0 to 10.0 spikes/frame (20 bins checked per frame)
  - Prevents false warnings from noise-floor fluctuations

- **I2C Probe for ES8311**: Changed to low-level ACK check using `i2c_cmd_link` API
  - Fixes intermittent probe failures with `i2c_master_write_read_device()` 0-length read

### Fixed
- **AudioActor FreeRTOS Tick Conversion**: Added `LW_MS_TO_TICKS_CEIL_MIN1()` macro
  - `pdMS_TO_TICKS(8)` returns 0 at 100Hz ticks (8ms < 10ms tick period)
  - New macro uses ceiling division with minimum of 1 tick

- **Self-clocked Mode Stability** (retained but unused): Actor.cpp now supports tickInterval=0
  - Caused watchdog triggers due to IDLE0 starvation - not recommended for audio
  - Kept for potential future use cases with proper yield points

---

## [Previous Unreleased] - Light Guide Plate Feature

### Added
- **Light Guide Plate Mode**: Revolutionary optical waveguide display system
  - Dual-edge LED injection into 329mm acrylic plate
  - Advanced interference pattern effects using wave physics
  - Depth illusion system with volumetric display capabilities
  - Physics simulations: plasma fields, magnetic field lines, particle collisions
  - Interactive features: proximity sensing, gesture recognition, touch-reactive surfaces
  - Advanced optical effects: holographic patterns, energy transfer visualization

### Technical Documentation Added
- `docs/LIGHT_GUIDE_PLATE.md`: Comprehensive 240+ line technical specification
  - Physical configuration and hardware specifications
  - Optical theory and wave interference mathematics
  - 5 major effect categories with detailed implementations
  - Performance optimization strategies and memory management
  - 6-phase implementation roadmap with weekly milestones
  - Testing and validation frameworks
  - Future enhancement possibilities

### Architecture Enhancements
- Light guide effect base classes and coordinate mapping systems
- Interference calculation framework with real-time optimization
- Edge-to-center coordinate transformation algorithms
- Specialized synchronization modes for optical effects
- Performance-optimized interference pattern storage

### Configuration Extensions
- New feature flags for light guide mode detection
- Hardware configuration for dual-edge LED control
- M5Stack encoder integration for light guide parameters
- Compile-time optimization for optical calculations

### Effect Categories Planned
1. **Interference Pattern Effects**
   - Standing wave patterns with mathematical precision
   - Moiré interference from overlapping frequencies
   - Constructive/destructive zone visualization

2. **Depth Illusion Effects**
   - Volumetric display simulation with apparent 3D objects
   - Parallax-like effects using edge intensity control
   - Z-depth mapping with atmospheric perspective

3. **Physics Simulation Effects**
   - Plasma field visualization with realistic physics
   - Magnetic field line rendering using dipole equations
   - Particle collision chamber with momentum conservation
   - Wave tank simulation with proper propagation physics

4. **Interactive Applications**
   - Proximity detection using light occlusion analysis
   - Gesture recognition from shadow pattern changes
   - Touch-reactive surfaces with ripple effects
   - Real-time data visualization framework

5. **Advanced Optical Effects**
   - Edge coupling resonance with feedback loops
   - Energy transfer visualization with conservation laws
   - Holographic interference pattern generation
   - Interactive light-based games (optical pong, light tennis)

### Performance Specifications
- Maintained 120 FPS target with complex interference calculations
- Optimized memory usage with efficient pattern storage
- Real-time interference calculation using ESP32-S3 dual cores
- Temporal coherence optimization for smooth animations

### Future Integration Points
- Machine learning for advanced gesture recognition
- Multi-unit synchronization for large installations
- Camera integration for computer vision applications
- Advanced physics: quantum mechanics and relativistic effects

### Changed
- Web control plane refactor (robustness-first):
  - Replaced ESPAsyncWebServer/AsyncTCP web stack with ESP-IDF `esp_http_server` backend (REST + WebSocket) in WiFi environments.
  - Default build (`esp32dev`) no longer compiles any web stack dependencies.
  - JSON handling remains cJSON-only.
  - Feature flags: `FEATURE_WEB_SERVER`, `FEATURE_WEBSOCKET`, `FEATURE_OTA_UPDATE` now default to OFF unless enabled via PlatformIO env build flags.

### Fixed
- Eliminated cross-platform dependency leakage in ESP32 builds (no ESP8266/RP2040 async TCP libraries pulled into ESP32-S3 builds).
- **Audio Gate Responsiveness**: Fixed activity gate closing on valid audio signals
  - Lowered default `gateStartFactor` from 1.5 to 1.0 (more permissive threshold)
  - Fixed hardcoded noise floor rise rate (now uses tunable `noiseFloorRise` parameter)
  - Increased SNR threshold from 2.0 to 3.0 to prevent floor drift during active audio
  - Added automatic recovery mechanism: forces noise floor down when gate stuck with signal present
  - Audio-reactive effects now respond correctly to normal audio levels
  - See `docs/audio-visual/audio-gate-fix-2025-01.md` for comprehensive documentation

---

## Previous Releases

### [Phase 1] - LED Strips Mode Implementation
- Added dual 160-LED strips infrastructure
- M5Stack 8encoder I2C integration
- 12 strip-specific advanced effects
- Propagation and synchronization modes

### [Base System] - Core Architecture
- Modular effect system with base classes
- PlatformIO project structure
- Performance monitoring and optimization
- 16 core visual effects
- 33 color palettes
- Smooth transition system