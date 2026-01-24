# LightwaveOS v2 Audio System Architecture

**Version:** 1.0
**Last Updated:** 2026-01-17
**Status:** Reference Documentation

This document provides a comprehensive blueprint of the audio DSP pipeline, showing who feeds what, who consumes what, and how data flows across the dual-core ESP32-S3.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [System Overview](#2-system-overview)
3. [Data Flow Pipeline](#3-data-flow-pipeline)
4. [Component Reference](#4-component-reference)
5. [Contract Structs](#5-contract-structs)
6. [Thread Safety Model](#6-thread-safety-model)
7. [Timing Constants](#7-timing-constants)
8. [Effect Consumer Interface](#8-effect-consumer-interface)
9. [Tuning Reference](#9-tuning-reference)
10. [Quick Reference Tables](#10-quick-reference-tables)

---

## 1. Executive Summary

**30-Second Mental Model:**

```
Microphone → AudioCapture → AudioActor → ControlBus → SnapshotBuffer → Effects
   (I2S)       (Core 0)      (Core 0)     (Core 0)    (lock-free)    (Core 1)
```

- **Input**: SPH0645 MEMS microphone via I2S at **12.8 kHz**
- **Processing**: Core 0 runs DSP at **50 Hz** (20ms hops of 256 samples)
- **Output**: Core 1 renders effects at **120 FPS** using published audio data
- **Cross-core**: Lock-free `SnapshotBuffer` with atomic double-buffering (no mutexes)

**Key Design Principles:**
1. **BY-VALUE semantics** - All cross-thread data is copied, never referenced
2. **Asymmetric smoothing** - Fast attack for transients, slow release for visual appeal
3. **Perceptual weighting** - Bass weighted higher for kick detection
4. **Musical intelligence** - Beyond raw spectrum: saliency, style, beat phase

---

## 2. System Overview

### 2.1 Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            HARDWARE LAYER                                    │
│  ┌─────────────────────┐                                                    │
│  │ SPH0645 MEMS Mic    │ ─── I2S DMA ─────→ 256 samples @ 12.8 kHz         │
│  │ (18-bit audio)      │     (20ms hops)                                    │
│  └─────────────────────┘                                                    │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CORE 0: AUDIO THREAD                                 │
│                         (FreeRTOS Priority 4)                               │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      AudioCapture                                    │   │
│  │  • I2S read (256 samples)                                           │   │
│  │  • DC-blocking IIR filter                                           │   │
│  │  • Bit-shift alignment (>>10)                                       │   │
│  │  • Normalize to [-1, 1] → int16_t                                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      AudioActor (Orchestrator)                       │   │
│  │  ┌──────────────┬──────────────┬──────────────┬──────────────┐      │   │
│  │  │ RMS + AGC    │ Goertzel     │ Goertzel     │ Chroma       │      │   │
│  │  │ Activity     │ 8-band       │ 64-bin       │ 12 pitch     │      │   │
│  │  │ Gating       │ (legacy)     │ (Phase 2)    │ classes      │      │   │
│  │  └──────────────┴──────────────┴──────────────┴──────────────┘      │   │
│  │                          │                                           │   │
│  │  ┌──────────────┬──────────────┬──────────────┬──────────────┐      │   │
│  │  │ Spectral     │ Tempo        │ Style        │ Saliency     │      │   │
│  │  │ Flux         │ Tracker      │ Detector     │ Computation  │      │   │
│  │  │ (novelty)    │ (beat/BPM)   │ (genre)      │ (MIS)        │      │   │
│  │  └──────────────┴──────────────┴──────────────┴──────────────┘      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      ControlBus (Smoothing)                          │   │
│  │  • Spike detection (3-frame lookahead)                              │   │
│  │  • Asymmetric attack/release filtering                              │   │
│  │  • Zone AGC (4 frequency zones)                                     │   │
│  │  • Chord detection                                                  │   │
│  │  • Silence detection (fade-to-black)                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
└────────────────────────────────────┼────────────────────────────────────────┘
                                     │
                    ┌────────────────┴────────────────┐
                    │     SnapshotBuffer<T>          │
                    │     (Lock-Free Double Buffer)   │
                    │     • Atomic index swap         │
                    │     • Memory fence              │
                    │     • BY-VALUE copy             │
                    └────────────────┬────────────────┘
                                     │
┌────────────────────────────────────┼────────────────────────────────────────┐
│                         CORE 1: RENDERER THREAD                             │
│                         (FreeRTOS Priority 5)                               │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      RendererActor (120 FPS)                         │   │
│  │  • ReadLatest() → ControlBusFrame (by-value)                        │   │
│  │  • MusicalGrid.Tick() → advance beat phase                          │   │
│  │  • AudioMappingRegistry.applyMappings()                             │   │
│  │  • effect->render(EffectContext)                                    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      LED Output                                      │   │
│  │  • FastLED + RMT driver                                             │   │
│  │  • 320 LEDs (2 × 160 WS2812)                                        │   │
│  │  • ~9.6ms frame time                                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 File Organization

```
firmware/v2/src/audio/
├── AudioActor.h/.cpp           # Pipeline orchestrator (Core 0)
├── AudioCapture.h/.cpp         # I2S input handling
├── AudioBehaviorSelector.h     # Narrative phase detection
├── AudioMappingRegistry.h      # Audio→visual parameter routing
├── AudioTuning.h               # Runtime configuration presets
├── ChromaAnalyzer.h            # 12 pitch class analysis
├── GoertzelAnalyzer.h/.cpp     # 8-band + 64-bin spectrum
├── StyleDetector.h             # Music genre classification
├── contracts/
│   ├── AudioTime.h             # Monotonic timing contract
│   ├── ControlBus.h/.cpp       # Main DSP output interface
│   ├── MusicalGrid.h/.cpp      # Beat/tempo PLL
│   ├── MusicalSaliency.h       # Perceptual importance
│   ├── SnapshotBuffer.h        # Lock-free cross-core buffer
│   └── AudioEffectMapping.h    # Parameter mapping definitions
└── tempo/
    └── TempoTracker.h/.cpp     # Beat detection algorithm
```

---

## 3. Data Flow Pipeline

### 3.1 Per-Hop Processing (Every 20ms)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ PHASE 1: CAPTURE (AudioCapture::captureHop)                             │
├─────────────────────────────────────────────────────────────────────────┤
│ I2S DMA read                                                            │
│    ↓                                                                    │
│ Raw 32-bit samples (SPH0645 18-bit MSB-aligned)                        │
│    ↓ [>>10 bit shift - legacy driver alignment]                        │
│ DC-blocking IIR filter (removes drift)                                  │
│    ↓ [Clamp to ±131072]                                                │
│ Normalize to [-1, 1] → scale to int16_t                                │
│    ↓                                                                    │
│ OUTPUT: m_hopBuffer[256] (int16_t)                                     │
└─────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ PHASE 2: DC REMOVAL + AGC (AudioActor::processHop)                      │
├─────────────────────────────────────────────────────────────────────────┤
│ m_hopBuffer[256]                                                        │
│    ↓ [DC estimate removal - IIR high-pass]                             │
│ AGC gain application (adaptive, 4:1 to 50:1 ratio)                     │
│    ↓ [Clip detection counter]                                          │
│ OUTPUT: m_hopBufferCentered[256] (gain-corrected)                      │
└─────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ PHASE 3: ENERGY ANALYSIS                                                │
├─────────────────────────────────────────────────────────────────────────┤
│ RMS Calculation                                                         │
│    → rmsRaw (0.0-1.0)                                                  │
│    → rmsMapped (dB-scaled, noise-floor gated)                          │
│    → activity (0.0-1.0, gates entire pipeline)                         │
│                                                                         │
│ Waveform Downsampling                                                   │
│    → raw.waveform[128] (peak of each 2-sample pair)                    │
└─────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ PHASE 4: SPECTRAL ANALYSIS (Parallel Paths)                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│ PATH A: Goertzel 8-Band (GoertzelAnalyzer)                             │
│    • Window: 512 samples (2 hops = 40ms latency)                       │
│    • Frequencies: 60, 120, 250, 500, 1000, 2000, 4000, 7800 Hz         │
│    • Output: raw.bands[8] (normalized, noise-gated)                    │
│                                                                         │
│ PATH B: Goertzel 64-Bin (Phase 2)                                      │
│    • Window: Adaptive 64-2000 samples per bin (~94ms max)              │
│    • Range: A1 (55 Hz) to C7 (2093 Hz), 1 semitone resolution         │
│    • Output: raw.bins64[64] (normalized)                               │
│    • Cache: m_bins64Cached[64] → TempoTracker input                    │
│                                                                         │
│ PATH C: Chromagram (ChromaAnalyzer)                                    │
│    • Window: 512 samples                                               │
│    • Analyzes: 48 frequencies (4 octaves × 12 notes)                   │
│    • Folds to: 12 pitch classes                                        │
│    • Output: raw.chroma[12]                                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ PHASE 5: NOVELTY & BEAT DETECTION                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│ Spectral Flux (Perceptually Weighted)                                  │
│    • Band weights: [1.4, 1.3, 1.0, 0.9, 0.8, 0.6, 0.4, 0.3]           │
│    • Half-wave rectification (positive changes only)                   │
│    • Output: raw.flux (0.0-1.0)                                        │
│                                                                         │
│ TempoTracker (Beat Detection)                                          │
│    • Input: bins64 (spectral novelty) + RMS (VU novelty)              │
│    • 96 tempo bins: 48-143 BPM                                         │
│    • Goertzel resonators per tempo                                     │
│    • Output: BPM, phase, confidence, beat_tick                         │
│                                                                         │
│ Chord Detection                                                         │
│    • Input: chroma[12]                                                 │
│    • Detect: root + 3rd + 5th intervals                                │
│    • Output: ChordState (root, type, confidence)                       │
│                                                                         │
│ Style Detection (if FEATURE_STYLE_DETECTION)                           │
│    • Input: rms, flux, bands, beatConfidence, chordChanged             │
│    • Output: MusicStyle (RHYTHMIC/HARMONIC/MELODIC/TEXTURE/DYNAMIC)    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ PHASE 6: SMOOTHING & PUBLICATION (ControlBus)                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│ ControlBusRawInput (unsmoothed)                                        │
│    ↓                                                                    │
│ Spike Detection (3-frame lookahead buffer)                             │
│    • Detects single-frame flicker                                      │
│    • Replaces spike with neighbor average                              │
│    • 2-frame delay (acceptable for visual)                             │
│    ↓                                                                    │
│ Asymmetric Attack/Release Smoothing                                    │
│    • bands: attack=0.15, release=0.03                                  │
│    • heavy_bands: attack=0.08, release=0.015                           │
│    ↓                                                                    │
│ Zone AGC (4 frequency zones)                                           │
│    • Zone 0: bands[0-1] (60-120 Hz)                                    │
│    • Zone 1: bands[2-3] (250-500 Hz)                                   │
│    • Zone 2: bands[4-5] (1-2 kHz)                                      │
│    • Zone 3: bands[6-7] (4-7.8 kHz)                                    │
│    • Prevents bass from drowning treble                                │
│    ↓                                                                    │
│ Musical Saliency Computation (if FEATURE_MUSICAL_SALIENCY)             │
│    • harmonicNovelty (chord changes)                                   │
│    • rhythmicNovelty (beat pattern changes)                            │
│    • timbralNovelty (spectral character changes)                       │
│    • dynamicNovelty (loudness envelope changes)                        │
│    ↓                                                                    │
│ Silence Detection                                                       │
│    • Threshold: 0.01 RMS                                               │
│    • Hysteresis: 3000ms fade                                           │
│    • Output: silentScale (1.0 → 0.0)                                   │
│    ↓                                                                    │
│ OUTPUT: ControlBusFrame (published via SnapshotBuffer)                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Cross-Core Publication

```
CORE 0 (Writer)                           CORE 1 (Reader)
─────────────────                         ─────────────────

SnapshotBuffer::Publish(frame)
  │
  ├── Write to buffer[inactive_idx]
  │
  ├── Atomic swap: active_idx ^= 1
  │
  └── Memory fence (__sync_synchronize)
                                          SnapshotBuffer::ReadLatest(out)
                                            │
                                            ├── Read active_idx (atomic)
                                            │
                                            ├── Copy buffer[active_idx] → out
                                            │   (BY-VALUE, ~500 bytes)
                                            │
                                            └── Retry if sequence changed
```

---

## 4. Component Reference

### 4.1 AudioCapture

**Location:** `src/audio/AudioCapture.h/.cpp`

**Responsibility:** I2S DMA interface to SPH0645 MEMS microphone

**Configuration:**
| Parameter | Value |
| --- | --- |
| I2S Port | I2S_NUM_0 |
| Sample Rate | 12,800 Hz |
| Bit Depth | 32-bit slots (18-bit data) |
| DMA Buffers | 4 × 512 samples |
| GPIO BCLK | 14 |
| GPIO LRCLK | 12 |
| GPIO DIN | 13 |

**Key Functions:**
```cpp
bool begin();                    // Initialize I2S
void captureHop();               // Read 256 samples, process
int16_t* getHopBuffer();         // Access processed samples
```

**Processing Pipeline:**
1. Read 256 samples from I2S DMA
2. Bit-shift >>10 (ESP-IDF legacy driver alignment)
3. DC-blocking IIR filter: `y[n] = x[n] - x[n-1] + 0.995*y[n-1]`
4. Clamp to ±131072
5. Normalize to int16_t range

---

### 4.2 AudioActor

**Location:** `src/audio/AudioActor.h/.cpp`

**Responsibility:** Core 0 DSP pipeline orchestrator

**Task Configuration:**
| Parameter | Value |
| --- | --- |
| Core | 0 |
| Priority | 4 |
| Stack | 8192 words (32 KB) |
| Tick Interval | 16 ms |

**Key Functions:**
```cpp
void onTick();                              // Main processing loop
const SnapshotBuffer<ControlBusFrame>& getControlBusBuffer() const;
uint32_t ReadLatest(MusicalGridSnapshot& out) const;
AudioPipelineTuning getPipelineTuning() const;
void setPipelineTuning(const AudioPipelineTuning& tuning);
TempoTracker& getTempoMut();                // For renderer phase advance
```

**Internal State:**
- `m_hopBuffer[256]` - Raw captured samples
- `m_hopBufferCentered[256]` - DC-removed, gain-corrected
- `m_prevBands[8]` - Previous frame bands (for flux)
- `m_bins64Cached[64]` - 64-bin spectrum cache
- `m_dcEstimate` - DC offset tracker
- `m_agcGain` - Adaptive gain (4:1 to 50:1 ratio)

---

### 4.3 GoertzelAnalyzer

**Location:** `src/audio/GoertzelAnalyzer.h/.cpp`

**Responsibility:** Frequency magnitude analysis using Goertzel DFT

**Dual-Mode Operation:**

**Mode A: 8-Band Legacy**
| Band | Frequency | Purpose |
| --- | --- | --- |
| 0 | 60 Hz | Sub-bass, kick |
| 1 | 120 Hz | Bass notes |
| 2 | 250 Hz | Bass harmonics |
| 3 | 500 Hz | Lower vocals |
| 4 | 1000 Hz | Vocals, instruments |
| 5 | 2000 Hz | Clarity |
| 6 | 4000 Hz | Sibilance, hi-hats |
| 7 | 7800 Hz | Treble transients |

**Mode B: 64-Bin Semitone**
- Range: A1 (55 Hz) to C7 (2093 Hz)
- Resolution: 1 semitone per bin
- Adaptive window: 64-2000 samples based on frequency
- Hann windowing via Q15 LUT

**Key Algorithm:**
```cpp
// Goertzel coefficient
coeff = 2 * cos(2π * k / N)
where k = round(block_size * freq / sample_rate)

// IIR filter
q0 = coeff * q1 - q2 + sample
q2 = q1
q1 = q0

// Magnitude extraction
magnitude = sqrt(q1² + q2² - coeff * q1 * q2)
```

**bands[8] vs bins64[64] - When to Use Which:**

| Aspect | bands[8] | bins64[64] |
| --- | --- | --- |
| **Resolution** | 8 fixed frequencies | 64 semitones (musical) |
| **Range** | 60 Hz - 7.8 kHz | 55 Hz - 2093 Hz (A1-C7) |
| **Window** | 512 samples (40ms) | Adaptive 64-2000 samples |
| **Update Rate** | 25 Hz (every 2 hops) | ~10 Hz (interlaced) |
| **Latency** | 40 ms | Up to 94 ms (low bins) |
| **Primary Use** | Effects visual response | TempoTracker novelty input |
| **When to Use** | Frequency-reactive visuals | Beat detection, pitch analysis |
| **Access** | `ctx.audio.controlBus.bands[i]` | `ctx.audio.controlBus.bins64[i]` |

**Rule of thumb:** Use `bands[8]` for effects. Use `bins64[64]` only if you need semitone-level resolution (rare).

---

### 4.4 ChromaAnalyzer

**Location:** `src/audio/ChromaAnalyzer.h`

**Responsibility:** 12-bin pitch class energy (chromagram)

**Coverage:**
- 4 octaves: C2-B5 (65.4 Hz - 987.8 Hz)
- 48 note frequencies analyzed
- Folded to 12 pitch classes

**Output:** `float chroma[12]` - Energy per pitch class (C, C#, D, ..., B)

**Use Cases:**
- Chord detection (find root + 3rd + 5th)
- Harmonic saliency (track key/chord changes)
- Style detection (harmonic-driven genres)

---

### 4.5 TempoTracker

**Location:** `src/audio/tempo/TempoTracker.h/.cpp`

**Responsibility:** Beat detection and tempo estimation

**Configuration:**
| Parameter | Value |
| --- | --- |
| BPM Range | 48-143 BPM |
| Tempo Bins | 96 (1 BPM resolution) |
| Spectral History | 1024 samples (~20s) |
| VU History | 512 samples (~10s) |
| Novelty Rate | 50 Hz |
| Hysteresis | 200 ms (10 frames) |

**Dual-Input Architecture:**
1. **Spectral Novelty** - L2 norm of 64-bin deltas (when bins ready)
2. **VU Novelty** - RMS derivative (every hop)

**Key Functions:**
```cpp
void updateNovelty(const float* bins64, float rms, bool bins_ready);
void updateTempo(float delta_sec);  // Compute 2 Goertzel magnitudes
void advancePhase(float delta_sec); // Called by renderer at 120 FPS
TempoOutput getOutput() const;
```

**Output:**
```cpp
struct TempoOutput {
    float bpm;            // Detected tempo (48-143)
    float phase01;        // Phase within beat [0,1)
    float confidence;     // Lock confidence [0,1]
    bool beat_tick;       // True for ONE frame at beat boundary
    bool locked;          // True if confidence > threshold
};
```

**IMPORTANT: Three Tempo Systems - Which One Do Effects Use?**

```
┌─────────────────────────────────────────────────────────────────┐
│ TEMPO SYSTEM COMPARISON                                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ 1. TempoTracker (Core 0, 50 Hz)                                │
│    └─→ DETECTS BPM from novelty analysis                       │
│        Output: bpm, phase, confidence, locked                  │
│        Consumer: Feeds ControlBus + MusicalGrid                │
│                                                                 │
│ 2. ControlBus.tempo* fields (Core 0, 50 Hz)                    │
│    └─→ PASSTHROUGH for saliency computation ONLY               │
│        Output: tempoLocked, tempoConfidence, tempoBeatTick     │
│        Consumer: Saliency computation (NOT for effects!)       │
│                                                                 │
│ 3. MusicalGrid (Core 1, 120 Hz) ← EFFECTS USE THIS ONE         │
│    └─→ PLL that INTERPOLATES beat phase at render rate         │
│        Output: beat_tick, beat_phase01, bpm_smoothed           │
│        Consumer: Effects via ctx.audio.musicalGrid             │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ Timeline:                                                       │
│   Core 0: ──●─────●─────●─────●───  (TempoTracker @ 50Hz)      │
│              │     │     │     │                                │
│              └─────┴─────┴─────┴──→ SnapshotBuffer              │
│                                         │                       │
│   Core 1: ─●●●●●●●●●●●●●●●●●●●●●●─  (MusicalGrid @ 120Hz)      │
│             ↑           ↑           ↑  beat_tick fires         │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ RULE: Effects should ONLY use ctx.audio.musicalGrid            │
│       Never read ControlBus.tempo* directly!                   │
└─────────────────────────────────────────────────────────────────┘
```

---

### 4.6 StyleDetector

**Location:** `src/audio/StyleDetector.h`

**Responsibility:** Music genre/style classification

**Feature Flag:** `FEATURE_STYLE_DETECTION` (~60 µs/hop)

**Styles:**
| Style | Characteristics |
| --- | --- |
| RHYTHMIC_DRIVEN | Strong beat, high bass ratio (EDM, hip-hop) |
| HARMONIC_DRIVEN | Frequent chord changes (jazz, classical) |
| MELODIC_DRIVEN | High treble ratio, vocal presence |
| TEXTURE_DRIVEN | High flux variance (ambient, drone) |
| DYNAMIC_DRIVEN | Large dynamic range (orchestral) |

**Tuning:**
- Analysis window: 250 hops (~4 seconds)
- Hysteresis threshold: 0.15 (prevents jitter)
- Style alpha: 0.05 (slow adaptation EMA)

---

### 4.7 ControlBus

**Location:** `src/audio/contracts/ControlBus.h/.cpp`

**Responsibility:** Smoothing, spike removal, and publication

**Processing Stages:**
1. **Spike Detection** - 3-frame lookahead buffer
2. **Asymmetric Smoothing** - Per-band attack/release
3. **Zone AGC** - 4 frequency zones normalized independently
4. **Saliency Computation** - Musical importance metrics
5. **Silence Detection** - Fade-to-black after sustained silence

**Key Functions:**
```cpp
void UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw);
ControlBusFrame GetFrame() const;
void setMoodSmoothing(uint8_t mood);        // 0=reactive, 255=smooth
void setLookaheadEnabled(bool enabled);
void setZoneAGCEnabled(bool enabled);
void setSilenceParameters(float threshold, float hysteresisMs);
```

---

## 5. Contract Structs

### 5.1 AudioTime

**Location:** `src/audio/contracts/AudioTime.h`

```cpp
struct AudioTime {
    uint64_t sample_index;      // Monotonic ADC sample count since boot
    uint32_t sample_rate_hz;    // Sample rate (12800)
    uint64_t monotonic_us;      // Microsecond timestamp (esp_timer_get_time)
};

// Helpers
int64_t AudioTime_SamplesBetween(const AudioTime& a, const AudioTime& b);
float AudioTime_SecondsBetween(const AudioTime& a, const AudioTime& b);
```

**Purpose:** Thread-safe time reference for audio synchronization.

---

### 5.2 ControlBusRawInput

**Location:** `src/audio/contracts/ControlBus.h`

```cpp
struct ControlBusRawInput {
    // Energy
    float rms;                  // 0..1 overall energy
    float flux;                 // 0..1 spectral novelty

    // Frequency bands
    float bands[8];             // 0..1 per-band energy
    float chroma[12];           // 0..1 pitch class energy

    // Time domain
    int16_t waveform[128];      // Downsampled waveform

    // Onset detection
    float snareEnergy;          // 150-300 Hz
    float hihatEnergy;          // 6-12 kHz
    bool snareTrigger;
    bool hihatTrigger;

    // 64-bin spectrum (Phase 2)
    float bins64[64];           // Full Goertzel spectrum

    // Tempo tracker state (for saliency computation)
    bool tempoLocked;           // TempoTracker lock state
    float tempoConfidence;      // TempoTracker confidence
    bool tempoBeatTick;         // Beat tick gated by lock
};
```

---

### 5.3 ControlBusFrame

**Location:** `src/audio/contracts/ControlBus.h`

```cpp
struct ControlBusFrame {
    AudioTime t;
    uint32_t hop_seq;

    // Smoothed energy (fast + slow)
    float rms, flux;
    float fast_rms, fast_flux;

    // Frequency bands (regular + heavy-smoothed)
    float bands[8], heavy_bands[8];
    float chroma[12], heavy_chroma[12];

    // Time domain
    int16_t waveform[128];

    // Onset triggers
    float snareEnergy, hihatEnergy;
    bool snareTrigger, hihatTrigger;

    // 64-bin spectrum
    float bins64[64];

    // Musical analysis
    ChordState chordState;           // Detected chord
    MusicalSaliencyFrame saliency;   // Perceptual importance
    MusicStyle currentStyle;         // Genre classification
    float styleConfidence;

    // Tempo state
    bool tempoLocked;
    float tempoConfidence;
    bool tempoBeatTick;

    // Silence detection
    float silentScale;               // 1.0=active, 0.0=silent
    bool isSilent;
};
```

---

### 5.4 MusicalGridSnapshot

**Location:** `src/audio/contracts/MusicalGrid.h`

```cpp
struct MusicalGridSnapshot {
    AudioTime t;

    float bpm_smoothed;         // Current tempo estimate
    float tempo_confidence;     // 0..1

    float beat_phase01;         // 0..1 position within beat
    float bar_phase01;          // 0..1 position within bar

    bool beat_tick;             // True on beat boundary
    bool downbeat_tick;         // True on bar boundary

    uint64_t beat_index;        // Monotonic beat count
    uint64_t bar_index;         // Monotonic bar count

    uint8_t beat_in_bar;        // Which beat (0..beats_per_bar-1)
    float beat_strength;        // 0..1 last beat strength, decays
};
```

**Purpose:** Renderer-domain PLL for beat-synchronized effects.

---

### 5.5 MusicalSaliencyFrame

**Location:** `src/audio/contracts/MusicalSaliency.h`

```cpp
struct MusicalSaliencyFrame {
    // Novelty metrics (0..1, higher = important change)
    float harmonicNovelty;      // Chord/key changes (slow: 500ms-5s)
    float rhythmicNovelty;      // Beat pattern changes (reactive: 100-300ms)
    float timbralNovelty;       // Spectral character (sustained: 300ms-2s)
    float dynamicNovelty;       // Loudness envelope (reactive: 100-300ms)

    // Derived
    float overallSaliency;      // Weighted combination
    uint8_t dominantType;       // Which novelty is strongest

    // Smoothed variants
    float harmonicNoveltySmooth;
    float rhythmicNoveltySmooth;
    float timbralNoveltySmooth;
    float dynamicNoveltySmooth;

    // Query
    SaliencyType getDominantType() const;
    bool isSalient(SaliencyType type, float threshold = 0.3f) const;
    float getNovelty(SaliencyType type) const;
};

enum class SaliencyType { HARMONIC, RHYTHMIC, TIMBRAL, DYNAMIC };
```

**Purpose:** Respond to "what's important" not raw signals.

---

### 5.6 ChordState

**Location:** `src/audio/contracts/ControlBus.h`

```cpp
enum class ChordType { NONE, MAJOR, MINOR, DIMINISHED, AUGMENTED };

struct ChordState {
    uint8_t rootNote;           // 0-11 (C=0, B=11)
    ChordType type;
    float confidence;           // 0..1
    float rootStrength;
    float thirdStrength;
    float fifthStrength;
};
```

---

### 5.7 AudioParameterMapping

**Location:** `src/audio/contracts/AudioEffectMapping.h`

```cpp
enum class AudioSource : uint8_t {
    RMS, FAST_RMS, FLUX, FAST_FLUX,
    BAND_0, BAND_1, BAND_2, BAND_3, BAND_4, BAND_5, BAND_6, BAND_7,
    BASS, MID, TREBLE, HEAVY_BASS,
    BEAT_PHASE, BPM, TEMPO_CONFIDENCE
};

enum class VisualTarget : uint8_t {
    BRIGHTNESS, SPEED, INTENSITY, SATURATION, COMPLEXITY, VARIATION, HUE
};

enum class MappingCurve { LINEAR, SQUARED, SQRT, LOG, EXP, INVERTED };

struct AudioParameterMapping {
    AudioSource source;
    VisualTarget target;
    MappingCurve curve;
    float inputMin, inputMax;
    float outputMin, outputMax;
    float smoothingAlpha;
    float gain;
    bool enabled, additive;

    float apply(float rawInput) const;
};
```

**Purpose:** Route audio features to visual parameters.

---

## 6. Thread Safety Model

### 6.1 Ownership Model

| Component | Owner Thread | Readers | Pattern |
| --- | --- | --- | --- |
| AudioCapture | Audio (Core 0) | None | Single-threaded |
| AudioActor | Audio (Core 0) | None | Single-threaded |
| ControlBus | Audio (Core 0) | None | Single-threaded |
| SnapshotBuffer | Audio (write) | Renderer (read) | Lock-free double buffer |
| MusicalGrid | Renderer (Core 1) | Effects | Single-threaded |
| TempoTracker | Audio (Core 0) | Renderer (advancePhase only) | Partitioned access |

### 6.2 SnapshotBuffer Pattern

```cpp
template <typename T>
class SnapshotBuffer {
private:
    T m_buffer[2];
    std::atomic<uint32_t> m_sequence{0};
    std::atomic<uint8_t> m_activeIdx{0};

public:
    void Publish(const T& v) {
        uint8_t writeIdx = m_activeIdx.load() ^ 1;
        m_buffer[writeIdx] = v;              // Write to inactive
        m_sequence.fetch_add(1);             // Increment sequence (odd = writing)
        m_activeIdx.store(writeIdx);         // Swap active index
        m_sequence.fetch_add(1);             // Increment again (even = stable)
        __sync_synchronize();                // Memory fence
    }

    uint32_t ReadLatest(T& out) const {
        uint32_t seq1, seq2;
        do {
            seq1 = m_sequence.load();
            out = m_buffer[m_activeIdx.load()];  // BY-VALUE copy
            seq2 = m_sequence.load();
        } while (seq1 != seq2 || (seq1 & 1));    // Retry if sequence changed
        return seq2;
    }
};
```

**Guarantees:**
- No mutexes in critical path
- Reader never blocks writer
- Writer never blocks reader
- At most 1 retry on read collision
- ~500 byte copy per read (acceptable at 120 FPS)

### 6.3 Cross-Core Data Flow

```
Core 0 (Audio Thread)                    Core 1 (Renderer Thread)
─────────────────────                    ───────────────────────

[Every 16ms]                             [Every 8.33ms]
AudioActor::onTick()                     RendererActor::renderFrame()
  │                                        │
  ├── captureHop()                         ├── m_controlBusBuffer->ReadLatest()
  ├── processHop()                         │     → ControlBusFrame (by-value)
  │     └── ControlBus::UpdateFromHop()    │
  │                                        ├── m_musicalGrid.Tick()
  └── m_controlBusBuffer.Publish(frame)    │     → Advance beat phase
        │                                  │
        └───────── SNAPSHOT ──────────────→├── AudioMappingRegistry::applyMappings()
              (lock-free)                  │
                                           └── effect->render(ctx)
```

---

## 7. Timing Constants

### 7.1 Audio Pipeline

| Constant | Value | Source | Purpose |
| --- | --- | --- | --- |
| `SAMPLE_RATE` | 12,800 Hz | I2S config | ADC sampling rate |
| `HOP_SIZE` | 256 samples | Audio config | Samples per hop |
| `HOP_DURATION_MS` | 20 ms | Derived | Time per hop |
| `HOP_RATE_HZ` | 50 Hz | Derived | Hops per second |
| `GOERTZEL_WINDOW` | 512 samples | GoertzelAnalyzer | 8-band analysis window |
| `GOERTZEL_64_MAX` | 2000 samples | GoertzelAnalyzer | 64-bin max history |

### 7.2 Tempo Tracking

| Constant | Value | Source | Purpose |
| --- | --- | --- | --- |
| `TEMPO_LOW` | 48 BPM | TempoTracker.h | Minimum detectable tempo |
| `TEMPO_HIGH` | 143 BPM | TempoTracker.h | Maximum detectable tempo |
| `NUM_TEMPI` | 96 | Derived | 1 BPM resolution bins |
| `SPECTRAL_LOG_HZ` | 50 Hz | TempoTracker.h | Novelty computation rate |
| `SPECTRAL_HISTORY_LENGTH` | 1024 | TempoTracker.h | ~20 seconds history |
| `HYSTERESIS_TIME_MS` | 200 ms | TempoTracker.h | Winner stability time |
| `HYSTERESIS_FRAMES` | 10 | Derived | Frames at 50 Hz |

### 7.3 Rendering

| Constant | Value | Source | Purpose |
| --- | --- | --- | --- |
| `RENDER_RATE` | 120 FPS | RendererActor | LED update rate |
| `FRAME_TIME_US` | ~8,333 µs | Derived | Time per frame |
| `STALENESS_THRESHOLD_MS` | 100 ms | RendererActor | Audio data freshness |

### 7.4 Smoothing

| Parameter | Attack | Release | Purpose |
| --- | --- | --- | --- |
| bands | 0.15 | 0.03 | Standard response |
| heavy_bands | 0.08 | 0.015 | Extra-smooth (ambient) |
| chroma | 0.15 | 0.03 | Pitch class response |
| heavy_chroma | 0.08 | 0.015 | Extra-smooth pitch |

---

## 8. Effect Consumer Interface

### 8.1 EffectContext Access

Effects receive audio data via `EffectContext`:

```cpp
void MyEffect::render(plugins::EffectContext& ctx) {
    // Check if audio data is available
    if (!ctx.audio.available) {
        // Render without audio
        return;
    }

    const ControlBusFrame& bus = ctx.audio.controlBus;
    const MusicalGridSnapshot& grid = ctx.audio.musicalGrid;

    // ... use audio data
}
```

### 8.2 Common Access Patterns

**Energy/Dynamics:**
```cpp
float energy = ctx.audio.controlBus.rms;           // Overall energy
float transients = ctx.audio.controlBus.flux;      // Spectral novelty
float fastEnergy = ctx.audio.controlBus.fast_rms;  // Quick response
```

**Frequency Bands:**
```cpp
float bass = ctx.audio.controlBus.bands[0];        // 60 Hz
float midBass = ctx.audio.controlBus.bands[1];     // 120 Hz
float treble = ctx.audio.controlBus.bands[7];      // 7.8 kHz

// Aggregated
float totalBass = (bus.bands[0] + bus.bands[1]) * 0.5f;
float totalTreble = (bus.bands[6] + bus.bands[7]) * 0.5f;
```

**Beat Synchronization:**
```cpp
if (ctx.audio.musicalGrid.beat_tick) {
    // Flash on beat!
    triggerPulse();
}

float phase = ctx.audio.musicalGrid.beat_phase01;  // 0..1 within beat

// Simple flash on beat (sharp)
float flash = 1.0f - phase;  // 1→0 decay after beat

// Smooth pulsing (full sine wave, 0→1→0 per beat)
float pulse = (1.0f + sinf(phase * 2.0f * M_PI)) * 0.5f;

// Breathing effect (gentle swell)
float breath = 0.5f + 0.5f * sinf(phase * M_PI);  // 0.5→1→0.5
```

**Musical Saliency:**
```cpp
const auto& sal = ctx.audio.controlBus.saliency;

if (sal.isSalient(SaliencyType::RHYTHMIC)) {
    // Beat pattern changed - respond with motion
}

if (sal.isSalient(SaliencyType::HARMONIC)) {
    // Chord changed - shift color
}

// Overall importance
float importance = sal.overallSaliency;
```

**Style-Adaptive (Complete Switch):**
```cpp
MusicStyle style = ctx.audio.controlBus.currentStyle;
float conf = ctx.audio.controlBus.styleConfidence;

switch (style) {
    case MusicStyle::RHYTHMIC_DRIVEN:
        // EDM, hip-hop: strong beat sync, bass-reactive
        beatSyncIntensity = 1.0f;
        useHeavyBands = false;  // Want quick response
        break;

    case MusicStyle::HARMONIC_DRIVEN:
        // Jazz, classical: smooth color transitions on chord changes
        respondToChordChanges = true;
        useHeavyBands = true;   // Ignore transients
        break;

    case MusicStyle::MELODIC_DRIVEN:
        // Vocal pop: treble-reactive, follow melody contour
        primaryBand = 4;  // 1kHz vocal range
        useChroma = true;
        break;

    case MusicStyle::TEXTURE_DRIVEN:
        // Ambient, drone: gentle breathing, slow evolution
        useHeavyBands = true;
        breathingRate = 0.25f;  // Slow
        break;

    case MusicStyle::DYNAMIC_DRIVEN:
        // Orchestral: follow loudness envelope
        useRMS = true;
        dynamicRange = ctx.audio.controlBus.rms;
        break;

    default:
        // UNKNOWN: balanced fallback
        useHeavyBands = false;
        beatSyncIntensity = 0.5f;
        break;
}
```

**Silence Handling:**
```cpp
// Automatically fade to black during silence
uint8_t finalBrightness = brightness * ctx.audio.controlBus.silentScale;
```

**Chord Detection:**
```cpp
const ChordState& chord = ctx.audio.controlBus.chordState;

if (chord.type != ChordType::NONE) {
    uint8_t rootHue = chord.rootNote * 21;  // 12 notes → 256 hue range
    setBaseColor(CHSV(rootHue, 255, 255));
}
```

### 8.3 Heavy (Extra-Smoothed) Variants

Use `heavy_bands` and `heavy_chroma` for ambient effects that should ignore quick transients:

```cpp
// Standard bands - responsive to beats
float kickResponse = ctx.audio.controlBus.bands[0];

// Heavy bands - ignores quick transients
float ambientBass = ctx.audio.controlBus.heavy_bands[0];
```

---

## 9. Tuning Reference

### 9.1 AudioPipelineTuning Struct

```cpp
struct AudioPipelineTuning {
    // DC offset tracking
    float dcAlpha;                      // IIR coefficient

    // AGC (Automatic Gain Control)
    float agcTargetRms;                 // Target RMS level
    float agcMinGain, agcMaxGain;       // Gain limits
    float agcAttack, agcRelease;        // Rates

    // ControlBus smoothing
    float controlBusAlphaFast;          // Transient response
    float controlBusAlphaSlow;          // Sustained response

    // Per-band asymmetric
    float bandAttack, bandRelease;
    float heavyBandAttack, heavyBandRelease;

    // Per-band gain correction
    float perBandGains[8];              // EQ curve

    // Silence detection
    float silenceHysteresisMs;
    float silenceThreshold;
};
```

### 9.2 Preset Configurations

| Preset | AGC Ratio | Band Attack | Use Case |
| --- | --- | --- | --- |
| LIGHTWAVE_V2 | 4:1 | 0.15 | Default balanced |
| SENSORY_BRIDGE | 50:1 | 0.12 | Max compression |
| AGGRESSIVE_AGC | 100:1 | 0.20 | Very loud venues |
| CONSERVATIVE_AGC | 2:1 | 0.10 | Acoustic music |
| LGP_SMOOTH | 4:1 | 0.08 | Light Guide Plate |

### 9.3 Runtime Configuration

```cpp
// Get current tuning
AudioPipelineTuning tuning = audioActor.getPipelineTuning();

// Modify
tuning.agcMaxGain = 50.0f;  // Increase compression
tuning.bandRelease = 0.02f; // Slower decay

// Apply
audioActor.setPipelineTuning(tuning);

// Or use preset
audioActor.setPipelineTuning(getPreset(AudioPreset::SENSORY_BRIDGE));
```

---

## 10. Quick Reference Tables

### 10.1 Data Flow Summary

| Stage | Input | Output | Rate | Latency |
| --- | --- | --- | --- | --- |
| I2S Capture | Microphone | 256 samples | 50 Hz | ~0.5 ms |
| DC/AGC | Raw samples | Centered samples | 50 Hz | ~1 ms |
| Goertzel 8-band | 512 samples | 8 magnitudes | 25 Hz | 40 ms |
| Goertzel 64-bin | 2000 samples | 64 magnitudes | ~10 Hz | 94 ms |
| Chromagram | 512 samples | 12 pitch classes | 25 Hz | 40 ms |
| Tempo Tracker | Novelty history | BPM/phase | 50 Hz | ~2 s lock |
| ControlBus | Raw frame | Smoothed frame | 50 Hz | 2-3 frames |
| Render | Audio frame | LED output | 120 Hz | ~8 ms |

### 10.2 Memory Budget

| Component | Allocation | Notes |
| --- | --- | --- |
| AudioActor stack | 32 KB | FreeRTOS task |
| Goertzel history | 8 KB | 2000 × 4 bytes |
| TempoTracker | 22 KB | 96 bins + histories |
| ControlBus | 4 KB | Smoothing state |
| SnapshotBuffer | 1 KB | 2 × 500 bytes |
| **Total** | ~67 KB | Within ESP32-S3 budget |

### 10.3 Feature Flags

| Flag | Default | CPU Cost | Purpose |
| --- | --- | --- | --- |
| `FEATURE_AUDIO_SYNC` | On | Base | Enable audio pipeline |
| `FEATURE_AUDIO_OA` | On | +10% | Overlap-Add mode |
| `FEATURE_MUSICAL_SALIENCY` | Audio | ~80 µs/hop | Novelty computation |
| `FEATURE_STYLE_DETECTION` | Audio | ~60 µs/hop | Genre classification |
| `FEATURE_AUDIO_BENCHMARK` | Off | ~5% | MabuTrace instrumentation |

---

## Related Documentation

- [AUDIO_OUTPUT_SPECIFICATIONS.md](./AUDIO_OUTPUT_SPECIFICATIONS.md) - Detailed output format specifications
- [AUDIO_VISUAL_SEMANTIC_MAPPING.md](./AUDIO_VISUAL_SEMANTIC_MAPPING.md) - Effect design guidelines
- [planning/17-port-emotiscope-tempo/](../../planning/17-port-emotiscope-tempo/) - Tempo tracker implementation details

---

*Document generated 2026-01-17. Source: firmware/v2/src/audio/*
