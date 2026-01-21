# Audio-Reactive Effects Analysis

**Date:** 2025-12-29
**Version:** 1.0
**Status:** Complete Analysis

---

## Executive Summary

LightwaveOS v2 implements a sophisticated audio-reactive visualization system featuring:

- **64-bin Goertzel frequency analysis** (NOT FFT) with Sensory Bridge algorithm parity
- **Musical Intelligence System (MIS)** with chord detection, saliency analysis, and style detection
- **4 dedicated audio-reactive effects** plus several enhanced LGP effects with audio integration
- **Rich AudioContext API** with 30+ accessors for effect developers

**Key Finding:** The audio pipeline is well-architected, but many advanced features remain underutilized by current effect implementations.

---

## 1. Audio Pipeline Architecture

### 1.1 Signal Flow

```
I2S Microphone (16 kHz)
         │
         ▼
┌─────────────────────────────────┐
│      GoertzelAnalyzer           │
│  • 64-bin Goertzel DFT          │
│  • Variable window sizing       │
│  • Hann windowing via Q15 LUT   │
│  • A2 (110 Hz) to C8 (4186 Hz)  │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│       AudioActor                │
│  • Hop-based processing (256)   │
│  • Lookahead spike smoothing    │
│  • Zone AGC (4 zones)           │
│  • Attack/release smoothing     │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│        ControlBus               │
│  • RMS / fast_rms               │
│  • Flux / fast_flux             │
│  • bands[8] / heavy_bands[8]    │
│  • chroma[12] / heavy_chroma[12]│
│  • bins64[64] (Goertzel output) │
│  • waveform[128] (time-domain)  │
│  • ChordState                   │
│  • MusicalSaliencyFrame         │
│  • MusicStyle                   │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│     MusicalGrid                 │
│  • beat_phase01 (0.0-1.0)       │
│  • beat_tick / downbeat_tick    │
│  • bpm_smoothed                 │
│  • tempo_confidence             │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│     EffectContext.audio         │
│  (passed to effects each frame) │
└─────────────────────────────────┘
```

### 1.2 Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Goertzel over FFT** | Musical semitone bins, variable window sizes per frequency |
| **64-bin spectrum** | Full 5-octave musical range (A2-C8) with semitone resolution |
| **Dual smoothing** | `bands[]` for responsive effects, `heavy_bands[]` for smooth LGP viewing |
| **Zone AGC** | Prevents bass domination, balanced frequency response |
| **Lookahead spike removal** | 3-frame ring buffer eliminates single-frame flicker |
| **By-value AudioContext** | Thread-safe copy to effects, no shared state |

---

## 2. Available Audio Data Reference

### 2.1 Energy Metrics

| Accessor | Range | Smoothing | Status |
|----------|-------|-----------|--------|
| `ctx.audio.rms()` | 0.0-1.0 | Slow | **Used** |
| `ctx.audio.fastRms()` | 0.0-1.0 | Fast | **Unused** |
| `ctx.audio.flux()` | 0.0-1.0 | Slow | Partial |
| `ctx.audio.fastFlux()` | 0.0-1.0 | Fast | Partial |

### 2.2 Frequency Bands (8-Band)

| Accessor | Frequency | Smoothing | Status |
|----------|-----------|-----------|--------|
| `ctx.audio.bass()` | 60-120 Hz | Fast | **Used** |
| `ctx.audio.heavyBass()` | 60-120 Hz | Extra smooth | **Used** |
| `ctx.audio.mid()` | 250-1000 Hz | Fast | Partial |
| `ctx.audio.heavyMid()` | 250-1000 Hz | Extra smooth | **Unused** |
| `ctx.audio.treble()` | 2-8 kHz | Fast | **Unused** |
| `ctx.audio.heavyTreble()` | 2-8 kHz | Extra smooth | **Unused** |
| `ctx.audio.getBand(0-7)` | Individual | Fast | **Used** |
| `ctx.audio.getHeavyBand(0-7)` | Individual | Extra smooth | Partial |

### 2.3 Chromagram (12-Bin Pitch Classes)

| Accessor | Notes | Status |
|----------|-------|--------|
| `ctx.audio.controlBus.chroma[0-11]` | C, C#, D, ..., B | **Used** |
| `ctx.audio.controlBus.heavy_chroma[0-11]` | Smoothed version | **Used** |

**Note:** Chroma bins are folded from 64-bin Goertzel output, representing pitch class energy regardless of octave.

### 2.4 Beat/Tempo Tracking

| Accessor | Type | Status |
|----------|------|--------|
| `ctx.audio.beatPhase()` | float 0.0-1.0 | **Used** |
| `ctx.audio.isOnBeat()` | bool (single frame) | **Used** |
| `ctx.audio.isOnDownbeat()` | bool (beat 1 of measure) | **Unused** |
| `ctx.audio.bpm()` | float (30-200) | Partial |
| `ctx.audio.tempoConfidence()` | float 0.0-1.0 | **Unused** |

### 2.5 Chord Detection

| Accessor | Type | Status |
|----------|------|--------|
| `ctx.audio.chordType()` | ChordType enum | **Used** |
| `ctx.audio.rootNote()` | uint8_t 0-11 | **Used** |
| `ctx.audio.chordConfidence()` | float 0.0-1.0 | **Used** |
| `ctx.audio.isMajor()` | bool | **Used** |
| `ctx.audio.isMinor()` | bool | **Used** |
| `ctx.audio.isDiminished()` | bool | **Unused** |
| `ctx.audio.isAugmented()` | bool | **Unused** |
| `ctx.audio.hasChord()` | bool | Partial |

**Chord Types:** NONE, MAJOR, MINOR, DIMINISHED, AUGMENTED

### 2.6 Percussion Detection

| Accessor | Frequency | Status |
|----------|-----------|--------|
| `ctx.audio.snare()` | 150-300 Hz energy | **Unused** |
| `ctx.audio.hihat()` | 6-12 kHz energy | **Unused** |
| `ctx.audio.isSnareHit()` | Single-frame trigger | **Used** (some effects) |
| `ctx.audio.isHihatHit()` | Single-frame trigger | **Used** (some effects) |

### 2.7 Musical Saliency (MIS Phase 1)

| Accessor | Meaning | Status |
|----------|---------|--------|
| `ctx.audio.overallSaliency()` | Combined importance | **Unused** |
| `ctx.audio.harmonicSaliency()` | Chord/key changes | **Unused** |
| `ctx.audio.rhythmicSaliency()` | Beat pattern changes | **Unused** |
| `ctx.audio.timbralSaliency()` | Spectral texture changes | **Unused** |
| `ctx.audio.dynamicSaliency()` | Loudness envelope changes | **Unused** |
| `ctx.audio.isHarmonicDominant()` | bool | **Unused** |
| `ctx.audio.isRhythmicDominant()` | bool | **Unused** |
| `ctx.audio.isTimbralDominant()` | bool | **Unused** |
| `ctx.audio.isDynamicDominant()` | bool | **Unused** |

### 2.8 Music Style Detection (MIS Phase 2)

| Accessor | Type | Status |
|----------|------|--------|
| `ctx.audio.musicStyle()` | MusicStyle enum | **Unused** |
| `ctx.audio.styleConfidence()` | float 0.0-1.0 | **Unused** |
| `ctx.audio.isRhythmicMusic()` | bool (EDM, hip-hop) | **Unused** |
| `ctx.audio.isHarmonicMusic()` | bool (jazz, classical) | **Unused** |
| `ctx.audio.isMelodicMusic()` | bool (vocal pop) | **Unused** |
| `ctx.audio.isTextureMusic()` | bool (ambient, drone) | **Unused** |
| `ctx.audio.isDynamicMusic()` | bool (orchestral) | **Unused** |

**Music Styles:** UNKNOWN, RHYTHMIC_DRIVEN, HARMONIC_DRIVEN, MELODIC_DRIVEN, TEXTURE_DRIVEN, DYNAMIC_DRIVEN

### 2.9 64-Bin Goertzel Spectrum (Detailed Frequency Analysis)

| Accessor | Range | Notes | Status |
|----------|-------|-------|--------|
| `ctx.audio.bin(0-63)` | float (0-1) | A2 (110 Hz) to C8 (4186 Hz) | **Unused** |
| `ctx.audio.bins64()` | float* | Full array pointer | **Unused** |
| `ctx.audio.bins64Count()` | 64 | Constant | N/A |

**Technical Details:**
- **Algorithm**: Goertzel DFT (not FFT) - computes specific frequencies efficiently
- **Bin spacing**: Musical semitones (12 bins per octave)
- **Variable windows**: Bass bins use longer windows (~1500 samples) for better resolution
- **Windowing**: Hann window via Q15 fixed-point LUT
- Matches Sensory Bridge audio analysis algorithm

### 2.10 Raw Waveform (Time-Domain)

| Accessor | Type | Status |
|----------|------|--------|
| `ctx.audio.getWaveformSample(0-127)` | int16_t | **Used** |
| `ctx.audio.getWaveformAmplitude(0-127)` | float 0.0-1.0 | Available |
| `ctx.audio.getWaveformNormalized(0-127)` | float -1.0 to +1.0 | Available |
| `ctx.audio.waveformSize()` | 128 | Constant |

### 2.11 Behavior Recommendations (MIS Phase 3)

| Accessor | Type | Status |
|----------|------|--------|
| `ctx.audio.recommendedBehavior()` | VisualBehavior enum | **Unused** |
| `ctx.audio.shouldPulseOnBeat()` | bool | **Unused** |
| `ctx.audio.shouldDriftWithHarmony()` | bool | **Unused** |
| `ctx.audio.shouldShimmerWithMelody()` | bool | **Unused** |
| `ctx.audio.shouldBreatheWithDynamics()` | bool | **Unused** |
| `ctx.audio.shouldTextureFlow()` | bool | **Unused** |

---

## 3. Effect-by-Effect Analysis

### 3.1 AudioWaveformEffect

**File:** `src/effects/ieffect/AudioWaveformEffect.cpp`
**Quality:** Good (Sensory Bridge parity)

**Audio Features Used:**
- `getWaveformSample()` - Time-domain visualization
- `controlBus.chroma[]` - Color derivation
- `hop_seq` - Update synchronization

**Strengths:**
- Faithful Sensory Bridge algorithm port
- 4-frame history averaging for smoothness
- AGC follower for consistent brightness
- Chromagram-driven color

**Issues:**
- No percussion integration
- No beat sync option
- Potential flicker at low signal levels

**Recommendation:** Add optional beat-sync mode using `beatPhase()` for time-stretched waveform display.

---

### 3.2 AudioBloomEffect

**File:** `src/effects/ieffect/AudioBloomEffect.cpp`
**Quality:** Very Good (chord-enhanced)

**Audio Features Used:**
- `controlBus.heavy_chroma[]` - Color derivation (smoothed)
- `chordState.type` - Palette warmth modulation
- `chordState.rootNote` - Hue rotation
- `chordState.confidence` - Effect intensity
- `hop_seq` - Update synchronization

**Strengths:**
- Chord-driven palette warmth (major=warm, minor=cool)
- Root note hue shifting
- Logarithmic distortion for visual compression
- Fractional scroll accumulator for smooth motion
- Saturation boost post-processing

**Issues:**
- No percussion triggers
- No saliency integration
- No style adaptation

**Recommendation:** Add snare-triggered "bloom burst" at center using `isSnareHit()`.

---

### 3.3 LGPChordGlowEffect

**File:** `src/effects/ieffect/LGPChordGlowEffect.cpp`
**Quality:** Excellent (showcase effect)

**Audio Features Used:**
- `chordState.type` - Mood/saturation mapping
- `chordState.rootNote` - Base hue (chromatic circle)
- `chordState.confidence` - Overall brightness
- `hop_seq` - Update synchronization

**Strengths:**
- Full chord detection integration showcase
- 200ms cross-fade transitions on chord changes
- 3rd and 5th interval accent positions
- Chord mood configurations (MAJOR=happy, MINOR=melancholic, etc.)
- Chord change pulse animation

**Issues:**
- No beat synchronization
- No percussion integration
- No saliency integration

**Recommendation:** Add optional beat pulse overlay using `beatPhase()` and `isOnBeat()`.

---

### 3.4 LGPAudioTestEffect

**File:** `src/effects/ieffect/LGPAudioTestEffect.cpp`
**Quality:** Good (demonstration effect)

**Audio Features Used:**
- `rms()` - Master intensity
- `beatPhase()` - Animation timing
- `isOnBeat()` - Beat pulse
- `getBand(0-7)` - Spectrum visualization

**Strengths:**
- Graceful fallback when audio unavailable
- Beat pulse animation with decay
- Center-to-edge frequency mapping (bass at center)

**Issues:**
- Simple implementation (intentionally)
- No chord integration
- No advanced features

**Recommendation:** Keep as-is; serves as template/test effect.

---

### 3.5 Enhanced LGP Effects (Audio Integration)

Several LGP effects have been enhanced with audio features:

| Effect | Audio Features Used |
|--------|---------------------|
| **RippleEffect** | `chroma[]`, `hop_seq`, `isSnareHit()`, `rootNote()`, chord warmth |
| **LGPStarBurstEffect** | `chroma[]`, `heavy_bands[]`, `fastFlux()`, `isSnareHit()`, `isHihatHit()`, chord detection |
| **LGPWaveCollisionEffect** | `chroma[]`, `heavy_bands[]`, `isSnareHit()`, `isHihatHit()`, `heavyBass()` |
| **ChevronWavesEffect** | `heavy_bands[]`, slew-limited speed modulation |

---

## 4. What's Working Well

### 4.1 Audio Pipeline
- **Goertzel implementation** matches Sensory Bridge quality
- **Dual smoothing paths** (fast/heavy) provide flexibility
- **Zone AGC** effectively balances frequency response
- **Spike detection** eliminates single-frame flicker
- **Thread-safe design** with by-value AudioContext

### 4.2 Effect Integration
- **Chord detection** is well-integrated in Bloom and ChordGlow
- **Heavy_chroma** provides smooth color transitions
- **Percussion triggers** (snare/hihat) work in enhanced LGP effects
- **CENTER ORIGIN** compliance throughout

### 4.3 API Design
- **Convenience accessors** (`bass()`, `mid()`, `treble()`) simplify usage
- **Stub AudioContext** allows effects to compile without audio
- **Rich metadata** in ControlBusFrame for debugging

---

## 5. What's Missing/Incomplete

### 5.1 Underutilized Features

| Feature | Availability | Usage |
|---------|--------------|-------|
| 64-bin Goertzel spectrum | Full | **0%** |
| Musical saliency metrics | Full | **0%** |
| Style detection | Full | **0%** |
| Behavior recommendations | Full | **0%** |
| Downbeat detection | Full | **0%** |
| Tempo confidence | Full | **0%** |
| Fast RMS/flux | Full | **~5%** |
| Treble accessor | Full | **0%** |

### 5.2 Missing Effect Types

1. **Spectrum Analyzer** - Classic bar visualizer using `bins64[]`
2. **Saliency-Adaptive** - Effect that responds ONLY to dominant saliency
3. **Style-Switching** - Different behaviors for EDM vs classical vs ambient
4. **Melodic Tracker** - Follows treble/mid frequency movement
5. **Downbeat Emphasis** - Stronger response on beat 1 of measure

### 5.3 Integration Gaps

- **AudioWaveform**: No beat synchronization option
- **AudioBloom**: No percussion burst feature
- **LGPChordGlow**: No beat overlay
- **All effects**: No saliency-aware behavior

---

## 6. Recommendations

### 6.1 High Priority

#### 6.1.1 Create Spectrum Analyzer Effect
```cpp
// Use bins64[] for classic visualizer
for (uint8_t bin = 0; bin < 64; bin++) {
    float magnitude = ctx.audio.bin(bin);
    // Map to LED position and brightness
}
```

#### 6.1.2 Integrate Saliency in Existing Effects
```cpp
// Example: Only respond to dominant saliency
if (ctx.audio.isRhythmicDominant()) {
    // Pulse on beat
} else if (ctx.audio.isHarmonicDominant()) {
    // Drift with chord changes
}
```

#### 6.1.3 Add Percussion Bursts to AudioBloom
```cpp
if (ctx.audio.isSnareHit()) {
    m_burstIntensity = 1.0f;  // Trigger center burst
}
```

### 6.2 Medium Priority

#### 6.2.1 Style-Adaptive Effect
```cpp
switch (ctx.audio.musicStyle()) {
    case RHYTHMIC_DRIVEN:
        // Fast pulses, bass-heavy
        break;
    case HARMONIC_DRIVEN:
        // Slow color drifts, chord-responsive
        break;
    case TEXTURE_DRIVEN:
        // Ambient shimmer, minimal beat response
        break;
}
```

#### 6.2.2 Downbeat Emphasis
```cpp
if (ctx.audio.isOnDownbeat()) {
    intensity *= 1.5f;  // Stronger pulse on beat 1
}
```

### 6.3 Low Priority

- Add tempo confidence indicator to UI
- Create debug visualization for saliency metrics
- Implement 64-bin heatmap effect

---

## 7. Implementation Priority Matrix

| Priority | Feature | Effort | Impact |
|----------|---------|--------|--------|
| **P0** | Spectrum analyzer effect | Medium | High |
| **P0** | Saliency integration example | Low | High |
| **P1** | Percussion burst in Bloom | Low | Medium |
| **P1** | Beat overlay in ChordGlow | Low | Medium |
| **P2** | Style-adaptive effect | High | High |
| **P2** | 64-bin visualizer | Medium | Medium |
| **P3** | Downbeat emphasis | Low | Low |
| **P3** | Tempo confidence UI | Low | Low |

---

## 8. Technical Reference

### 8.1 Goertzel Analyzer Configuration

```cpp
// From GoertzelAnalyzer.h
static constexpr size_t NUM_BINS = 64;           // A2 to C8
static constexpr uint32_t SAMPLE_RATE_HZ = 16000;
static constexpr size_t MAX_BLOCK_SIZE = 1500;   // ~94ms @ 16kHz
static constexpr size_t MIN_BLOCK_SIZE = 64;     // ~4ms @ 16kHz
static constexpr size_t HANN_LUT_SIZE = 4096;    // Q15 fixed-point
```

### 8.2 ControlBus Smoothing Parameters

```cpp
// From ControlBus.h
float m_alpha_fast = 0.35f;        // Fast response
float m_alpha_slow = 0.12f;        // Slow response
float m_band_attack = 0.15f;       // Fast rise
float m_band_release = 0.03f;      // Slow fall
float m_heavy_band_attack = 0.08f; // Extra slow rise
float m_heavy_band_release = 0.015f; // Ultra slow fall
```

### 8.3 Saliency Tuning Defaults

```cpp
// From MusicalSaliency.h
float harmonicWeight = 0.25f;
float rhythmicWeight = 0.30f;
float timbralWeight = 0.20f;
float dynamicWeight = 0.25f;
```

---

## 9. Conclusion

The LightwaveOS audio-reactive system is well-architected with rich features. The main opportunity lies in **utilizing the advanced Musical Intelligence System features** (saliency, style detection, behavior recommendations) that are fully implemented but unused by current effects.

The **64-bin Goertzel spectrum** is a particularly valuable untapped resource that could enable new effect types like classic spectrum analyzers or melodic trackers.

**Next Steps:**
1. Create a "saliency-aware" reference effect as a template
2. Implement a basic spectrum analyzer using `bins64[]`
3. Add percussion triggers to AudioBloom effect
4. Document best practices for new audio-reactive effects
