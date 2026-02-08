# LightwaveOS Audio-Visual Timing Architecture

**Document Version:** 1.0
**Date:** 2026-02-08
**Purpose:** Canonical specification for external specialist review
**Scope:** Audio pipeline timing, visual rendering synchronisation, and rate alignment analysis

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [System Overview](#2-system-overview)
3. [Audio Pipeline Architecture](#3-audio-pipeline-architecture)
4. [Visual Rendering Pipeline](#4-visual-rendering-pipeline)
5. [Cross-Domain Synchronisation](#5-cross-domain-synchronisation)
6. [Timing Constants & Dependencies](#6-timing-constants--dependencies)
7. [Rate Alignment Analysis](#7-rate-alignment-analysis)
8. [Challenges & Risk Assessment](#8-challenges--risk-assessment)
9. [File Reference Index](#9-file-reference-index)
10. [Function Reference Index](#10-function-reference-index)
11. [Appendices](#appendices)

---

## 1. Executive Summary

LightwaveOS is an ESP32-S3 firmware controlling 320 WS2812 LEDs arranged as a dual-strip Light Guide Plate (LGP). The system provides real-time audio-reactive visual effects with beat synchronisation.

### Current Architecture

| Domain | Rate | Period | Core |
|--------|------|--------|------|
| Audio DSP | 50 Hz | 20ms | Core 0 |
| Visual Rendering | 120 FPS | 8.33ms | Core 1 |
| **Ratio** | **2.4:1** | - | - |

### Original Design Intent

| Domain | Rate | Period | Core |
|--------|------|--------|------|
| Audio DSP | 125 Hz | 8ms | Core 0 |
| Visual Rendering | ~120 FPS | 8.33ms | Core 1 |
| **Ratio** | **~1:1** | - | - |

### Key Finding

The current 2.4:1 rate mismatch means approximately 60% of render frames process stale audio data. The system compensates via phase-locked loop (PLL) interpolation, but beat-reactive effects experience 0-20ms latency (vs. original 0-8ms design).

---

## 2. System Overview

### 2.1 Hardware Platform

- **MCU:** ESP32-S3 (dual-core Xtensa LX7, 240 MHz)
- **RAM:** 512 KB SRAM + 8 MB PSRAM (required)
- **LED Controller:** Dual WS2812 strips (160 LEDs each, 320 total)
- **Audio Input:** ES8311 I2S codec (ES v1.1 backend) or SPH0645 MEMS microphone
- **Wireless:** WiFi (REST API, WebSocket) + ESP-NOW (Sensory Bridge receiver)

### 2.2 Core Allocation

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32-S3                             │
├─────────────────────────────┬───────────────────────────────┤
│          CORE 0             │           CORE 1              │
├─────────────────────────────┼───────────────────────────────┤
│  AudioActor (Priority 4)    │  RendererActor (Priority 5)   │
│  - I2S DMA capture          │  - Effect execution           │
│  - Goertzel analysis        │  - LED buffer composition     │
│  - Tempo tracking           │  - FastLED output             │
│  - ControlBus smoothing     │  - Beat phase interpolation   │
│                             │                               │
│  NetworkActor (Priority 2)  │  IDLE1 Task (Watchdog)        │
│  - WiFi stack               │                               │
│  - REST/WebSocket handlers  │                               │
└─────────────────────────────┴───────────────────────────────┘
```

### 2.3 Data Flow Overview

```
I2S DMA (12.8 kHz)
       │
       ▼
┌──────────────────┐
│   AudioActor     │  Core 0, 50 Hz
│  (DSP Pipeline)  │
└────────┬─────────┘
         │ ControlBusFrame
         ▼
┌──────────────────┐
│  SnapshotBuffer  │  Lock-free publish/subscribe
│  (Cross-Core)    │
└────────┬─────────┘
         │ ReadLatest()
         ▼
┌──────────────────┐
│  RendererActor   │  Core 1, 120 FPS
│  (Visual Output) │
└────────┬─────────┘
         │
         ▼
    FastLED.show()
         │
         ▼
   WS2812 Protocol (320 LEDs)
```

---

## 3. Audio Pipeline Architecture

### 3.1 Sample Acquisition

#### ES v1.1 Backend (Current Primary)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sample Rate | 12,800 Hz | Self-clocked from I2S DMA |
| Bit Depth | 16-bit signed | Mono |
| Chunk Size | 64 samples | I2S DMA buffer |
| Hop Size | 256 samples | 4 chunks accumulated |
| Hop Period | 20 ms | 256 / 12800 |
| Hop Rate | **50 Hz** | Current operating rate |

#### LWLS Native Backend (Alternative)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sample Rate | 16,000 Hz | Configurable |
| Bit Depth | 16-bit signed | Mono |
| Chunk Size | 160 samples | 10ms window |
| Hop Size | 160 samples | Aligned with chunk |
| Hop Period | 10 ms | 160 / 16000 |
| Hop Rate | **100 Hz** | Native target rate |

### 3.2 DSP Processing Stages

#### Stage 1: DC Removal & AGC

```
Raw Samples → DC Blocking → AGC → Normalised Samples
```

| Function | File | Lines |
|----------|------|-------|
| `processHop()` | `AudioActor.cpp` | 750-900 |
| DC estimator | `AudioActor.cpp` | 853-865 |
| AGC loop | `AudioActor.cpp` | 872-889 |

**AGC Parameters:**
- Target RMS: 0.18
- Attack: 0.02 (per-frame alpha)
- Release: 0.005 (per-frame alpha)
- Gain range: [0.2, 8.0]

#### Stage 2: Frequency Analysis (Goertzel)

**8-Band Analysis:**
- Window: 512 samples (40ms)
- Bands: 60 Hz to 7,800 Hz (logarithmic)
- Update rate: ~31 Hz (when 512 samples accumulated)

**64-Bin Analysis (Sensory Bridge Parity):**
- Window: 1,500 samples (~94ms)
- Bins: 110 Hz to 4,186 Hz (semitone-spaced, A2 to C8)
- Update rate: ~10 Hz

| Function | File | Lines |
|----------|------|-------|
| `GoertzelAnalyzer::process()` | `GoertzelAnalyzer.cpp` | 45-180 |
| 64-bin computation | `AudioActor.cpp` | 1176-1248 |

#### Stage 3: Tempo Tracking

**Algorithm:** 96-bin Goertzel resonator bank (48-143 BPM, 1 BPM resolution)

**Novelty Sources:**
1. Spectral flux (64-bin FFT derivative)
2. VU derivative (RMS delta per hop)

**Key Constants (RATE-DEPENDENT):**

| Constant | Value | Dependency |
|----------|-------|------------|
| `SPECTRAL_LOG_HZ` | 50.0f | **Hardcoded to hop rate** |
| `VU_LOG_HZ` | 50.0f | **Hardcoded to hop rate** |
| `NOVELTY_DECAY` | 0.999f | **Per-frame (rate-dependent)** |
| `HYSTERESIS_FRAMES` | 10 | **Frame count (rate-dependent)** |

| Function | File | Lines |
|----------|------|-------|
| `TempoTracker::updateNovelty()` | `TempoTracker.cpp` | 158-261 |
| `TempoTracker::updateTempo()` | `TempoTracker.cpp` | 263-430 |
| Resonator computation | `TempoTracker.cpp` | 310-380 |

#### Stage 4: Chroma & Chord Detection

- 12-bin chromagram (pitch classes C through B)
- Triad classification: MAJOR, MINOR, DIMINISHED, AUGMENTED

| Function | File | Lines |
|----------|------|-------|
| `ChromaAnalyzer::process()` | `ChromaAnalyzer.cpp` | 35-120 |
| Chord classification | `ChromaAnalyzer.cpp` | 122-180 |

#### Stage 5: ControlBus Smoothing

**Dual-Rate Smoothing:**
- Fast path: α = 0.35 (reactive)
- Slow path: α = 0.12 (smooth)

**Attack/Release (RATE-DEPENDENT):**

| Parameter | Base Value | At 100 Hz |
|-----------|------------|-----------|
| `m_band_attack` | 0.25 | 0.125 (halved) |
| `m_band_release` | 0.02 | 0.01 (halved) |
| `m_heavy_band_attack` | 0.12 | 0.06 (halved) |
| `m_heavy_band_release` | 0.01 | 0.005 (halved) |

| Function | File | Lines |
|----------|------|-------|
| `ControlBus::update()` | `ControlBus.cpp` | 85-280 |
| `setMoodSmoothing()` | `ControlBus.cpp` | 106-114 |
| Spike detection | `ControlBus.cpp` | 131-276 |

### 3.3 ControlBusFrame Structure

Published at hop rate (50 Hz) via lock-free `SnapshotBuffer`:

```cpp
struct ControlBusFrame {
    // Sequence & Timing
    uint32_t hop_seq;                    // Monotonic counter
    uint64_t timestamp_us;               // Microsecond timestamp

    // Energy
    float rms, fast_rms;                 // Dual-rate RMS
    float flux, fast_flux;               // Dual-rate spectral flux

    // Frequency Bands
    float bands[8];                      // 8-band (60-7800 Hz)
    float heavy_bands[8];                // Extra-smoothed for LGP

    // Pitch Classes
    float chroma[12];                    // Raw chromagram
    float heavy_chroma[12];              // Smoothed chromagram

    // Full Spectrum
    float bins64[64];                    // 64-bin FFT
    float bins64Adaptive[64];            // Zone-normalised

    // Waveform
    int16_t waveform[128];               // Time-domain samples

    // Chord
    ChordState chordState;               // Root, type, confidence

    // Saliency (MIS)
    MusicalSaliencyFrame saliency;       // Novelty scores

    // Style
    MusicStyle currentStyle;             // Classification
    float styleConfidence;               // Confidence [0,1]

    // Silence
    float silentScale;                   // 0=silent, 1=active
    bool isSilent;

    // ES Backend Extensions (FEATURE_AUDIO_BACKEND_ESV11)
    float es_bpm;
    float es_tempo_confidence;
    bool es_beat_tick;
    float es_phase01_at_audio_t;
    uint8_t es_beat_in_bar;
    bool es_downbeat_tick;
};
```

**Size:** ~2 KB per frame

---

## 4. Visual Rendering Pipeline

### 4.1 Frame Timing

| Parameter | Value | Notes |
|-----------|-------|-------|
| Target FPS | 120 | 8.33ms frame budget |
| FastLED.show() | ~9.6ms | WS2812 protocol blocking |
| Effect budget | ~2ms | Per-frame render time |
| Actual FPS | 50-60 | Limited by FastLED.show() |

**Note:** The 120 FPS target is theoretical. FastLED.show() for 320 LEDs requires ~9.6ms, limiting practical maximum to ~100 FPS.

### 4.2 RendererActor Frame Loop

```cpp
void RendererActor::onTick() {
    // 1. Read audio snapshot
    uint32_t seq;
    m_controlBusBuffer->ReadLatest(m_lastControlBus, seq);
    bool newAudio = (seq != m_lastControlBusSeq);

    // 2. Advance beat phase (PLL)
    if (m_esBackendActive) {
        m_esBeatClock.tick(render_now);
    } else {
        m_musicalGrid.Tick(render_now);
    }

    // 3. Populate EffectContext
    EffectContext ctx = buildContext();
    ctx.audio = m_sharedAudioCtx;

    // 4. Execute effect
    effect->render(ctx);

    // 5. Output
    yield();  // Let IDLE1 task run
    FastLED.show();  // Blocking (~9.6ms)

    // 6. Throttle to target frame time
    throttleToTarget();
}
```

| Function | File | Lines |
|----------|------|-------|
| `onTick()` | `RendererActor.cpp` | 690-792 |
| Audio read | `RendererActor.cpp` | 1146-1268 |
| Effect execute | `RendererActor.cpp` | 1294-1431 |

### 4.3 EffectContext Structure

Passed to every `effect->render(ctx)` call:

```cpp
struct EffectContext {
    // LED Buffer
    CRGB* leds;                          // 320-LED unified buffer
    uint16_t ledCount;                   // Always 320 (or zone length)
    uint16_t centerPoint;                // LED 79 (centre origin)

    // Palette
    PaletteRef palette;                  // CRGBPalette16 wrapper

    // Animation Parameters
    uint8_t brightness;                  // 0-255
    uint8_t speed;                       // 0-255 (50 = 1.0x)
    uint8_t gHue;                        // Global rotating hue
    uint8_t intensity;                   // Effect intensity
    uint8_t saturation;                  // Colour saturation
    uint8_t complexity;                  // Pattern complexity
    uint8_t variation;                   // Random variation
    uint8_t mood;                        // Sensory Bridge MOOD (0=reactive, 255=dreamy)
    uint8_t fadeAmount;                  // Trail fade rate

    // Timing (CRITICAL FOR DT-CORRECTNESS)
    uint32_t deltaTimeMs;                // Frame delta (milliseconds)
    float deltaTimeSeconds;              // Frame delta (seconds, speed-scaled)
    uint32_t frameNumber;                // Effect-local frame counter
    uint32_t totalTimeMs;                // Effect runtime since init

    // Zone
    uint8_t zoneId;                      // 0xFF=global, 0-3=zone
    uint16_t zoneStart;                  // Zone start LED
    uint16_t zoneLength;                 // Zone LED count

    // Audio (nested)
    AudioContext audio;                  // See below

    // Helper Methods
    float getSafeDeltaSeconds() const;   // Clamped [0.0001, 0.05]
    float getDistanceFromCenter(uint16_t i) const;
    float getPhase() const;
};
```

### 4.4 AudioContext (Nested in EffectContext)

```cpp
struct AudioContext {
    // Core Data
    ControlBusFrame controlBus;          // By-value copy
    MusicalGridSnapshot musicalGrid;     // Beat/tempo/phase
    bool available;                      // Freshness flag
    bool trinityActive;                  // Trinity sync mode

    // Convenience Accessors (sampling from controlBus)
    float rms() const;
    float fastRms() const;
    float flux() const;
    float fastFlux() const;
    float bass() const;                  // bands[0] + bands[1]
    float mid() const;                   // bands[2] + bands[3] + bands[4]
    float treble() const;                // bands[5] + bands[6] + bands[7]
    float heavyBass() const;             // heavy_bands[0] + heavy_bands[1]
    float heavyMid() const;              // heavy_bands[2..4]
    float beatPhase() const;             // 0-1 beat phase
    float barPhase() const;              // 0-1 bar phase
    bool isOnBeat() const;               // Beat tick this frame
    bool isOnDownbeat() const;           // Downbeat tick this frame
    float beatStrength() const;          // Decaying beat intensity
    float bpm() const;                   // Current tempo
    float tempoConfidence() const;       // Tracking confidence
    float bin(uint8_t i) const;          // 64-bin access
    float binAdaptive(uint8_t i) const;  // Zone-normalised bin
    // ... additional accessors
};
```

### 4.5 Delta Time Handling

**getSafeDeltaSeconds():**

```cpp
float EffectContext::getSafeDeltaSeconds() const {
    float dt = deltaTimeSeconds;
    if (dt < 0.0001f) dt = 0.0001f;  // Floor: 0.1ms
    if (dt > 0.05f) dt = 0.05f;       // Ceiling: 50ms (20 FPS)
    return dt;
}
```

**Purpose:** Prevent physics explosion on frame drops while maintaining precision.

**Speed Scaling:**

`deltaTimeSeconds` is pre-multiplied by speed factor:

```cpp
float speedFactor = computeSpeedTimeFactor(ctx.speed);  // 0.0 to 2.0
float scaledDeltaSeconds = rawDeltaSeconds * speedFactor;
ctx.deltaTimeSeconds = scaledDeltaSeconds;
```

**Effects see "slow motion" at low speed settings.**

---

## 5. Cross-Domain Synchronisation

### 5.1 SnapshotBuffer (Lock-Free Communication)

```cpp
template<typename T>
class SnapshotBuffer {
    T m_data[2];                         // Double buffer
    std::atomic<uint32_t> m_seq;         // Sequence counter

public:
    void Publish(const T& frame);        // Writer (AudioActor)
    bool ReadLatest(T& out, uint32_t& seq); // Reader (RendererActor)
};
```

**Guarantees:**
- Writer never blocks
- Reader gets most recent frame (may skip frames)
- Sequence number detects staleness

### 5.2 Beat Phase Interpolation

#### MusicalGrid PLL (LWLS Native Backend)

```cpp
class MusicalGrid {
    float m_phase01;                     // Current beat phase [0,1)
    float m_bpmSmooth;                   // Smoothed tempo
    float m_confidence;                  // Tracking confidence

    void OnTempoEstimate(AudioTime t, float bpm, float conf);  // Audio domain
    void OnBeatObservation(AudioTime t, float strength, bool downbeat);
    void Tick(RenderTime now);           // **RENDERER DOMAIN (120 FPS)**
};
```

**PLL Behaviour:**
- `Tick()` advances phase based on smoothed BPM
- Phase correction on beat observation (gain: 0.35 for beat, 0.20 for downbeat)
- Confidence smoothing (τ = 1.0s)
- BPM smoothing (τ = 0.5s)

#### EsBeatClock (ES v1.1 Backend)

```cpp
class EsBeatClock {
    float m_phase01;
    float m_bpm;
    float m_beatStrength;

    void tick(RenderTime now);           // **RENDERER DOMAIN**
};
```

**Simpler model:** Direct consumption of ES backend tempo fields, no PLL.

| Function | File | Lines |
|----------|------|-------|
| `MusicalGrid::Tick()` | `MusicalGrid.cpp` | 89-145 |
| `EsBeatClock::tick()` | `EsBeatClock.cpp` | 125-145 |

### 5.3 Timing Diagram

```
Audio Domain (50 Hz):
  t=0ms    : hop_seq=0, publish ControlBusFrame
  t=20ms   : hop_seq=1, publish ControlBusFrame
  t=40ms   : hop_seq=2, publish ControlBusFrame
  ...

Render Domain (120 FPS target, ~100 FPS actual):
  t=0ms    : ReadLatest() → seq=0 (NEW), Tick() → phase=0.00
  t=10ms   : ReadLatest() → seq=0 (STALE), Tick() → phase=0.10 (interpolated)
  t=20ms   : ReadLatest() → seq=1 (NEW), Tick() → phase=0.20 (corrected)
  t=30ms   : ReadLatest() → seq=1 (STALE), Tick() → phase=0.30 (interpolated)
  ...
```

**Key Insight:** Render frames with stale audio (seq unchanged) still advance beat phase via PLL, providing smooth visual motion.

---

## 6. Timing Constants & Dependencies

### 6.1 Rate-Dependent Constants (MUST CHANGE AT 100 Hz)

| Constant | File | Line | Current | At 100 Hz | Formula |
|----------|------|------|---------|-----------|---------|
| `SPECTRAL_LOG_HZ` | `TempoTracker.h` | 55 | 50.0f | 100.0f | = HOP_RATE_HZ |
| `VU_LOG_HZ` | `TempoTracker.h` | 58 | 50.0f | 100.0f | = HOP_RATE_HZ |
| `NOVELTY_DECAY` | `TempoTracker.h` | 79 | 0.999f | **dt-based** | `exp(-decay_rate * dt)` |
| `HYSTERESIS_FRAMES` | `TempoTracker.h` | 85 | 10 | 20 | = HYSTERESIS_TIME_MS * HOP_RATE_HZ / 1000 |
| `spectral_scale_count` threshold | `TempoTracker.cpp` | 245 | 3 | 6 | = target_interval_ms * HOP_RATE_HZ / 1000 |
| `vu_scale_count` threshold | `TempoTracker.cpp` | 253 | 5 | 10 | = target_interval_ms * HOP_RATE_HZ / 1000 |
| `m_band_attack` | `ControlBus.cpp` | 107 | 0.25f | 0.125f | /= rate_scale |
| `m_band_release` | `ControlBus.cpp` | 108 | 0.02f | 0.01f | /= rate_scale |
| `m_heavy_band_attack` | `ControlBus.cpp` | 110 | 0.12f | 0.06f | /= rate_scale |
| `m_heavy_band_release` | `ControlBus.cpp` | 111 | 0.01f | 0.005f | /= rate_scale |
| `attack_rate` (Zone AGC) | `ControlBus.cpp` | 220 | 0.05f | 0.025f | /= rate_scale |
| `release_rate` (Zone AGC) | `ControlBus.cpp` | 221 | 0.05f | 0.025f | /= rate_scale |

### 6.2 Rate-Independent Constants (No Change Needed)

| Constant | File | Line | Value | Why Independent |
|----------|------|------|-------|-----------------|
| `SAMPLE_RATE` | `audio_config.h` | 12 | 12800 | Hardware fixed |
| `WINDOW_SIZE` (Goertzel) | `GoertzelAnalyzer.cpp` | 18 | 512 | Sample-based, not hop-based |
| `BPM_MIN/MAX` | `TempoTracker.h` | 35-36 | 48/143 | Musical range |
| `liveliness` tau | `ControlBus.cpp` | 476 | 0.30f | Uses `expf(-dt/tau)` |
| `EsBeatClock` decay | `EsBeatClock.cpp` | 142 | τ=0.30f | Uses `exp(-dt_s/tau)` |
| `MusicalGrid` phase advance | `MusicalGrid.cpp` | 115 | - | Uses `dt * bpm / 60` |

### 6.3 ES v1.1 Backend Hardcoded Constants (CANNOT CHANGE)

| Constant | File | Line | Value | Impact |
|----------|------|------|-------|--------|
| `NOVELTY_LOG_HZ` | `vendor/global_defines.h` | 47 | 50 | **FROZEN VENDOR CODE** |
| `SAMPLE_RATE` | `vendor/global_defines.h` | 22 | 12800 | **FROZEN VENDOR CODE** |
| `CHUNK_SIZE` | `vendor/global_defines.h` | 25 | 64 | **FROZEN VENDOR CODE** |

**Critical Constraint:** ES v1.1 backend cannot be modified without breaking Emotiscope parity.

---

## 7. Rate Alignment Analysis

### 7.1 Current State: 50 Hz Audio → 120 FPS Render

```
Audio Hops:    H0          H1          H2          H3
               |-----------|-----------|-----------|
Time:          0          20          40          60ms

Render Frames: R0 R1 R2 R3 R4 R5 R6 R7 R8 R9 R10 R11
               |--|--|--|--|--|--|--|--|--|--|---|--|
Time:          0  8 17 25 33 42 50 58 67 75 83  92ms

Fresh data?:   Y  N  N  Y  N  N  Y  N  N  Y  N   N
```

**Statistics:**
- Fresh audio frames: ~40%
- Stale audio frames: ~60%
- Maximum audio latency: 20ms
- Average audio latency: 10ms

### 7.2 Original Design: 125 Hz Audio → 120 FPS Render

```
Audio Hops:    H0    H1    H2    H3    H4    H5    H6
               |-----|-----|-----|-----|-----|-----|
Time:          0     8    16    24    32    40    48ms

Render Frames: R0  R1  R2  R3  R4  R5  R6  R7  R8  R9
               |---|---|---|---|---|---|---|---|---|
Time:          0   8  17  25  33  42  50  58  67  75ms

Fresh data?:   Y   Y   Y   Y   Y   Y   Y   Y   Y   Y
```

**Statistics:**
- Fresh audio frames: ~100% (slight drift)
- Maximum audio latency: 8ms
- Average audio latency: 4ms

### 7.3 Proposed: 100 Hz Audio → 120 FPS Render

```
Audio Hops:    H0      H1      H2      H3      H4
               |-------|-------|-------|-------|
Time:          0      10      20      30      40ms

Render Frames: R0 R1 R2 R3 R4 R5 R6 R7 R8 R9 R10
               |--|--|--|--|--|--|--|--|--|--|--|
Time:          0  8 17 25 33 42 50 58 67 75 83ms

Fresh data?:   Y  N  Y  N  Y  N  Y  N  Y  N  Y
```

**Statistics:**
- Fresh audio frames: ~50%
- Stale audio frames: ~50%
- Maximum audio latency: 10ms
- Average audio latency: 5ms

### 7.4 Comparison Summary

| Metric | 50 Hz (Current) | 100 Hz (Proposed) | 125 Hz (Original) |
|--------|-----------------|-------------------|-------------------|
| Fresh frames | 40% | 50% | ~100% |
| Max latency | 20ms | 10ms | 8ms |
| Avg latency | 10ms | 5ms | 4ms |
| Perceptible lag | Yes | Marginal | No |
| CPU load | 4.5% | 9% | 11% |

---

## 8. Challenges & Risk Assessment

### 8.1 ES v1.1 Backend Incompatibility (BLOCKER)

**Issue:** ES v1.1 backend has `NOVELTY_LOG_HZ = 50` hardcoded in frozen vendor code.

**Impact:** Cannot change ES backend hop rate without:
1. Modifying frozen vendor headers (breaks Emotiscope parity)
2. Forking ES v1.1 codebase (maintenance burden)
3. Implementing dual-rate architecture (complexity)

**Mitigation Options:**

| Option | Effort | Risk | Recommendation |
|--------|--------|------|----------------|
| Keep ES at 50 Hz, LWLS at 100 Hz | Medium | Low | **Recommended** |
| Patch vendor headers | Low | High | Not recommended |
| Fork ES v1.1 | High | Medium | Future consideration |

### 8.2 Tempo Tracking Algorithm Regression

**Issue:** Tempo tracking uses hop-rate-dependent constants throughout.

**Specific Risks:**

1. **Novelty Decay Too Fast:**
   - Current: 0.999^50 per second = 0.951 (4.9% decay/sec)
   - At 100 Hz: 0.999^100 per second = 0.905 (9.5% decay/sec)
   - **Impact:** Recent beats weighted less, tempo lock may become unstable

2. **Goertzel Coefficient Mismatch:**
   - Coefficients computed assuming 50 Hz input rate
   - At 100 Hz: frequency resolution changes
   - **Impact:** Resonator bank may not align with BPM grid

3. **Hysteresis Too Short:**
   - Current: 10 frames × 20ms = 200ms
   - At 100 Hz: 10 frames × 10ms = 100ms
   - **Impact:** Tempo may flip-flop more frequently

**Mitigation:** Convert all timing constants to time-based (dt-independent) formulas.

### 8.3 ControlBus Smoothing Regression

**Issue:** All smoothing alphas are per-frame, not per-second.

**Specific Risks:**

1. **Faster Attack/Release:**
   - At 100 Hz, alphas applied twice as often
   - **Impact:** More reactive but potentially jittery

2. **Zone AGC Oscillation:**
   - Faster adaptation may overshoot
   - **Impact:** Visible brightness pumping

**Mitigation:** Apply rate compensation factor:

```cpp
const float rate_scale = HOP_RATE_HZ / 50.0f;  // 2.0 at 100 Hz
m_band_attack = base_attack / rate_scale;
```

### 8.4 CPU Budget Overrun

**Issue:** Doubling hop rate doubles DSP workload on Core 0.

**Current Budget:**
- Audio DSP: ~0.9ms per hop × 50 hops/sec = 45ms/sec = **4.5% CPU**
- Headroom: 95.5%

**At 100 Hz:**
- Audio DSP: ~0.9ms per hop × 100 hops/sec = 90ms/sec = **9% CPU**
- Headroom: 91%

**Risk Assessment:**

| Factor | Impact |
|--------|--------|
| NetworkActor contention | Marginal |
| WiFi stack interference | Low |
| Watchdog margin | Adequate |
| Heap fragmentation | Unchanged |

**Mitigation:** Monitor Core 0 load post-deployment, reduce effect complexity if needed.

### 8.5 Beat Phase Interpolation Quality

**Issue:** Fewer audio updates per render frame means more interpolation.

**At 50 Hz:** Up to 2-3 render frames between audio updates
**At 100 Hz:** Up to 1-2 render frames between audio updates

**Risk Assessment:** **LOW** - MusicalGrid PLL already handles this well. Halving the interpolation window is strictly beneficial.

### 8.6 History Buffer Duration

**Issue:** Fixed-size history buffers cover less time at higher rates.

| Buffer | Size | At 50 Hz | At 100 Hz |
|--------|------|----------|-----------|
| `SPECTRAL_HISTORY_LENGTH` | 1024 | 20.48 sec | 10.24 sec |
| `VU_HISTORY_LENGTH` | 512 | 10.24 sec | 5.12 sec |

**Risk Assessment:** **LOW** - 5-10 seconds is still adequate for tempo tracking (typically needs 3-5 seconds minimum).

---

## 9. File Reference Index

### 9.1 Audio Pipeline

| File | Path | Purpose |
|------|------|---------|
| `audio_config.h` | `firmware/v2/src/config/audio_config.h` | Sample rate, hop size, rate constants |
| `AudioActor.cpp` | `firmware/v2/src/core/actors/AudioActor.cpp` | Main audio processing loop |
| `AudioActor.h` | `firmware/v2/src/core/actors/AudioActor.h` | AudioActor class definition |
| `ControlBus.cpp` | `firmware/v2/src/audio/contracts/ControlBus.cpp` | Smoothing, AGC, spike detection |
| `ControlBus.h` | `firmware/v2/src/audio/contracts/ControlBus.h` | ControlBusFrame structure |
| `SnapshotBuffer.h` | `firmware/v2/src/audio/contracts/SnapshotBuffer.h` | Lock-free publish/subscribe |
| `GoertzelAnalyzer.cpp` | `firmware/v2/src/audio/GoertzelAnalyzer.cpp` | Frequency analysis |
| `GoertzelAnalyzer.h` | `firmware/v2/src/audio/GoertzelAnalyzer.h` | Goertzel class definition |
| `ChromaAnalyzer.cpp` | `firmware/v2/src/audio/ChromaAnalyzer.cpp` | Chroma/chord detection |
| `ChromaAnalyzer.h` | `firmware/v2/src/audio/ChromaAnalyzer.h` | ChromaAnalyzer class definition |
| `StyleDetector.cpp` | `firmware/v2/src/audio/StyleDetector.cpp` | Music style classification |
| `StyleDetector.h` | `firmware/v2/src/audio/StyleDetector.h` | StyleDetector class definition |
| `MusicalSaliency.h` | `firmware/v2/src/audio/contracts/MusicalSaliency.h` | MIS novelty types |

### 9.2 Tempo Tracking

| File | Path | Purpose |
|------|------|---------|
| `TempoTracker.cpp` | `firmware/v2/src/audio/tempo/TempoTracker.cpp` | Tempo detection algorithm |
| `TempoTracker.h` | `firmware/v2/src/audio/tempo/TempoTracker.h` | Constants, class definition |

### 9.3 Beat Phase Interpolation

| File | Path | Purpose |
|------|------|---------|
| `MusicalGrid.cpp` | `firmware/v2/src/audio/contracts/MusicalGrid.cpp` | LWLS PLL implementation |
| `MusicalGrid.h` | `firmware/v2/src/audio/contracts/MusicalGrid.h` | MusicalGrid class definition |
| `EsBeatClock.cpp` | `firmware/v2/src/audio/backends/esv11/EsBeatClock.cpp` | ES beat phase tracking |
| `EsBeatClock.h` | `firmware/v2/src/audio/backends/esv11/EsBeatClock.h` | EsBeatClock class definition |

### 9.4 ES v1.1 Backend

| File | Path | Purpose |
|------|------|---------|
| `EsV11Backend.cpp` | `firmware/v2/src/audio/backends/esv11/EsV11Backend.cpp` | ES backend implementation |
| `EsV11Backend.h` | `firmware/v2/src/audio/backends/esv11/EsV11Backend.h` | ES backend class definition |
| `EsV11Shim.cpp` | `firmware/v2/src/audio/backends/esv11/vendor/EsV11Shim.cpp` | Emotiscope DSP pipeline |
| `global_defines.h` | `firmware/v2/src/audio/backends/esv11/vendor/global_defines.h` | **FROZEN** vendor constants |

### 9.5 Visual Rendering

| File | Path | Purpose |
|------|------|---------|
| `RendererActor.cpp` | `firmware/v2/src/core/actors/RendererActor.cpp` | Frame loop, effect execution |
| `RendererActor.h` | `firmware/v2/src/core/actors/RendererActor.h` | RendererActor class definition |
| `EffectContext.h` | `firmware/v2/src/plugins/api/EffectContext.h` | Effect parameter injection |
| `FastLedDriver.cpp` | `firmware/v2/src/hal/led/FastLedDriver.cpp` | LED output driver |

### 9.6 Effect Framework

| File | Path | Purpose |
|------|------|---------|
| `IEffect.h` | `firmware/v2/src/plugins/api/IEffect.h` | Effect interface |
| `SmoothingEngine.h` | `firmware/v2/src/effects/enhancement/SmoothingEngine.h` | dt-correct smoothing primitives |
| `MotionEngine.h` | `firmware/v2/src/effects/enhancement/MotionEngine.h` | Phase, spring, subpixel helpers |

---

## 10. Function Reference Index

### 10.1 Audio Pipeline Functions

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| `AudioActor::processHop()` | `AudioActor.cpp` | 750-1300 | Main DSP processing |
| `AudioActor::onTick()` | `AudioActor.cpp` | 589-619 | Audio loop entry |
| `ControlBus::update()` | `ControlBus.cpp` | 85-280 | Smoothing, AGC |
| `ControlBus::setMoodSmoothing()` | `ControlBus.cpp` | 106-114 | MOOD parameter |
| `ControlBus::GetFrame()` | `ControlBus.cpp` | 320-380 | Build ControlBusFrame |
| `GoertzelAnalyzer::process()` | `GoertzelAnalyzer.cpp` | 45-180 | Frequency analysis |
| `ChromaAnalyzer::process()` | `ChromaAnalyzer.cpp` | 35-120 | Chroma extraction |
| `StyleDetector::classify()` | `StyleDetector.cpp` | 55-140 | Style classification |

### 10.2 Tempo Tracking Functions

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| `TempoTracker::updateNovelty()` | `TempoTracker.cpp` | 158-261 | Novelty computation |
| `TempoTracker::updateTempo()` | `TempoTracker.cpp` | 263-430 | Resonator bank, BPM estimation |
| `TempoTracker::checkSilence()` | `TempoTracker.cpp` | 263-293 | Silence detection |
| `TempoTracker::getOutput()` | `TempoTracker.cpp` | 432-450 | Build TempoTrackerOutput |

### 10.3 Beat Phase Functions

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| `MusicalGrid::OnTempoEstimate()` | `MusicalGrid.cpp` | 45-65 | Tempo input (audio domain) |
| `MusicalGrid::OnBeatObservation()` | `MusicalGrid.cpp` | 67-87 | Beat input (audio domain) |
| `MusicalGrid::Tick()` | `MusicalGrid.cpp` | 89-145 | Phase advance (render domain) |
| `EsBeatClock::tick()` | `EsBeatClock.cpp` | 125-145 | ES phase advance |

### 10.4 Rendering Functions

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| `RendererActor::onTick()` | `RendererActor.cpp` | 690-792 | Frame loop entry |
| `RendererActor::renderFrame()` | `RendererActor.cpp` | 1146-1431 | Audio read, effect execute |
| `EffectContext::getSafeDeltaSeconds()` | `EffectContext.h` | 689-708 | Clamped dt |
| `FastLedDriver::show()` | `FastLedDriver.cpp` | 236-270 | LED output |

### 10.5 Cross-Core Functions

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| `SnapshotBuffer::Publish()` | `SnapshotBuffer.h` | 45-60 | Lock-free write |
| `SnapshotBuffer::ReadLatest()` | `SnapshotBuffer.h` | 62-80 | Lock-free read |

---

## Appendices

### Appendix A: Configuration Change for 100 Hz

```cpp
// firmware/v2/src/config/audio_config.h

#if FEATURE_AUDIO_BACKEND_ESV11
    // ES v1.1 backend: FROZEN at 50 Hz
    constexpr uint16_t SAMPLE_RATE = 12800;
    constexpr uint16_t HOP_SIZE = 256;  // 50 Hz
#else
    // LWLS native backend: 100 Hz target
    constexpr uint16_t SAMPLE_RATE = 16000;
    constexpr uint16_t HOP_SIZE = 160;  // 100 Hz
#endif

// Derived constants (DO NOT HARDCODE)
constexpr float HOP_DURATION_MS = (HOP_SIZE * 1000.0f) / SAMPLE_RATE;
constexpr float HOP_RATE_HZ = SAMPLE_RATE / static_cast<float>(HOP_SIZE);
```

### Appendix B: dt-Independent Decay Pattern

```cpp
// BEFORE (rate-dependent):
constexpr float NOVELTY_DECAY = 0.999f;  // Per frame
for (auto& v : history) {
    v *= NOVELTY_DECAY;
}

// AFTER (rate-independent):
constexpr float DECAY_RATE_PER_SEC = 0.05f;  // 5% per second
const float frame_decay = expf(-DECAY_RATE_PER_SEC * dt);
for (auto& v : history) {
    v *= frame_decay;
}
```

### Appendix C: Rate-Compensated Smoothing

```cpp
// BEFORE (rate-dependent):
m_band_attack = 0.25f - 0.17f * moodNorm;

// AFTER (rate-compensated):
const float rate_scale = HOP_RATE_HZ / 50.0f;  // 1.0 at 50 Hz, 2.0 at 100 Hz
m_band_attack = (0.25f - 0.17f * moodNorm) / rate_scale;
```

### Appendix D: Glossary

| Term | Definition |
|------|------------|
| **Hop** | Fixed-size block of audio samples processed together |
| **Hop Rate** | Frequency of hop processing (SAMPLE_RATE / HOP_SIZE) |
| **Goertzel** | Efficient single-frequency DFT algorithm |
| **PLL** | Phase-Locked Loop for beat phase interpolation |
| **AGC** | Automatic Gain Control |
| **MIS** | Musical Intelligence System (saliency detection) |
| **LGP** | Light Guide Plate (the physical LED panel) |
| **dt** | Delta time (time since last frame/hop) |
| **Alpha** | Smoothing coefficient for exponential moving average |
| **Sensory Bridge** | External audio analyser hardware (ESP-NOW source) |
| **Trinity** | Offline audio analysis with position sync |
| **MOOD** | Sensory Bridge reactivity parameter (0=reactive, 255=dreamy) |

---

## Document Control

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-02-08 | Claude (Anthropic) | Initial specification |

**Review Status:** Pending external specialist review

**Distribution:** Internal + External Consultants

---

*End of Document*
