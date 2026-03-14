# Audio-Reactive Effects Analysis

> Consolidated from two files on 2026-03-14

**Document Version:** 1.0.0
**Date:** 2025-12-29
**Author:** Claude Code Technical Audit
**Status:** Reference Documentation

---

## Executive Summary

LightwaveOS v2 implements a sophisticated audio-reactive visualization system featuring:

- **64-bin Goertzel frequency analysis** (NOT FFT) with Sensory Bridge algorithm parity
- **Musical Intelligence System (MIS)** with chord detection, saliency analysis, and style detection
- **4 dedicated audio-reactive effects** plus several enhanced LGP effects with audio integration
- **Rich AudioContext API** with 30+ accessors for effect developers

**Key Finding:** The audio pipeline is well-architected, but many advanced features remain underutilized by current effect implementations.

### Key Findings

| Category | Status | Summary |
|----------|--------|---------|
| Audio Pipeline | Excellent | Rich data: chroma, bands, saliency, chords, percussion |
| Effect Quality | Good | 4 effects ranging from basic to sophisticated |
| Feature Utilization | Partial | ~40% of available audio features actively used |
| CENTER ORIGIN | Perfect | All effects comply with LGP hardware requirements |
| Thread Safety | Excellent | AudioContext by-value prevents race conditions |

### Top Recommendations

1. **Quick Win**: Fix AudioWaveformEffect color flicker (change `chroma[]` to `heavy_chroma[]`)
2. **Medium Effort**: Add percussion detection to existing effects (snare/hihat triggers)
3. **Major Feature**: Create style-adaptive effect using `musicStyle()` detection

---

## 1. Audio Pipeline Architecture

### 1.1 Signal Flow (Goertzel Pipeline)

```
I2S Microphone (16 kHz)
         |
         v
+----------------------------------+
|      GoertzelAnalyzer            |
|  - 64-bin Goertzel DFT           |
|  - Variable window sizing        |
|  - Hann windowing via Q15 LUT    |
|  - A2 (110 Hz) to C8 (4186 Hz)  |
+----------------------------------+
         |
         v
+----------------------------------+
|       AudioActor                 |
|  - Hop-based processing (256)    |
|  - Lookahead spike smoothing     |
|  - Zone AGC (4 zones)            |
|  - Attack/release smoothing      |
+----------------------------------+
         |
         v
+----------------------------------+
|        ControlBus                |
|  - RMS / fast_rms                |
|  - Flux / fast_flux              |
|  - bands[8] / heavy_bands[8]    |
|  - chroma[12] / heavy_chroma[12]|
|  - bins64[64] (Goertzel output)  |
|  - waveform[128] (time-domain)   |
|  - ChordState                    |
|  - MusicalSaliencyFrame          |
|  - MusicStyle                    |
+----------------------------------+
         |
         v
+----------------------------------+
|     MusicalGrid                  |
|  - beat_phase01 (0.0-1.0)        |
|  - beat_tick / downbeat_tick     |
|  - bpm_smoothed                  |
|  - tempo_confidence              |
+----------------------------------+
         |
         v
+----------------------------------+
|     RendererActor                |
|  - Receives ControlBusFrame      |
|    via queue                     |
|  - Extrapolates values between   |
|    hops                          |
|  - Populates AudioContext        |
|    (by-value copy for thread     |
|    safety)                       |
+----------------------------------+
         |
         v
+----------------------------------+
|  Effect::render(EffectContext&)   |
|  - ctx.audio contains            |
|    AudioContext snapshot          |
|  - Thread-safe read-only access  |
|    to all audio data             |
|  - Maps audio features to LED    |
|    brightness, hue, saturation   |
+----------------------------------+
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

### 1.3 Timing Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sample Rate | 44,100 Hz | Standard audio |
| Hop Size | 256 samples | ~5.8ms per hop |
| Hop Rate | ~172 Hz | ControlBus update frequency |
| LED Frame Rate | 120 Hz | Target render rate |
| Smoothing Attack | 0.08 (heavy) | Fast rise for transients |
| Smoothing Release | 0.015 (heavy) | Slow decay for smooth visuals |

---

## 2. Available Audio Data Reference

### 2.1 Energy & Dynamics

| Accessor | Type | Range | Smoothing | Description | Status |
|----------|------|-------|-----------|-------------|--------|
| `ctx.audio.rms()` | float | 0.0-1.0 | Slow | Overall energy level | **Used** |
| `ctx.audio.fastRms()` | float | 0.0-1.0 | Fast | Less-smoothed RMS (faster response) | **Unused** |
| `ctx.audio.flux()` | float | 0.0-1.0 | Slow | Spectral change (onset detection) | Partial |
| `ctx.audio.fastFlux()` | float | 0.0-1.0 | Fast | Less-smoothed flux | Partial |

### 2.2 Frequency Bands (8-Band)

| Accessor | Frequency Range | Smoothing | Typical Use | Status |
|----------|-----------------|-----------|-------------|--------|
| `ctx.audio.bass()` / `getBand(0)` | 60-120 Hz | Fast | Sub-bass, kick drums | **Used** |
| `ctx.audio.heavyBass()` | 60-120 Hz | Extra smooth | Sub-bass, kick drums | **Used** |
| `ctx.audio.getBand(1)` | 120-250 Hz | Fast | Bass, bass guitar | **Used** |
| `ctx.audio.getBand(2)` | 250-500 Hz | Fast | Low-mids, warmth | **Used** |
| `ctx.audio.mid()` / `getBand(3)` | 500-1000 Hz | Fast | Mids, vocals body | Partial |
| `ctx.audio.heavyMid()` | 250-1000 Hz | Extra smooth | Mids | **Unused** |
| `ctx.audio.getBand(4)` | 1-2 kHz | Fast | Upper-mids, presence | **Used** |
| `ctx.audio.getBand(5)` | 2-4 kHz | Fast | Clarity, articulation | **Used** |
| `ctx.audio.treble()` | 2-8 kHz | Fast | Treble | **Unused** |
| `ctx.audio.heavyTreble()` | 2-8 kHz | Extra smooth | Treble | **Unused** |
| `ctx.audio.getBand(6)` | 4-8 kHz | Fast | Brilliance, hi-hats | **Used** |
| `ctx.audio.getBand(7)` | 8-16 kHz | Fast | Air, cymbal shimmer | **Used** |

**Heavy (smoothed) versions:** `ctx.audio.getHeavyBand(0-7)` / `ctx.audio.controlBus.heavy_bands[0-7]`

### 2.3 Chromagram (12-Bin Pitch Classes)

| Index | Note | Hue Mapping (suggested) | Status |
|-------|------|------------------------|--------|
| 0 | C | 0 (Red) | **Used** |
| 1 | C#/Db | 21 | **Used** |
| 2 | D | 42 | **Used** |
| 3 | D#/Eb | 63 | **Used** |
| 4 | E | 84 | **Used** |
| 5 | F | 105 | **Used** |
| 6 | F#/Gb | 126 | **Used** |
| 7 | G | 147 (Cyan) | **Used** |
| 8 | G#/Ab | 168 | **Used** |
| 9 | A | 189 | **Used** |
| 10 | A#/Bb | 210 | **Used** |
| 11 | B | 231 (Violet) | **Used** |

**Accessors:**
- `ctx.audio.controlBus.chroma[0-11]` - Fast-changing (can cause flicker)
- `ctx.audio.controlBus.heavy_chroma[0-11]` - Smoothed (recommended for color)

**Note:** Chroma bins are folded from 64-bin Goertzel output, representing pitch class energy regardless of octave.

### 2.4 Beat/Tempo Tracking

| Accessor | Type | Description | Status |
|----------|------|-------------|--------|
| `ctx.audio.beatPhase()` | float 0.0-1.0 | Position within current beat | **Used** |
| `ctx.audio.isOnBeat()` | bool (single frame) | True at beat onset | **Used** |
| `ctx.audio.isOnDownbeat()` / `isDownbeat()` | bool (beat 1 of measure) | First beat of measure | **Unused** |
| `ctx.audio.bpm()` | float (30-200) | Detected tempo (BPM) | Partial |
| `ctx.audio.tempoConfidence()` | float 0.0-1.0 | Confidence in tempo detection | **Unused** |

### 2.5 Chord Detection

| Accessor | Type | Description | Status |
|----------|------|-------------|--------|
| `ctx.audio.rootNote()` | uint8_t 0-11 | Root note of detected chord | **Used** |
| `ctx.audio.chordType()` | ChordType enum | NONE, MAJOR, MINOR, DIMINISHED, AUGMENTED | **Used** |
| `ctx.audio.chordConfidence()` | float 0.0-1.0 | Detection confidence | **Used** |
| `ctx.audio.isMajor()` | bool | Quick check for major chord | **Used** |
| `ctx.audio.isMinor()` | bool | Quick check for minor chord | **Used** |
| `ctx.audio.isDiminished()` | bool | Quick check for diminished chord | **Unused** |
| `ctx.audio.isAugmented()` | bool | Quick check for augmented chord | **Unused** |
| `ctx.audio.hasChord()` | bool | Whether any chord is detected | Partial |

### 2.6 Percussion Detection

| Accessor | Type | Frequency | Description | Status |
|----------|------|-----------|-------------|--------|
| `ctx.audio.snare()` | float (0-1) | 150-300 Hz | Snare band energy | **Unused** |
| `ctx.audio.hihat()` | float (0-1) | 6-12 kHz | Hi-hat band energy | **Unused** |
| `ctx.audio.isSnareHit()` | bool | -- | Onset trigger for snare | **Used** (some effects) |
| `ctx.audio.isHihatHit()` | bool | -- | Onset trigger for hi-hat | **Used** (some effects) |

### 2.7 Musical Saliency (MIS Phase 1)

| Accessor | Type | Meaning | Status |
|----------|------|---------|--------|
| `ctx.audio.overallSaliency()` | float (0-1) | Combined importance | **Unused** |
| `ctx.audio.harmonicSaliency()` | float (0-1) | Chord/key changes | **Unused** |
| `ctx.audio.rhythmicSaliency()` | float (0-1) | Beat pattern changes | **Unused** |
| `ctx.audio.timbralSaliency()` | float (0-1) | Spectral texture changes | **Unused** |
| `ctx.audio.dynamicSaliency()` | float (0-1) | Loudness envelope changes | **Unused** |
| `ctx.audio.isHarmonicDominant()` | bool | -- | **Unused** |
| `ctx.audio.isRhythmicDominant()` | bool | -- | **Unused** |
| `ctx.audio.isTimbralDominant()` | bool | -- | **Unused** |
| `ctx.audio.isDynamicDominant()` | bool | -- | **Unused** |

### 2.8 Music Style Detection (MIS Phase 2)

| Value | Description | Visualization Strategy | Status |
|-------|-------------|----------------------|--------|
| `RHYTHMIC_DRIVEN` | EDM, hip-hop, dance | Strong beat sync, pulse effects | **Unused** |
| `HARMONIC_DRIVEN` | Jazz, classical, ambient | Chord-aware coloring | **Unused** |
| `MELODIC_DRIVEN` | Vocal pop, ballads | Pitch-following, smooth glides | **Unused** |
| `TEXTURE_DRIVEN` | Ambient, soundscapes | Gradual morphing, no pulses | **Unused** |
| `DYNAMIC_DRIVEN` | Orchestral, builds | Intensity-based brightness | **Unused** |

**Accessor:** `ctx.audio.musicStyle()`, `ctx.audio.styleConfidence()`

Additional style queries:
- `ctx.audio.isRhythmicMusic()`, `isHarmonicMusic()`, `isMelodicMusic()`, `isTextureMusic()`, `isDynamicMusic()`

### 2.9 Behavior Recommendations (MIS Phase 3)

| Accessor | Type | Status |
|----------|------|--------|
| `ctx.audio.recommendedBehavior()` | VisualBehavior enum | **Unused** |
| `ctx.audio.shouldPulseOnBeat()` | bool | **Unused** |
| `ctx.audio.shouldDriftWithHarmony()` | bool | **Unused** |
| `ctx.audio.shouldShimmerWithMelody()` | bool | **Unused** |
| `ctx.audio.shouldBreatheWithDynamics()` | bool | **Unused** |
| `ctx.audio.shouldTextureFlow()` | bool | **Unused** |

### 2.10 64-Bin Goertzel Spectrum (Detailed Frequency Analysis)

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

### 2.11 Raw Waveform (Time-Domain)

| Accessor | Type | Description | Status |
|----------|------|-------------|--------|
| `ctx.audio.getWaveformSample(0-127)` | int16_t | Raw audio samples | **Used** |
| `ctx.audio.getWaveformAmplitude(0-127)` | float 0.0-1.0 | Absolute amplitude | Available |
| `ctx.audio.getWaveformNormalized(0-127)` | float -1.0 to +1.0 | Normalised bipolar | Available |
| `ctx.audio.waveformSize()` | 128 | Constant | N/A |

---

## 3. Effect-by-Effect Analysis

### 3.1 LGPAudioTestEffect

**File:** `src/effects/ieffect/LGPAudioTestEffect.cpp`
**Purpose:** Basic audio pipeline demonstration
**Quality:** Good (demonstration effect)

**Audio Features Used:**
- `rms()` - Overall energy / master intensity
- `beatPhase()` - Beat timing / animation timing
- `isOnBeat()` - Beat detection / beat pulse
- `getBand(0-7)` - Frequency bands / spectrum visualization

**Visual Pattern:**
- Center-origin radial expansion
- Distance-to-band mapping (center=bass, edges=treble)
- Beat-synchronized brightness pulse
- Graceful fallback when audio unavailable

**Audio Mapping Logic:**
```cpp
masterIntensity = 0.3f + (rms * 0.5f) + (beatPulse * 0.2f);
// Maps LED distance to frequency band:
// dist 0-9 -> band 0 (bass)
// dist 70-79 -> band 7 (treble)
```

**Recommendation:** Keep as-is; serves as template/test effect.

---

### 3.2 AudioWaveformEffect (ID: 72)

**File:** `src/effects/ieffect/AudioWaveformEffect.cpp`
**Purpose:** Time-domain waveform visualization (Sensory Bridge parity)
**Quality:** Good

**Audio Features Used:**
- `getWaveformSample()` - Time-domain visualization
- `controlBus.chroma[]` - Color derivation
- `hop_seq` - Update synchronization
- 4-frame history buffer for smoothing

**Visual Pattern:**
- Mirrored waveform from center outward
- Peak envelope with slow decay
- Chromatic coloring from dominant pitch
- AGC follower for consistent brightness

**Known Issue:**
```cpp
// Line 102 uses fast chroma (causes color flicker):
float colorMix = ctx.audio.controlBus.chroma[c];

// SHOULD use heavy_chroma for smooth color:
float colorMix = ctx.audio.controlBus.heavy_chroma[c];
```

**Other Issues:**
- No percussion integration
- No beat sync option
- Potential flicker at low signal levels

**Recommendation:** Fix heavy_chroma issue. Add optional beat-sync mode using `beatPhase()` for time-stretched waveform display.

---

### 3.3 AudioBloomEffect (ID: 73)

**File:** `src/effects/ieffect/AudioBloomEffect.cpp`
**Purpose:** Chromagram-driven scrolling visualization
**Quality:** Very Good (chord-enhanced)

**Audio Features Used:**
- `controlBus.heavy_chroma[]` - Color derivation (smoothed)
- `chordState.type` - Palette warmth modulation
- `chordState.rootNote` - Hue rotation
- `chordState.confidence` - Effect intensity
- `hop_seq` - Update synchronization

**Visual Pattern:**
- Center-origin push-outward scrolling
- Color driven by chromagram + chord mood
- Logarithmic distortion for LGP viewing
- Fractional scroll accumulator for smooth motion
- Saturation boost post-processing

**Chord-to-Mood Mapping:**
```cpp
// Emotional color shifts based on chord type:
MAJOR: +32 hue offset (warm/orange)
MINOR: -24 hue offset (cool/blue)
DIMINISHED: -32 hue offset (darker)
AUGMENTED: +40 hue offset (ethereal)
```

**Issues:**
- No percussion triggers
- No saliency integration
- No style adaptation

**Recommendation:** Add snare-triggered "bloom burst" at center using `isSnareHit()`.

---

### 3.4 LGPChordGlowEffect (ID: 75)

**File:** `src/effects/ieffect/LGPChordGlowEffect.cpp`
**Purpose:** Full chord detection showcase
**Quality:** Excellent (showcase effect)

**Audio Features Used:**
- `chordState.type` - Mood/saturation mapping
- `chordState.rootNote` - Base hue (chromatic circle)
- `chordState.confidence` - Overall brightness
- `hop_seq` - Update synchronization
- Chord intervals (3rd, 5th) for accent positions

**Visual Pattern:**
- Center-origin radial glow
- Root note to hue (chromatic circle)
- Chord type to saturation/mood
- Accent LEDs at 1/3 and 2/3 radius for intervals
- 200ms cross-fade transitions on chord changes
- Chord change pulse / ripple burst animation

**Mood Configuration:**
```cpp
mood[MAJOR] = {sat:255, offset:0, bright:1.0};
mood[MINOR] = {sat:200, offset:-10, bright:0.85};
mood[DIMINISHED] = {sat:140, offset:15, bright:0.7};
mood[AUGMENTED] = {sat:240, offset:30, bright:0.95};
```

**Issues:**
- No beat synchronization
- No percussion integration
- No saliency integration

**Recommendation:** Add optional beat pulse overlay using `beatPhase()` and `isOnBeat()`.

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

### 4.2 Technical Excellence

| Aspect | Evidence |
|--------|----------|
| **CENTER ORIGIN Compliance** | All effects use `SET_CENTER_PAIR()` macro correctly |
| **Thread Safety** | AudioContext passed by-value prevents race conditions |
| **Graceful Degradation** | All effects handle `ctx.audio.available == false` |
| **Frame Sync** | Proper use of `hop_seq` to detect new audio data |
| **Smoothing** | Heavy chroma/bands prevent visual jitter (when used) |
| **LGP Optimization** | Logarithmic distortion, tuned time constants |

### 4.3 Audio-Visual Mapping Quality

| Effect | Mapping Sophistication |
|--------|----------------------|
| LGPAudioTestEffect | Basic: energy -> brightness, bands -> position |
| AudioWaveformEffect | Intermediate: waveform -> shape, chroma -> color |
| AudioBloomEffect | Advanced: chroma -> scroll, chords -> mood |
| LGPChordGlowEffect | Expert: full harmonic analysis -> visual design |

### 4.4 Performance
- All effects render in <1ms (well under 8.3ms budget for 120 FPS)
- No heap allocations in render loops
- Radial buffers appropriately sized

### 4.5 API Design
- **Convenience accessors** (`bass()`, `mid()`, `treble()`) simplify usage
- **Stub AudioContext** allows effects to compile without audio
- **Rich metadata** in ControlBusFrame for debugging

---

## 5. What's Missing/Incomplete

### 5.1 Underutilized Features

| Feature | Availability | Usage | Gap |
|---------|-------------|-------|-----|
| 64-bin Goertzel spectrum | Full | **0%** | Detailed spectrum unused |
| Musical saliency metrics | Full | **0%** | Could drive adaptive behavior |
| Style detection | Full | **0%** | Genre-aware rendering possible |
| Behavior recommendations | Full | **0%** | -- |
| Downbeat detection | Full | **0%** | -- |
| Tempo confidence | Full | **0%** | -- |
| Fast RMS/flux | Full | **~5%** | -- |
| Treble accessor | Full | **0%** | -- |
| Percussion Detection | Full | **0%** | High impact opportunity |
| Onset Triggers | Full | **0%** | Visual punch potential |

### 5.2 Missing Effect Types

1. **Spectrum Analyzer** - Classic bar visualizer using `bins64[]`
2. **Saliency-Adaptive** - Effect that responds ONLY to dominant saliency
3. **Style-Switching** - Different behaviors for EDM vs classical vs ambient
4. **Melodic Tracker** - Follows treble/mid frequency movement
5. **Downbeat Emphasis** - Stronger response on beat 1 of measure
6. **Percussion-Focused** - Dedicated drum pattern visualizer

### 5.3 Code Issues

| Issue | Location | Severity | Fix |
|-------|----------|----------|-----|
| Color flicker | `AudioWaveformEffect.cpp:102` | Medium | Change `chroma[]` to `heavy_chroma[]` |
| Static mood values | `LGPChordGlowEffect.cpp` | Low | Could be runtime configurable |
| No error logging | Various | Low | Add validation when audio unavailable |

### 5.4 Architectural Gaps

1. **No per-effect audio mapping configuration** - Effects hardcode their audio-to-visual mapping
2. **No saliency-driven effect selection** - System does not switch effects based on music type
3. **No genre adaptation** - Same visual response regardless of music style

---

## 6. Recommendations

### 6.1 Priority 1: Quick Wins (< 1 hour each)

#### 6.1.1 Fix AudioWaveform Color Flicker
```cpp
// File: src/effects/ieffect/AudioWaveformEffect.cpp
// Line 102: Change from:
float colorMix = ctx.audio.controlBus.chroma[c];
// To:
float colorMix = ctx.audio.controlBus.heavy_chroma[c];
```
**Impact:** Eliminates color flickering in waveform visualization

#### 6.1.2 Add Percussion Response to AudioBloom
```cpp
// Add to render() after scroll update:
if (ctx.audio.isSnareHit()) {
    m_bloomIntensity = 1.0f;  // Flash on snare
}
if (ctx.audio.isHihatHit()) {
    m_shimmerPhase += 0.5f;   // Shimmer on hi-hat
}
```
**Impact:** Makes AudioBloom more rhythmically responsive

#### 6.1.3 Integrate Saliency in Existing Effects
```cpp
// Example: Only respond to dominant saliency
if (ctx.audio.isRhythmicDominant()) {
    // Pulse on beat
} else if (ctx.audio.isHarmonicDominant()) {
    // Drift with chord changes
}
```

### 6.2 Priority 2: Medium Effort (2-4 hours each)

#### 6.2.1 Create Spectrum Analyzer Effect
```cpp
// Use bins64[] for classic visualizer
for (uint8_t bin = 0; bin < 64; bin++) {
    float magnitude = ctx.audio.bin(bin);
    // Map to LED position and brightness
}
```

#### 6.2.2 Create Percussion-Focused Effect
New effect that specifically visualizes drum patterns:
- Snare -> center burst (radius 0-40)
- Kick -> outer pulse (radius 60-80)
- Hi-hat -> edge shimmer (radius 70-80)
- Creates layered rhythmic visualization

#### 6.2.3 Add Saliency-Driven Behavior
```cpp
void render(EffectContext& ctx) {
    if (ctx.audio.harmonicSaliency() > 0.5f) {
        // Chord change detected - trigger color transition
        startColorTransition(ctx.audio.rootNote());
    }
    if (ctx.audio.rhythmicSaliency() > 0.5f) {
        // Beat pattern change - adjust pulse intensity
        m_pulseIntensity *= 1.5f;
    }
}
```
**Impact:** Effects respond to musical structure, not just raw features

#### 6.2.4 Add Downbeat Emphasis
```cpp
if (ctx.audio.isOnDownbeat()) {
    intensity *= 1.5f;  // Stronger pulse on beat 1
}
```

### 6.3 Priority 3: Major Features (1-2 days each)

#### 6.3.1 Genre-Adaptive / Style-Switching Effect
Single effect that adapts its behavior based on detected music style:
```cpp
switch (ctx.audio.musicStyle()) {
    case MusicStyle::RHYTHMIC_DRIVEN:
        // EDM mode: sharp beat pulses, high contrast
        m_beatResponse = 1.0f;
        m_smoothing = 0.3f;
        break;
    case MusicStyle::HARMONIC_DRIVEN:
        // Jazz mode: smooth chord transitions, rich colors
        m_beatResponse = 0.3f;
        m_smoothing = 0.9f;
        break;
    case MusicStyle::MELODIC_DRIVEN:
        // Vocal pop: pitch-following, smooth glides
        renderPitchGlide(ctx);
        break;
    case MusicStyle::TEXTURE_DRIVEN:
        // Ambient mode: glacial motion, subtle shifts
        m_beatResponse = 0.0f;
        m_smoothing = 0.98f;
        break;
    case MusicStyle::DYNAMIC_DRIVEN:
        // Orchestral: intensity-based brightness
        renderDynamicSwell(ctx);
        break;
}
```

#### 6.3.2 64-Bin Spectrum Visualizer
Create detailed frequency display using `ctx.audio.bin(0-63)`:
- Logarithmic binning to match ear perception
- Smooth interpolation between adjacent bins
- Color gradient from bass (warm) to treble (cool)

### 6.4 Low Priority

- Add tempo confidence indicator to UI
- Create debug visualization for saliency metrics
- Implement 64-bin heatmap effect
- Runtime mood configuration (2 hr, low impact)

---

## 7. Implementation Priority Matrix

| Feature | Effort | Impact | Priority |
|---------|--------|--------|----------|
| Fix AudioWaveform flicker | 5 min | Medium | **P0 - Do Now** |
| Add percussion to existing effects | 1-2 hr | High | **P0 - Do Now** |
| Saliency integration example | Low | High | **P0 - Do Now** |
| Create percussion-focused effect | 3-4 hr | High | P1 - Soon |
| Spectrum analyzer effect | Medium | High | P1 - Soon |
| Add saliency-driven behavior | 2-3 hr | Medium | P1 - Soon |
| Beat overlay in ChordGlow | Low | Medium | P1 - Soon |
| Genre-adaptive effect | 1-2 days | Very High | P2 - Planned |
| 64-bin spectrum visualizer | 4-6 hr | Medium | P2 - Planned |
| Downbeat emphasis | Low | Low | P3 - Backlog |
| Tempo confidence UI | Low | Low | P3 - Backlog |
| Runtime mood configuration | 2 hr | Low | P3 - Backlog |

---

## 8. Code Examples

### 8.1 Using Percussion Detection

```cpp
void render(EffectContext& ctx) {
    // Snare-triggered center burst
    if (ctx.audio.isSnareHit()) {
        for (uint16_t dist = 0; dist < 40; ++dist) {
            uint8_t brightness = 255 - (dist * 6);
            SET_CENTER_PAIR(ctx, dist, CHSV(0, 200, brightness));
        }
    }

    // Hi-hat shimmer at edges
    if (ctx.audio.isHihatHit()) {
        for (uint16_t dist = 60; dist < HALF_LENGTH; ++dist) {
            uint8_t brightness = 128 + random8(127);
            SET_CENTER_PAIR(ctx, dist, CHSV(160, 100, brightness));
        }
    }
}
```

### 8.2 Using Musical Saliency

```cpp
void render(EffectContext& ctx) {
    // React to what's musically important
    float importance = max(
        ctx.audio.harmonicSaliency(),
        ctx.audio.rhythmicSaliency()
    );

    if (importance > 0.7f) {
        // Major musical event - dramatic visual response
        m_intensity = 1.0f;
        m_transitionSpeed = 2.0f;
    } else if (importance < 0.2f) {
        // Quiet passage - subtle ambient motion
        m_intensity *= 0.95f;
        m_transitionSpeed = 0.5f;
    }
}
```

### 8.3 Style-Aware Rendering

```cpp
void render(EffectContext& ctx) {
    switch (ctx.audio.musicStyle()) {
        case MusicStyle::RHYTHMIC_DRIVEN:
            // EDM mode: sharp beat pulses, high contrast
            m_beatResponse = 1.0f;
            m_smoothing = 0.3f;
            break;

        case MusicStyle::HARMONIC_DRIVEN:
            // Jazz mode: smooth chord transitions, rich colors
            m_beatResponse = 0.3f;
            m_smoothing = 0.9f;
            break;

        case MusicStyle::TEXTURE_DRIVEN:
            // Ambient mode: glacial motion, subtle shifts
            m_beatResponse = 0.0f;
            m_smoothing = 0.98f;
            break;
    }
}
```

---

## 9. Technical Reference

### 9.1 Goertzel Analyzer Configuration

```cpp
// From GoertzelAnalyzer.h
static constexpr size_t NUM_BINS = 64;           // A2 to C8
static constexpr uint32_t SAMPLE_RATE_HZ = 16000;
static constexpr size_t MAX_BLOCK_SIZE = 1500;   // ~94ms @ 16kHz
static constexpr size_t MIN_BLOCK_SIZE = 64;     // ~4ms @ 16kHz
static constexpr size_t HANN_LUT_SIZE = 4096;    // Q15 fixed-point
```

### 9.2 ControlBus Smoothing Parameters

```cpp
// From ControlBus.h
float m_alpha_fast = 0.35f;        // Fast response
float m_alpha_slow = 0.12f;        // Slow response
float m_band_attack = 0.15f;       // Fast rise
float m_band_release = 0.03f;      // Slow fall
float m_heavy_band_attack = 0.08f; // Extra slow rise
float m_heavy_band_release = 0.015f; // Ultra slow fall
```

### 9.3 Saliency Tuning Defaults

```cpp
// From MusicalSaliency.h
float harmonicWeight = 0.25f;
float rhythmicWeight = 0.30f;
float timbralWeight = 0.20f;
float dynamicWeight = 0.25f;
```

---

## Appendix A: File Locations

| Component | Path |
|-----------|------|
| ControlBus (audio data) | `v2/src/audio/contracts/ControlBus.h` |
| AudioContext (effect API) | `v2/src/plugins/api/EffectContext.h` |
| AudioWaveformEffect | `v2/src/effects/ieffect/AudioWaveformEffect.cpp` |
| AudioBloomEffect | `v2/src/effects/ieffect/AudioBloomEffect.cpp` |
| LGPChordGlowEffect | `v2/src/effects/ieffect/LGPChordGlowEffect.cpp` |
| LGPAudioTestEffect | `v2/src/effects/ieffect/LGPAudioTestEffect.cpp` |

## Appendix B: Related Documentation

- `AUDIO_VISUAL_SEMANTIC_MAPPING.md` - Philosophy of audio-visual mapping
- `AUDIO_CONTROL_API.md` - Control bus API reference
- `AUDIO_LGP_EXPECTED_OUTCOMES.md` - Expected visual behaviors
- `AUDIO_STREAM_API.md` - Real-time audio streaming

---

## 10. Conclusion

The LightwaveOS audio-reactive system is well-architected with rich features. The main opportunity lies in **utilizing the advanced Musical Intelligence System features** (saliency, style detection, behavior recommendations) that are fully implemented but unused by current effects.

The **64-bin Goertzel spectrum** is a particularly valuable untapped resource that could enable new effect types like classic spectrum analyzers or melodic trackers.

**Next Steps:**
1. Create a "saliency-aware" reference effect as a template
2. Implement a basic spectrum analyzer using `bins64[]`
3. Add percussion triggers to AudioBloom effect
4. Document best practices for new audio-reactive effects

---

**End of Document**
