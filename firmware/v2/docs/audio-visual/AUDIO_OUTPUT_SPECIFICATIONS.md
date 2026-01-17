# Audio Output Specifications

**LightwaveOS v2 Audio-Visual Processing Guide - Part 1**

This document provides comprehensive technical specifications for all audio outputs available to visual effects in LightwaveOS v2.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [ControlBus Data Structures](#2-controlbus-data-structures)
3. [Processing Pipeline (8 Stages)](#3-processing-pipeline-8-stages)
4. [Value Ranges & Clamping](#4-value-ranges--clamping)
5. [Timing Parameters](#5-timing-parameters)
6. [Heavy Bands vs Regular Bands](#6-heavy-bands-vs-regular-bands)
7. [Effect Context Access Methods](#7-effect-context-access-methods)
8. [Audio Presets & Tuning](#8-audio-presets--tuning)
9. [Critical Constants Reference](#9-critical-constants-reference)

---

## 1. Architecture Overview

### Audio Pipeline Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        I2S MICROPHONE INPUT                                  │
│                         (16 kHz, mono)                                       │
└─────────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                         AUDIO ACTOR (Core 0)                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Hop Processing (every 256 samples = 16ms)                               ││
│  │   • Goertzel Analysis (64 bins, adaptive windows)                       ││
│  │   • RMS calculation                                                     ││
│  │   • Spectral flux computation                                           ││
│  │   • Percussion onset detection (snare/hihat)                            ││
│  └─────────────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                          CONTROLBUS (Thread-Safe)                            │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Stage 1: Input Clamping         → All values bounded [0.0, 1.0]         ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │ Stage 2: Spike Detection        → 3-frame lookahead, 32ms delay         ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │ Stage 3: Zone AGC               → 4 zones, asymmetric attack/release    ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │ Stage 4: Band Smoothing         → Normal (15%/3%) vs Heavy (8%/1.5%)    ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │ Stage 5: RMS/Flux Smoothing     → Fast α=0.35, Slow α=0.12              ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │ Stage 6: Chord Detection        → Triad analysis, confidence ≥0.3       ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │ Stage 7: Musical Saliency       → 4 dimensions, weighted combination    ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │ Stage 8: Silence Detection      → 5s hysteresis, 400ms fade             ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  Output: ControlBusFrame (copied by value to effects)                        │
└─────────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                       RENDERER ACTOR (Core 1, 120 FPS)                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ EffectContext.audio                                                     ││
│  │   • controlBus: ControlBusFrame (complete snapshot)                     ││
│  │   • musicalGrid: MusicalGridSnapshot (beat tracking)                    ││
│  │   • available: bool (true if audio <100ms old)                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  Effects call ctx.audio.* methods (rms(), bass(), beatPhase(), etc.)        │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Source Files

| File | Purpose |
|------|---------|
| `v2/src/audio/contracts/ControlBus.h` | Data structure definitions |
| `v2/src/audio/contracts/ControlBus.cpp` | Processing pipeline implementation |
| `v2/src/audio/AudioTuning.h` | Tuning parameters and presets |
| `v2/src/audio/AudioActor.h` | Timing constants, hop size |
| `v2/src/plugins/api/EffectContext.h` | Effect access methods |

---

## 2. ControlBus Data Structures

### 2.1 Constants

```cpp
// Source: v2/src/audio/contracts/ControlBus.h (lines 15-20)

static constexpr uint8_t CONTROLBUS_NUM_BANDS  = 8;      // Legacy 8-band spectrum
static constexpr uint8_t CONTROLBUS_NUM_CHROMA = 12;     // 12 pitch classes (C through B)
static constexpr uint8_t CONTROLBUS_WAVEFORM_N = 128;    // Sensory Bridge native resolution
static constexpr size_t LOOKAHEAD_FRAMES = 3;            // Ring buffer for spike detection
static constexpr uint8_t CONTROLBUS_NUM_ZONES = 4;       // 4 frequency zones for AGC
static constexpr uint8_t BINS_64_COUNT = 64;             // Full 64-bin Goertzel spectrum
```

### 2.2 ControlBusRawInput (Audio Thread → ControlBus)

```cpp
// Source: v2/src/audio/contracts/ControlBus.h (lines 45-70)

struct ControlBusRawInput {
    // === Core Energy Metrics ===
    float rms = 0.0f;                          // 0..1 RMS energy (loudness)
    float flux = 0.0f;                         // 0..1 spectral flux (onset/novelty proxy)

    // === 8-Band Frequency Decomposition ===
    float bands[8] = {0};                      // 0..1 per-band magnitudes
    // Band 0: Sub-bass (20-60 Hz)
    // Band 1: Bass (60-120 Hz)
    // Band 2: Low-mid (120-250 Hz)
    // Band 3: Mid (250-500 Hz)
    // Band 4: Upper-mid (500-1000 Hz)
    // Band 5: Presence (1-2 kHz)
    // Band 6: Brilliance (2-4 kHz)
    // Band 7: Air (4-8 kHz)

    // === 12-Bin Chromagram (Pitch Classes) ===
    float chroma[12] = {0};                    // 0..1 pitch-class energy
    // chroma[0] = C, chroma[1] = C#, ..., chroma[11] = B

    // === Time-Domain Waveform ===
    int16_t waveform[128] = {0};              // Raw samples (-32768 to 32767)

    // === Percussion Onset Detection ===
    float snareEnergy = 0.0f;                 // 0..1 snare band energy (150-300 Hz)
    float hihatEnergy = 0.0f;                 // 0..1 hi-hat band energy (6-12 kHz)
    bool snareTrigger = false;                // Single-frame spike on snare onset
    bool hihatTrigger = false;                // Single-frame spike on hi-hat onset

    // === 64-Bin Full Spectrum ===
    float bins64[64] = {0};                   // 0..1 normalized 64-bin FFT
    // bins64[0] = A1 (55 Hz)
    // bins64[63] = C7 (2093 Hz)
    // Each bin is one semitone apart: freq = 55 * 2^(bin/12)

    // === Tempo Tracker State (for saliency computation; effects use MusicalGrid) ===
    bool tempoLocked = false;                 // True if phase-locked to beat
    float tempoConfidence = 0.0f;             // 0-1 lock confidence
    bool tempoBeatTick = false;               // Single-frame pulse on beat
};
```

### 2.3 ControlBusFrame (Published to Effects)

```cpp
// Source: v2/src/audio/contracts/ControlBus.h (lines 80-140)

struct ControlBusFrame {
    AudioTime t;                              // Timestamp
    uint32_t hop_seq = 0;                     // Monotonic hop counter (for change detection)

    // === RMS & Flux (Raw + Smoothed Variants) ===
    float rms = 0.0f;                         // Smoothed RMS (use for most effects)
    float flux = 0.0f;                        // Smoothed spectral flux
    float fast_rms = 0.0f;                    // Unsmoothed RMS (fast transient response)
    float fast_flux = 0.0f;                   // Unsmoothed flux

    // === Band & Chroma (Two Smoothing Variants) ===
    float bands[8] = {0};                     // Normal attack/release smoothed
    float chroma[12] = {0};                   // Normal attack/release smoothed
    float heavy_bands[8] = {0};               // Extra-smooth (ambient/slow effects)
    float heavy_chroma[12] = {0};             // Extra-smooth chroma

    // === Time-Domain Waveform ===
    int16_t waveform[128] = {0};              // Raw waveform (passthrough)

    // === Chord Detection ===
    ChordState chordState;                    // Triad detection result
    // chordState.rootNote: 0-11 (C through B)
    // chordState.type: NONE, MAJOR, MINOR, DIMINISHED, AUGMENTED
    // chordState.confidence: 0.0-1.0

    // === Musical Saliency (4 Dimensions) ===
    MusicalSaliencyFrame saliency;            // Musical intelligence metrics
    // saliency.harmonicNovelty: chord changes
    // saliency.rhythmicNovelty: beat pattern changes
    // saliency.timbralNovelty: spectral changes
    // saliency.dynamicNovelty: loudness changes
    // saliency.overallSaliency: weighted combination

    // === Style Detection ===
    MusicStyle currentStyle;                  // RHYTHMIC, HARMONIC, MELODIC, TEXTURE, DYNAMIC
    float styleConfidence = 0.0f;             // 0.0-1.0

    // === Percussion (Passthrough) ===
    float snareEnergy = 0.0f;
    float hihatEnergy = 0.0f;
    bool snareTrigger = false;
    bool hihatTrigger = false;

    // === 64-Bin Spectrum ===
    float bins64[64] = {0};                   // Full Goertzel spectrum

    // === Tempo Tracker (for saliency; effects use MusicalGrid via ctx.audio.*) ===
    bool tempoLocked = false;
    float tempoConfidence = 0.0f;
    bool tempoBeatTick = false;

    // === Silence Detection ===
    float silentScale = 1.0f;                 // 0.0=silent, 1.0=active (multiply with brightness)
    bool isSilent = false;                    // True when silence threshold exceeded duration
};
```

---

## 3. Processing Pipeline (8 Stages)

### Stage 1: Input Clamping

All raw inputs are immediately clamped to [0.0, 1.0]:

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 50-60)

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

// Applied to all inputs:
m_frame.fast_rms = clamp01(raw.rms);
m_frame.fast_flux = clamp01(raw.flux);
for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
    clamped_bands[i] = clamp01(raw.bands[i]);
}
for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
    clamped_chroma[i] = clamp01(raw.chroma[i]);
}
```

### Stage 2: Spike Detection (Lookahead Smoothing)

Removes single-frame spikes that cause visual jitter:

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 80-140)

// 3-frame ring buffer structure
struct LookaheadBuffer {
    float history[3][64] = {{0}};  // 3-frame history for 64 bins
    size_t current_frame = 0;       // Ring buffer write index
    size_t frames_filled = 0;       // Warmup counter (0-3)
    bool enabled = true;            // Runtime toggle
};

// Spike detection algorithm:
// When middle frame deviates significantly from neighbors, replace with average
void detectAndCorrectSpike(LookaheadBuffer& buffer, uint8_t numBins) {
    const size_t oldest = (buffer.current_frame + 1) % 3;
    const size_t middle = (buffer.current_frame + 2) % 3;
    const size_t newest = buffer.current_frame;

    for (uint8_t i = 0; i < numBins; ++i) {
        float oldest_val = buffer.history[oldest][i];
        float middle_val = buffer.history[middle][i];
        float newest_val = buffer.history[newest][i];

        // Expected value = average of neighbors
        const float expected = (oldest_val + newest_val) * 0.5f;
        const float deviation = fabs(middle_val - expected);

        // Threshold: 15% of expected value, with 0.02f floor
        const float threshold = (expected * 0.15f > 0.02f) ?
                               expected * 0.15f : 0.02f;

        if (deviation > threshold) {
            // Replace spike with interpolated value
            buffer.history[middle][i] = expected;
        }
    }
}

// Output is delayed 2 frames (~32ms @ 60fps) to allow lookahead
```

**Key Parameters:**
- Buffer depth: 3 frames
- Deviation threshold: 15% of expected value
- Threshold floor: 0.02f (prevents over-correction of quiet signals)
- Output delay: 32ms

### Stage 3: Zone AGC (Automatic Gain Control)

Normalizes frequency bands per-zone to handle varying music dynamics:

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 150-220)

struct ZoneAGC {
    float max_mag = 0.0f;           // Current zone maximum
    float max_mag_follower = 1.0f;  // Smoothed maximum (starts at 1.0 to avoid spike)
    float attack_rate = 0.05f;       // Rise rate (default)
    float release_rate = 0.05f;      // Fall rate (default)
    float min_floor = 0.01f;         // Minimum follower value (caps gain at 100x)
};

// Zone boundaries for 8-band system:
// Zone 0: bands 0-1  (20-120 Hz)   - Sub-bass/Bass
// Zone 1: bands 2-3  (120-500 Hz)  - Low-mid/Mid
// Zone 2: bands 4-5  (500-2000 Hz) - Upper-mid/Presence
// Zone 3: bands 6-7  (2-8 kHz)     - Brilliance/Air

void applyZoneAGC(float* bands_out, const float* bands_in) {
    for (uint8_t z = 0; z < 4; ++z) {
        // Find maximum magnitude in zone
        float zone_max = 0.0f;
        for (uint8_t b = z * 2; b < (z + 1) * 2; ++b) {
            if (bands_in[b] > zone_max) zone_max = bands_in[b];
        }

        // Asymmetric attack/release follower
        if (zone_max > m_zones[z].max_mag_follower) {
            // Attack: rise toward new maximum
            float delta = zone_max - m_zones[z].max_mag_follower;
            m_zones[z].max_mag_follower += delta * m_zones[z].attack_rate;
        } else {
            // Release: slowly decay toward new (lower) maximum
            float delta = m_zones[z].max_mag_follower - zone_max;
            m_zones[z].max_mag_follower -= delta * m_zones[z].release_rate;
        }

        // Clamp follower to minimum floor (prevents infinite gain)
        if (m_zones[z].max_mag_follower < m_zones[z].min_floor) {
            m_zones[z].max_mag_follower = m_zones[z].min_floor;
        }

        // Normalize bands by zone follower
        float norm_factor = 1.0f / m_zones[z].max_mag_follower;
        for (uint8_t b = z * 2; b < (z + 1) * 2; ++b) {
            bands_out[b] = clamp01(bands_in[b] * norm_factor);
        }
    }
}
```

**Key Parameters:**
- Default attack: 0.05 (5% per hop)
- Default release: 0.05 (5% per hop)
- Minimum floor: 0.01f (maximum gain = 100x)
- Zones: 4 (2 bands each)

### Stage 4: Band Smoothing (Normal vs Heavy)

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 230-270)

// Linear interpolation helper
static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Default smoothing parameters
float m_band_attack = 0.15f;          // Normal: 15% per hop rise
float m_band_release = 0.03f;         // Normal: 3% per hop decay
float m_heavy_band_attack = 0.08f;    // Heavy: 8% per hop rise
float m_heavy_band_release = 0.015f;  // Heavy: 1.5% per hop decay

void smoothBands(const float* normalized_bands) {
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        float target = normalized_bands[i];

        // === Normal Bands (faster response) ===
        float alpha = (target > m_bands_s[i]) ? m_band_attack : m_band_release;
        m_bands_s[i] = lerp(m_bands_s[i], target, alpha);
        m_frame.bands[i] = m_bands_s[i];

        // === Heavy Bands (extra smooth for ambient) ===
        float heavyAlpha = (target > m_heavy_bands_s[i]) ?
                           m_heavy_band_attack : m_heavy_band_release;
        m_heavy_bands_s[i] = lerp(m_heavy_bands_s[i], target, heavyAlpha);
        m_frame.heavy_bands[i] = m_heavy_bands_s[i];
    }
}
```

### Stage 5: RMS/Flux Smoothing

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 280-300)

float m_alpha_fast = 0.35f;   // Fast smoothing (35% per hop)
float m_alpha_slow = 0.12f;   // Slow smoothing (12% per hop)

// RMS smoothing (fast alpha for transient response)
m_rms_s = lerp(m_rms_s, m_frame.fast_rms, m_alpha_fast);
m_frame.rms = m_rms_s;

// Flux smoothing (slow alpha for stability)
m_flux_s = lerp(m_flux_s, m_frame.fast_flux, m_alpha_slow);
m_frame.flux = m_flux_s;
```

### Stage 6: Chord Detection

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 310-400)

void detectChord(const float* chroma) {
    ChordState& cs = m_frame.chordState;

    // 1. Find dominant pitch class (root)
    uint8_t rootIdx = 0;
    float rootVal = chroma[0];
    float totalEnergy = 0.0f;

    for (uint8_t i = 0; i < 12; ++i) {
        totalEnergy += chroma[i];
        if (chroma[i] > rootVal) {
            rootVal = chroma[i];
            rootIdx = i;
        }
    }

    // 2. Calculate interval indices (semitones from root)
    uint8_t minorThirdIdx = (rootIdx + 3) % 12;   // 3 semitones
    uint8_t majorThirdIdx = (rootIdx + 4) % 12;   // 4 semitones
    uint8_t dimFifthIdx = (rootIdx + 6) % 12;     // 6 semitones (tritone)
    uint8_t perfectFifthIdx = (rootIdx + 7) % 12; // 7 semitones
    uint8_t augFifthIdx = (rootIdx + 8) % 12;     // 8 semitones

    float minorThird = chroma[minorThirdIdx];
    float majorThird = chroma[majorThirdIdx];
    float dimFifth = chroma[dimFifthIdx];
    float perfectFifth = chroma[perfectFifthIdx];
    float augFifth = chroma[augFifthIdx];

    // 3. Determine chord type
    bool hasMinorThird = minorThird > majorThird;
    float thirdVal = hasMinorThird ? minorThird : majorThird;

    float fifthVal = perfectFifth;
    if (perfectFifth >= dimFifth && perfectFifth >= augFifth) {
        cs.type = hasMinorThird ? ChordType::MINOR : ChordType::MAJOR;
        fifthVal = perfectFifth;
    } else if (dimFifth > perfectFifth && dimFifth > augFifth) {
        cs.type = ChordType::DIMINISHED;
        fifthVal = dimFifth;
    } else {
        cs.type = ChordType::AUGMENTED;
        fifthVal = augFifth;
    }

    cs.rootNote = rootIdx;

    // 4. Calculate confidence (triad energy ratio)
    float triadEnergy = rootVal + thirdVal + fifthVal;
    if (totalEnergy > 0.01f) {
        // Normalize by 0.4 (typical triad energy fraction)
        cs.confidence = clamp01((triadEnergy / totalEnergy) / 0.4f);
    } else {
        cs.confidence = 0.0f;
        cs.type = ChordType::NONE;
    }

    // 5. Apply confidence threshold
    if (cs.confidence < 0.3f) {
        cs.type = ChordType::NONE;
    }
}
```

### Stage 7: Musical Saliency

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 410-520)

struct SaliencyTuning {
    // Rise/fall time constants (seconds)
    float harmonicRiseTime = 0.15f;    // 150ms
    float harmonicFallTime = 0.80f;    // 800ms
    float rhythmicRiseTime = 0.05f;    // 50ms
    float rhythmicFallTime = 0.30f;    // 300ms
    float timbralRiseTime = 0.10f;     // 100ms
    float timbralFallTime = 0.50f;     // 500ms
    float dynamicRiseTime = 0.08f;     // 80ms
    float dynamicFallTime = 0.40f;     // 400ms

    // Derivative thresholds
    float fluxDerivativeThreshold = 0.05f;   // Timbral sensitivity
    float rmsDerivativeThreshold = 0.02f;    // Dynamic sensitivity

    // Output weights (sum = 1.0)
    float harmonicWeight = 0.25f;
    float rhythmicWeight = 0.30f;
    float timbralWeight = 0.20f;
    float dynamicWeight = 0.25f;
};

void computeSaliency() {
    MusicalSaliencyFrame& sal = m_frame.saliency;
    const ChordState& chord = m_frame.chordState;

    // === Harmonic Novelty (chord changes) ===
    float harmonicRaw = chord.confidence * 0.3f;  // Base from confidence
    if (chord.rootNote != sal.prevChordRoot) {
        harmonicRaw = 1.0f;  // Max spike on root change
    } else if (static_cast<uint8_t>(chord.type) != sal.prevChordType &&
               chord.type != ChordType::NONE) {
        harmonicRaw = fmaxf(harmonicRaw, 0.6f);  // Quality change = 0.6 minimum
    }
    sal.harmonicNovelty = harmonicRaw;
    sal.prevChordRoot = chord.rootNote;
    sal.prevChordType = static_cast<uint8_t>(chord.type);

    // === Timbral Novelty (spectral flux derivative) ===
    float fluxDelta = m_frame.flux - sal.prevFlux;
    if (fluxDelta < 0.0f) fluxDelta = -fluxDelta;
    sal.timbralNovelty = clamp01(fluxDelta / m_saliencyTuning.fluxDerivativeThreshold);
    sal.prevFlux = m_frame.flux;

    // === Dynamic Novelty (RMS derivative) ===
    float rmsDelta = m_frame.rms - sal.prevRms;
    if (rmsDelta < 0.0f) rmsDelta = -rmsDelta;
    sal.dynamicNovelty = clamp01(rmsDelta / m_saliencyTuning.rmsDerivativeThreshold);
    sal.prevRms = m_frame.rms;

    // === Rhythmic Novelty (Tempo tracker integration) ===
    if (m_frame.tempoLocked) {
        float baseRhythmic = m_frame.tempoConfidence * 0.8f;
        if (m_frame.tempoBeatTick) {
            sal.rhythmicNovelty = clamp01(baseRhythmic + 0.5f);  // Spike on beat
        } else {
            sal.rhythmicNovelty = baseRhythmic;
        }
    } else {
        sal.rhythmicNovelty = clamp01(m_frame.fast_flux * 0.5f);  // Flux fallback
    }

    // === Asymmetric Smoothing ===
    auto asymmetricSmooth = [](float current, float target,
                               float riseAlpha, float fallAlpha) {
        float alpha = (target > current) ? riseAlpha : fallAlpha;
        return current + (target - current) * alpha;
    };

    sal.harmonicNoveltySmooth = asymmetricSmooth(
        sal.harmonicNoveltySmooth, sal.harmonicNovelty,
        m_saliencyTuning.harmonicRiseTime, m_saliencyTuning.harmonicFallTime);

    sal.rhythmicNoveltySmooth = asymmetricSmooth(
        sal.rhythmicNoveltySmooth, sal.rhythmicNovelty,
        m_saliencyTuning.rhythmicRiseTime, m_saliencyTuning.rhythmicFallTime);

    sal.timbralNoveltySmooth = asymmetricSmooth(
        sal.timbralNoveltySmooth, sal.timbralNovelty,
        m_saliencyTuning.timbralRiseTime, m_saliencyTuning.timbralFallTime);

    sal.dynamicNoveltySmooth = asymmetricSmooth(
        sal.dynamicNoveltySmooth, sal.dynamicNovelty,
        m_saliencyTuning.dynamicRiseTime, m_saliencyTuning.dynamicFallTime);

    // === Overall Saliency (weighted sum) ===
    sal.overallSaliency = clamp01(
        sal.harmonicNoveltySmooth * m_saliencyTuning.harmonicWeight +
        sal.rhythmicNoveltySmooth * m_saliencyTuning.rhythmicWeight +
        sal.timbralNoveltySmooth * m_saliencyTuning.timbralWeight +
        sal.dynamicNoveltySmooth * m_saliencyTuning.dynamicWeight
    );

    // === Dominant Type ===
    float maxSaliency = sal.harmonicNoveltySmooth;
    sal.dominantType = static_cast<uint8_t>(SaliencyType::HARMONIC);

    if (sal.rhythmicNoveltySmooth > maxSaliency) {
        maxSaliency = sal.rhythmicNoveltySmooth;
        sal.dominantType = static_cast<uint8_t>(SaliencyType::RHYTHMIC);
    }
    if (sal.timbralNoveltySmooth > maxSaliency) {
        maxSaliency = sal.timbralNoveltySmooth;
        sal.dominantType = static_cast<uint8_t>(SaliencyType::TIMBRAL);
    }
    if (sal.dynamicNoveltySmooth > maxSaliency) {
        sal.dominantType = static_cast<uint8_t>(SaliencyType::DYNAMIC);
    }
}
```

### Stage 8: Silence Detection

```cpp
// Source: v2/src/audio/contracts/ControlBus.cpp (lines 530-580)

float m_silence_threshold = 0.01f;      // RMS below this = silent
float m_silence_hysteresis_ms = 5000.0f; // 5 seconds before triggering
uint32_t m_silence_start_ms = 0;         // When silence began
bool m_silence_triggered = false;        // Has hysteresis elapsed?
float m_silent_scale_smoothed = 1.0f;    // Current brightness scale

void detectSilence(const AudioTime& now) {
    if (m_silence_hysteresis_ms <= 0.0f) {
        // Disabled
        m_frame.silentScale = 1.0f;
        m_frame.isSilent = false;
        return;
    }

    uint32_t now_ms = now.monotonic_us / 1000;
    bool currently_silent = (m_frame.fast_rms < m_silence_threshold);

    if (currently_silent && !m_silence_triggered) {
        // Start counting silence duration
        if (m_silence_start_ms == 0) {
            m_silence_start_ms = now_ms;
        }
        // Check if hysteresis has elapsed
        if ((now_ms - m_silence_start_ms) >= static_cast<uint32_t>(m_silence_hysteresis_ms)) {
            m_silence_triggered = true;
        }
    } else if (!currently_silent) {
        // Reset on any audio
        m_silence_start_ms = 0;
        m_silence_triggered = false;
    }

    // Smooth transition to silence (90% averaging = ~400ms fade at 60fps)
    float target = m_silence_triggered ? 0.0f : 1.0f;
    m_silent_scale_smoothed = target * 0.1f + m_silent_scale_smoothed * 0.9f;

    m_frame.silentScale = m_silent_scale_smoothed;
    m_frame.isSilent = m_silence_triggered;
}
```

---

## 4. Value Ranges & Clamping

| Output Type | Range | Clamping Method | Notes |
|------------|-------|-----------------|-------|
| `rms`, `fast_rms` | 0.0 - 1.0 | `clamp01()` | Loudness metric |
| `flux`, `fast_flux` | 0.0 - 1.0 | `clamp01()` | Onset/novelty |
| `bands[0-7]` | 0.0 - 1.0 | `clamp01()` | Normal smoothed |
| `heavy_bands[0-7]` | 0.0 - 1.0 | `clamp01()` | Extra smoothed |
| `chroma[0-11]` | 0.0 - 1.0 | `clamp01()` | Pitch classes |
| `heavy_chroma[0-11]` | 0.0 - 1.0 | `clamp01()` | Extra smoothed |
| `bins64[0-63]` | 0.0 - 1.0 | `clamp01()` | Full spectrum |
| `waveform[0-127]` | -32768 to 32767 | None | Raw int16 |
| `beatPhase` | 0.0 - 1.0 | Wraps per beat | 0=beat, 0.5=offbeat |
| `beatStrength` | 0.0 - 1.0 | Always bounded | Beat intensity |
| `saliency.*` | 0.0 - 1.0 | `clamp01()` | All metrics |
| `chordConfidence` | 0.0 - 1.0 | Ratio-based | ≥0.3 for valid |
| `styleConfidence` | 0.0 - 1.0 | Weighted max | Style certainty |
| `silentScale` | 0.0 - 1.0 | Smooth fade | Multiply brightness |

---

## 5. Timing Parameters

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           TIMING DIAGRAM                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Sample Rate:           16,000 Hz (16 kHz)                                   │
│                                                                              │
│  Hop Size:              256 samples                                          │
│  Hop Duration:          16 ms (256 / 16000)                                  │
│  Hop Frequency:         62.5 Hz (1000 / 16)                                  │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│  │ Hop 1 │ Hop 2 │ Hop 3 │ Hop 4 │ Hop 5 │ ...                              │
│  ───────────────────────────────────────────────────────────────────────    │
│  0ms    16ms    32ms    48ms    64ms                                         │
│                                                                              │
│  Goertzel Analysis:     Every 2 hops (~32 ms)                                │
│  Goertzel Window:       64-2000 samples (frequency-adaptive)                 │
│    - Low freq (55 Hz):  2000 samples (125 ms @ 16kHz)                        │
│    - High freq (2 kHz): ~64 samples (4 ms)                                   │
│                                                                              │
│  Spike Detection:       3-frame buffer, 32 ms output delay                   │
│                                                                              │
│  Render Rate:           120 FPS (8.33 ms per frame)                          │
│  Audio → Visual Ratio:  ~1.9 audio hops per render frame                     │
│                                                                              │
│  Beat Tracking Update:  Every hop (16 ms tick)                               │
│  Silence Hysteresis:    5000 ms default (configurable)                       │
│  Silence Fade:          ~400 ms (90% averaging @ 60 FPS)                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

| Parameter | Value | Derivation |
|-----------|-------|------------|
| Sample rate | 16,000 Hz | I2S configuration |
| Hop size | 256 samples | Audio processing chunk |
| Hop duration | 16 ms | 256 / 16000 |
| Hop frequency | 62.5 Hz | 1000 / 16 |
| Goertzel trigger | Every 2 hops | 32 ms analysis interval |
| Goertzel min window | 64 samples | Highest frequency bin |
| Goertzel max window | 2000 samples | Lowest frequency bin (capped) |
| Spike detection delay | 32 ms | 2 frames at 62.5 Hz |
| Render rate | 120 FPS | RendererActor target |
| Frame time | 8.33 ms | 1000 / 120 |
| Noise calibration | 3000 ms | Default startup period |
| Silence threshold | 0.01 | RMS below = silent |
| Silence hysteresis | 5000 ms | Time before fade starts |
| Silence fade duration | ~400 ms | 90% averaging decay |

---

## 6. Heavy Bands vs Regular Bands

Both variants receive **identical processing** (spike detection, AGC, normalization). Only the final attack/release smoothing differs:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SMOOTHING COMPARISON                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Input                                                                       │
│    │                                                                         │
│    ├──► bands[] (Normal Smoothing)                                          │
│    │      Attack:  0.15 (15% per hop) → ~64 ms to target                    │
│    │      Release: 0.03 (3% per hop)  → ~528 ms to target                   │
│    │      Use: Responsive effects (beat sync, flash triggers)               │
│    │                                                                         │
│    └──► heavy_bands[] (Heavy Smoothing)                                     │
│           Attack:  0.08 (8% per hop)  → ~144 ms to target                   │
│           Release: 0.015 (1.5% per hop) → ~1056 ms to target                │
│           Use: Ambient effects (speed modulation, slow transitions)         │
│                                                                              │
│  Time to 63% of target (time constant):                                      │
│    Normal attack:  τ ≈ -1/ln(1-0.15) ≈ 4 hops ≈ 64 ms                       │
│    Normal release: τ ≈ -1/ln(1-0.03) ≈ 33 hops ≈ 528 ms                     │
│    Heavy attack:   τ ≈ -1/ln(1-0.08) ≈ 9 hops ≈ 144 ms                      │
│    Heavy release:  τ ≈ -1/ln(1-0.015) ≈ 66 hops ≈ 1056 ms                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### When to Use Each

| Variant | Use Case | Example Effects |
|---------|----------|-----------------|
| `bands[]` | Beat-synced visuals, flash triggers, percussion response | RippleEffect, BPMEffect |
| `heavy_bands[]` | Speed modulation, ambient movement, slow breathing | ChevronWaves, InterferenceScanner, PhotonicCrystal |

### Access Pattern (PROVEN)

```cpp
// CORRECT: Use heavy_bands for speed modulation
float heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                     ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
float targetSpeed = 0.6f + 0.8f * heavyEnergy;
speedMult = m_speedSpring.update(targetSpeed, dt);

// WRONG: Using regular bands for speed causes jitter
float energy = (ctx.audio.controlBus.bands[1] + ctx.audio.controlBus.bands[2]) / 2.0f;
// This is too responsive and creates jerky motion!
```

---

## 7. Effect Context Access Methods

All audio data is accessed through `ctx.audio.*` in effect `render()` functions:

```cpp
// Source: v2/src/plugins/api/EffectContext.h (lines 120-250)

struct AudioContext {
    audio::ControlBusFrame controlBus;      // Complete snapshot
    audio::MusicalGridSnapshot musicalGrid; // Beat tracking
    bool available = false;                  // True if <100ms old

    // ===========================================
    // ENERGY & LOUDNESS
    // ===========================================
    float rms() const { return controlBus.rms; }
    float fastRms() const { return controlBus.fast_rms; }

    // ===========================================
    // SPECTRAL FLUX (ONSET DETECTION)
    // ===========================================
    float flux() const { return controlBus.flux; }
    float fastFlux() const { return controlBus.fast_flux; }

    // ===========================================
    // 8-BAND FREQUENCY DECOMPOSITION
    // ===========================================
    float getBand(uint8_t i) const {
        return (i < 8) ? controlBus.bands[i] : 0.0f;
    }
    float getHeavyBand(uint8_t i) const {
        return (i < 8) ? controlBus.heavy_bands[i] : 0.0f;
    }

    // Convenience aggregates
    float bass() const {
        return (controlBus.bands[0] + controlBus.bands[1]) * 0.5f;
    }
    float heavyBass() const {
        return (controlBus.heavy_bands[0] + controlBus.heavy_bands[1]) * 0.5f;
    }
    float mid() const {
        return (controlBus.bands[2] + controlBus.bands[3] + controlBus.bands[4]) / 3.0f;
    }
    float heavyMid() const {
        return (controlBus.heavy_bands[2] + controlBus.heavy_bands[3] +
                controlBus.heavy_bands[4]) / 3.0f;
    }
    float treble() const {
        return (controlBus.bands[5] + controlBus.bands[6] + controlBus.bands[7]) / 3.0f;
    }

    // ===========================================
    // 64-BIN FULL SPECTRUM
    // ===========================================
    float bin(uint8_t index) const {
        return (index < 64) ? controlBus.bins64[index] : 0.0f;
    }

    // ===========================================
    // BEAT TRACKING (K1 + MusicalGrid)
    // ===========================================
    float beatPhase() const { return musicalGrid.beat_phase01; }  // 0-1, wraps per beat
    bool isOnBeat() const { return musicalGrid.beat_tick; }       // Single-frame pulse
    float beatStrength() const { return musicalGrid.beat_strength; }
    float bpm() const { return musicalGrid.bpm; }
    float tempoConfidence() const { return musicalGrid.tempo_confidence; }

    // ===========================================
    // CHORD DETECTION
    // ===========================================
    audio::ChordType chordType() const { return controlBus.chordState.type; }
    uint8_t rootNote() const { return controlBus.chordState.rootNote; }
    float chordConfidence() const { return controlBus.chordState.confidence; }
    bool isMajor() const { return chordType() == audio::ChordType::MAJOR; }
    bool isMinor() const { return chordType() == audio::ChordType::MINOR; }
    bool isDiminished() const { return chordType() == audio::ChordType::DIMINISHED; }
    bool isAugmented() const { return chordType() == audio::ChordType::AUGMENTED; }

    // ===========================================
    // PERCUSSION ONSET
    // ===========================================
    float snare() const { return controlBus.snareEnergy; }
    float hihat() const { return controlBus.hihatEnergy; }
    bool isSnareHit() const { return controlBus.snareTrigger; }
    bool isHihatHit() const { return controlBus.hihatTrigger; }

    // ===========================================
    // MUSICAL SALIENCY
    // ===========================================
    float overallSaliency() const { return controlBus.saliency.overallSaliency; }
    float harmonicSaliency() const { return controlBus.saliency.harmonicNoveltySmooth; }
    float rhythmicSaliency() const { return controlBus.saliency.rhythmicNoveltySmooth; }
    float timbralSaliency() const { return controlBus.saliency.timbralNoveltySmooth; }
    float dynamicSaliency() const { return controlBus.saliency.dynamicNoveltySmooth; }

    bool isHarmonicDominant() const {
        return controlBus.saliency.dominantType ==
               static_cast<uint8_t>(audio::SaliencyType::HARMONIC);
    }
    bool isRhythmicDominant() const {
        return controlBus.saliency.dominantType ==
               static_cast<uint8_t>(audio::SaliencyType::RHYTHMIC);
    }
    bool isTimbralDominant() const {
        return controlBus.saliency.dominantType ==
               static_cast<uint8_t>(audio::SaliencyType::TIMBRAL);
    }
    bool isDynamicDominant() const {
        return controlBus.saliency.dominantType ==
               static_cast<uint8_t>(audio::SaliencyType::DYNAMIC);
    }

    // ===========================================
    // STYLE DETECTION
    // ===========================================
    audio::MusicStyle musicStyle() const { return controlBus.currentStyle; }
    float styleConfidence() const { return controlBus.styleConfidence; }

    // ===========================================
    // SILENCE & AVAILABILITY
    // ===========================================
    float silentScale() const { return controlBus.silentScale; }
    bool isSilent() const { return controlBus.isSilent; }
};
```

---

## 8. Audio Presets & Tuning

```cpp
// Source: v2/src/audio/AudioTuning.h (lines 20-80)

enum class AudioPreset : uint8_t {
    LIGHTWAVE_V2 = 0,       // Current defaults (4:1 AGC ratio)
    SENSORY_BRIDGE = 1,     // Sensory Bridge style (50:1 AGC ratio)
    AGGRESSIVE_AGC = 2,     // High compression
    CONSERVATIVE_AGC = 3,   // Low compression
    LGP_SMOOTH = 4,         // Optimized for Light Guide Plate
    CUSTOM = 255
};
```

| Preset | AGC Attack | AGC Release | Band Attack | Band Release | Use Case |
|--------|------------|-------------|-------------|--------------|----------|
| LIGHTWAVE_V2 | 0.08 | 0.02 | 0.15 | 0.03 | Default balanced |
| SENSORY_BRIDGE | 0.25 | 0.005 | 0.15 | 0.03 | High compression |
| LGP_SMOOTH | 0.06 | 0.015 | 0.12 | 0.025 | Optical interference |

### Mood Parameter (0-255)

Maps to smoothing parameters for user-adjustable responsiveness:

```cpp
// mood = 0:   Reactive (fast attack, very slow release)
// mood = 128: Balanced (medium attack, medium release)
// mood = 255: Smooth (slow attack, faster release)

void setMoodSmoothing(uint8_t mood) {
    float moodNorm = mood / 255.0f;  // 0.0 to 1.0

    m_alpha_fast = 0.25f + 0.20f * moodNorm;        // 0.25 to 0.45
    m_alpha_slow = 0.08f + 0.10f * moodNorm;        // 0.08 to 0.18
    m_band_attack = 0.25f - 0.17f * moodNorm;       // 0.25 to 0.08 (inverted!)
    m_band_release = 0.02f + 0.04f * moodNorm;      // 0.02 to 0.06
    m_heavy_band_attack = 0.12f - 0.08f * moodNorm; // 0.12 to 0.04 (inverted)
    m_heavy_band_release = 0.01f + 0.02f * moodNorm;// 0.01 to 0.03
}
```

---

## 9. Critical Constants Reference

| Constant | Value | Location | Purpose |
|----------|-------|----------|---------|
| `SAMPLE_RATE` | 16000 | AudioActor.h | I2S sample rate |
| `HOP_SIZE` | 256 | AudioActor.h | Samples per analysis hop |
| `CONTROLBUS_NUM_BANDS` | 8 | ControlBus.h | Standard frequency bands |
| `CONTROLBUS_NUM_CHROMA` | 12 | ControlBus.h | Pitch classes (C-B) |
| `BINS_64_COUNT` | 64 | ControlBus.h | Full Goertzel spectrum |
| `CONTROLBUS_WAVEFORM_N` | 128 | ControlBus.h | Time-domain samples |
| `LOOKAHEAD_FRAMES` | 3 | ControlBus.h | Spike detection buffer |
| `CONTROLBUS_NUM_ZONES` | 4 | ControlBus.h | AGC frequency zones |
| `GOERTZEL_MAX_BLOCK` | 2000 | AudioActor.h | Max analysis window |
| `GOERTZEL_MIN_BLOCK` | 64 | AudioActor.h | Min analysis window |
| `AGC_MIN_FLOOR` | 0.01f | ControlBus.cpp | Max 100x gain limit |
| `SILENCE_THRESHOLD` | 0.01f | ControlBus.cpp | RMS silence cutoff |
| `SILENCE_HYSTERESIS_MS` | 5000 | ControlBus.cpp | Default silence delay |
| `SPIKE_THRESHOLD` | 15% | ControlBus.cpp | Deviation trigger |
| `SPIKE_FLOOR` | 0.02f | ControlBus.cpp | Minimum threshold |

---

## See Also

- [VISUAL_PIPELINE_MECHANICS.md](VISUAL_PIPELINE_MECHANICS.md) - How effects consume audio data
- [IMPLEMENTATION_PATTERNS.md](IMPLEMENTATION_PATTERNS.md) - Proven code patterns
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common audio issues and fixes
