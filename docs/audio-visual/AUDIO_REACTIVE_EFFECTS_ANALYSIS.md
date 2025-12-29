# Audio-Reactive Effects Analysis

**Document Version:** 1.0.0
**Date:** 2025-12-29
**Author:** Claude Code Technical Audit
**Status:** Reference Documentation

---

## Executive Summary

This document provides a comprehensive analysis of the audio-reactive visualization system in LightwaveOS v2. The system demonstrates **sophisticated audio processing** with chord detection, musical saliency analysis, and multi-band frequency tracking—however, **many advanced features remain underutilized** by the current effect implementations.

### Key Findings

| Category | Status | Summary |
|----------|--------|---------|
| Audio Pipeline | Excellent | Rich data: chroma, bands, saliency, chords, percussion |
| Effect Quality | Good | 4 effects ranging from basic to sophisticated |
| Feature Utilization | Partial | ~40% of available audio features actively used |
| CENTER ORIGIN | Perfect | All effects comply with LGP hardware requirements |
| Thread Safety | Excellent | AudioContext by-value prevents race conditions |

### Top Recommendations

1. **Quick Win**: Fix AudioWaveformEffect color flicker (change `chroma[]` → `heavy_chroma[]`)
2. **Medium Effort**: Add percussion detection to existing effects (snare/hihat triggers)
3. **Major Feature**: Create style-adaptive effect using `musicStyle()` detection

---

## 1. Audio Pipeline Architecture

### Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                      I2S MICROPHONE INPUT                       │
│                    (44.1kHz, 16-bit mono)                       │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        AudioActor                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ Ring Buffer │→ │ DSP Engine  │→ │ Feature     │             │
│  │ (512 samp)  │  │ (FFT, RMS)  │  │ Extraction  │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ControlBusRawInput                           │
│  • RMS (energy)           • Bands[8] (frequency spectrum)       │
│  • Flux (spectral change) • Chroma[12] (pitch classes)          │
│  • Waveform[128] (time)   • Beat detection                      │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        ControlBus                               │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │ Asymmetric      │  │ Chord Detection │  │ Musical         │ │
│  │ Smoothing       │  │ (root, type,    │  │ Saliency        │ │
│  │ (attack/release)│  │  confidence)    │  │ Analysis        │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
│                                                                 │
│  Output: ControlBusFrame @ ~172 Hz (every hop)                  │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      RendererActor                              │
│  • Receives ControlBusFrame via queue                           │
│  • Extrapolates values between hops                             │
│  • Populates AudioContext (by-value copy for thread safety)     │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Effect::render(EffectContext&)                 │
│  • ctx.audio contains AudioContext snapshot                     │
│  • Thread-safe read-only access to all audio data               │
│  • Maps audio features → LED brightness, hue, saturation        │
└─────────────────────────────────────────────────────────────────┘
```

### Timing Characteristics

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

| Accessor | Type | Range | Description |
|----------|------|-------|-------------|
| `ctx.audio.rms()` | float | 0.0-1.0 | Overall energy level |
| `ctx.audio.fastRms()` | float | 0.0-1.0 | Less-smoothed RMS (faster response) |
| `ctx.audio.flux()` | float | 0.0-1.0 | Spectral change (onset detection) |
| `ctx.audio.fastFlux()` | float | 0.0-1.0 | Less-smoothed flux |

### 2.2 Frequency Bands (8-band)

| Accessor | Frequency Range | Typical Use |
|----------|-----------------|-------------|
| `ctx.audio.getBand(0)` | 60-120 Hz | Sub-bass, kick drums |
| `ctx.audio.getBand(1)` | 120-250 Hz | Bass, bass guitar |
| `ctx.audio.getBand(2)` | 250-500 Hz | Low-mids, warmth |
| `ctx.audio.getBand(3)` | 500-1000 Hz | Mids, vocals body |
| `ctx.audio.getBand(4)` | 1-2 kHz | Upper-mids, presence |
| `ctx.audio.getBand(5)` | 2-4 kHz | Clarity, articulation |
| `ctx.audio.getBand(6)` | 4-8 kHz | Brilliance, hi-hats |
| `ctx.audio.getBand(7)` | 8-16 kHz | Air, cymbal shimmer |

**Heavy (smoothed) versions:** `ctx.audio.controlBus.heavy_bands[0-7]`

### 2.3 Chromagram (12 pitch classes)

| Index | Note | Hue Mapping (suggested) |
|-------|------|------------------------|
| 0 | C | 0 (Red) |
| 1 | C#/Db | 21 |
| 2 | D | 42 |
| 3 | D#/Eb | 63 |
| 4 | E | 84 |
| 5 | F | 105 |
| 6 | F#/Gb | 126 |
| 7 | G | 147 (Cyan) |
| 8 | G#/Ab | 168 |
| 9 | A | 189 |
| 10 | A#/Bb | 210 |
| 11 | B | 231 (Violet) |

**Accessors:**
- `ctx.audio.controlBus.chroma[0-11]` - Fast-changing (can cause flicker)
- `ctx.audio.controlBus.heavy_chroma[0-11]` - Smoothed (recommended for color)

### 2.4 Chord Detection

| Accessor | Type | Description |
|----------|------|-------------|
| `ctx.audio.rootNote()` | uint8_t (0-11) | Root note of detected chord |
| `ctx.audio.chordType()` | enum | NONE, MAJOR, MINOR, DIMINISHED, AUGMENTED |
| `ctx.audio.chordConfidence()` | float (0-1) | Detection confidence |
| `ctx.audio.isMajor()` | bool | Quick check for major chord |
| `ctx.audio.isMinor()` | bool | Quick check for minor chord |

### 2.5 Percussion Detection

| Accessor | Type | Description | Status |
|----------|------|-------------|--------|
| `ctx.audio.snare()` | float (0-1) | Snare band energy (150-300 Hz) | **Unused** |
| `ctx.audio.hihat()` | float (0-1) | Hi-hat band energy (6-12 kHz) | **Unused** |
| `ctx.audio.isSnareHit()` | bool | Onset trigger for snare | **Unused** |
| `ctx.audio.isHihatHit()` | bool | Onset trigger for hi-hat | **Unused** |

### 2.6 Beat & Tempo

| Accessor | Type | Description |
|----------|------|-------------|
| `ctx.audio.beatPhase()` | float (0-1) | Position within current beat |
| `ctx.audio.isOnBeat()` | bool | True at beat onset |
| `ctx.audio.bpm()` | float | Detected tempo (BPM) |
| `ctx.audio.isDownbeat()` | bool | First beat of measure |

### 2.7 Musical Saliency (What's "Important")

| Accessor | Type | Description | Status |
|----------|------|-------------|--------|
| `ctx.audio.harmonicSaliency()` | float (0-1) | Chord/key changes | **Unused** |
| `ctx.audio.rhythmicSaliency()` | float (0-1) | Beat pattern changes | **Unused** |
| `ctx.audio.timbralSaliency()` | float (0-1) | Spectral texture changes | **Unused** |
| `ctx.audio.dynamicSaliency()` | float (0-1) | Loudness changes | **Unused** |

### 2.8 Style Detection

| Value | Description | Visualization Strategy | Status |
|-------|-------------|----------------------|--------|
| `RHYTHMIC_DRIVEN` | EDM, hip-hop, dance | Strong beat sync, pulse effects | **Unused** |
| `HARMONIC_DRIVEN` | Jazz, classical, ambient | Chord-aware coloring | **Unused** |
| `MELODIC_DRIVEN` | Vocal pop, ballads | Pitch-following, smooth glides | **Unused** |
| `TEXTURE_DRIVEN` | Ambient, soundscapes | Gradual morphing, no pulses | **Unused** |
| `DYNAMIC_DRIVEN` | Orchestral, builds | Intensity-based brightness | **Unused** |

**Accessor:** `ctx.audio.musicStyle()`

### 2.9 64-Bin Goertzel Spectrum (Detailed Frequency Analysis)

| Accessor | Range | Notes | Status |
|----------|-------|-------|--------|
| `ctx.audio.bin(0-63)` | float (0-1) | A2 (110 Hz) to C8 (4186 Hz) | **Unused** |

**Technical Details:**
- **Algorithm**: Goertzel DFT (not FFT) - computes specific frequencies efficiently
- **Bin spacing**: Musical semitones (12 bins per octave)
- **Variable windows**: Bass bins use longer windows (~1500 samples) for better resolution
- **Windowing**: Hann window via Q15 fixed-point LUT
- Matches Sensory Bridge audio analysis algorithm

### 2.10 Waveform (Time Domain)

| Accessor | Type | Description |
|----------|------|-------------|
| `ctx.audio.getWaveformSample(0-127)` | int16_t | Raw audio samples |

---

## 3. Effect-by-Effect Analysis

### 3.1 LGPAudioTestEffect

**Purpose:** Basic audio pipeline demonstration

**Audio Features Used:**
- `rms()` - Overall energy
- `beatPhase()` - Beat timing
- `isOnBeat()` - Beat detection
- `getBand(0-7)` - Frequency bands

**Visual Pattern:**
- Center-origin radial expansion
- Distance-to-band mapping (center=bass, edges=treble)
- Beat-synchronized brightness pulse
- Fallback time-based animation when audio unavailable

**Audio Mapping Logic:**
```cpp
masterIntensity = 0.3f + (rms * 0.5f) + (beatPulse * 0.2f);
// Maps LED distance to frequency band:
// dist 0-9 → band 0 (bass)
// dist 70-79 → band 7 (treble)
```

**Assessment:** ✅ Solid basic implementation

---

### 3.2 AudioWaveformEffect (ID: 72)

**Purpose:** Time-domain waveform visualization

**Audio Features Used:**
- `getWaveformSample(0-127)` - Raw audio samples
- `chroma[12]` - For color mapping (⚠️ causes flicker)
- `beatPhase()` - Temporal context
- 4-frame history buffer for smoothing

**Visual Pattern:**
- Mirrored waveform from center outward
- Peak envelope with slow decay
- Chromatic coloring from dominant pitch

**Known Issue:**
```cpp
// Line 102 uses fast chroma (causes color flicker):
float colorMix = ctx.audio.controlBus.chroma[c];

// SHOULD use heavy_chroma for smooth color:
float colorMix = ctx.audio.controlBus.heavy_chroma[c];
```

**Assessment:** ⚠️ Good but needs heavy_chroma fix

---

### 3.3 AudioBloomEffect (ID: 73)

**Purpose:** Chromagram-driven scrolling visualization

**Audio Features Used:**
- `heavy_chroma[12]` - Smoothed pitch classes ✅
- `chordState` - For warmth adjustment
- `hop_seq` - Frame synchronization
- Chord type for emotional color offset

**Visual Pattern:**
- Center-origin push-outward scrolling
- Color driven by chromagram + chord mood
- Logarithmic distortion for LGP viewing
- Saturation boost for vibrancy

**Chord-to-Mood Mapping:**
```cpp
// Emotional color shifts based on chord type:
MAJOR: +32 hue offset (warm/orange)
MINOR: -24 hue offset (cool/blue)
DIMINISHED: -32 hue offset (darker)
AUGMENTED: +40 hue offset (ethereal)
```

**Assessment:** ✅ Excellent implementation - best audio mapping

---

### 3.4 LGPChordGlowEffect (ID: 75)

**Purpose:** Full chord detection showcase

**Audio Features Used:**
- `chordState.rootNote` - For hue selection
- `chordState.type` - For mood/saturation
- `chordState.confidence` - For brightness
- Chord intervals (3rd, 5th) for accent positions

**Visual Pattern:**
- Center-origin radial glow
- Root note → hue (chromatic circle)
- Chord type → saturation/mood
- Accent LEDs at 1/3 and 2/3 radius for intervals
- 200ms cross-fade on chord changes
- Ripple burst animation on chord detection

**Mood Configuration:**
```cpp
mood[MAJOR] = {sat:255, offset:0, bright:1.0};
mood[MINOR] = {sat:200, offset:-10, bright:0.85};
mood[DIMINISHED] = {sat:140, offset:15, bright:0.7};
mood[AUGMENTED] = {sat:240, offset:30, bright:0.95};
```

**Assessment:** ✅ Sophisticated and complete

---

## 4. What's Working Well

### Technical Excellence

| Aspect | Evidence |
|--------|----------|
| **CENTER ORIGIN Compliance** | All effects use `SET_CENTER_PAIR()` macro correctly |
| **Thread Safety** | AudioContext passed by-value prevents race conditions |
| **Graceful Degradation** | All effects handle `ctx.audio.available == false` |
| **Frame Sync** | Proper use of `hop_seq` to detect new audio data |
| **Smoothing** | Heavy chroma/bands prevent visual jitter (when used) |
| **LGP Optimization** | Logarithmic distortion, tuned time constants |

### Audio-Visual Mapping Quality

| Effect | Mapping Sophistication |
|--------|----------------------|
| LGPAudioTestEffect | Basic: energy → brightness, bands → position |
| AudioWaveformEffect | Intermediate: waveform → shape, chroma → color |
| AudioBloomEffect | Advanced: chroma → scroll, chords → mood |
| LGPChordGlowEffect | Expert: full harmonic analysis → visual design |

### Performance

- All effects render in <1ms (well under 8.3ms budget for 120 FPS)
- No heap allocations in render loops
- Radial buffers appropriately sized

---

## 5. What's Missing/Incomplete

### Underutilized Features (Available but Unused)

| Feature | Availability | Usage | Gap |
|---------|-------------|-------|-----|
| Percussion Detection | ✅ Full API | ❌ None | High impact opportunity |
| Musical Saliency | ✅ Full API | ❌ None | Could drive adaptive behavior |
| Style Detection | ✅ Full API | ❌ None | Genre-aware rendering possible |
| 64-bin FFT | ✅ Full API | ❌ None | Detailed spectrum unused |
| Onset Triggers | ✅ Full API | ❌ None | Visual punch potential |

### Code Issues

| Issue | Location | Severity | Fix |
|-------|----------|----------|-----|
| Color flicker | `AudioWaveformEffect.cpp:102` | Medium | Change `chroma[]` → `heavy_chroma[]` |
| Static mood values | `LGPChordGlowEffect.cpp` | Low | Could be runtime configurable |
| No error logging | Various | Low | Add validation when audio unavailable |

### Architectural Gaps

1. **No per-effect audio mapping configuration** - Effects hardcode their audio→visual mapping
2. **No saliency-driven effect selection** - System doesn't switch effects based on music type
3. **No genre adaptation** - Same visual response regardless of music style

---

## 6. Recommendations

### Priority 1: Quick Wins (< 1 hour each)

#### 1.1 Fix AudioWaveform Color Flicker
```cpp
// File: src/effects/ieffect/AudioWaveformEffect.cpp
// Line 102: Change from:
float colorMix = ctx.audio.controlBus.chroma[c];
// To:
float colorMix = ctx.audio.controlBus.heavy_chroma[c];
```
**Impact:** Eliminates color flickering in waveform visualization

#### 1.2 Add Percussion Response to AudioBloom
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

### Priority 2: Medium Effort (2-4 hours each)

#### 2.1 Create Percussion-Focused Effect
New effect that specifically visualizes drum patterns:
- Snare → center burst (radius 0-40)
- Kick → outer pulse (radius 60-80)
- Hi-hat → edge shimmer (radius 70-80)
- Creates layered rhythmic visualization

#### 2.2 Add Saliency-Driven Behavior
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

### Priority 3: Major Features (1-2 days each)

#### 3.1 Genre-Adaptive Effect
Single effect that adapts its behavior based on detected music style:
```cpp
switch (ctx.audio.musicStyle()) {
    case RHYTHMIC_DRIVEN:  // EDM, hip-hop
        renderBeatPulse(ctx);  // Strong sync to beat
        break;
    case HARMONIC_DRIVEN:  // Jazz, classical
        renderChordGlow(ctx);  // Smooth chord-aware color
        break;
    case MELODIC_DRIVEN:   // Vocal pop
        renderPitchGlide(ctx); // Follow melody contour
        break;
    case TEXTURE_DRIVEN:   // Ambient
        renderGradualMorph(ctx); // No pulses, slow evolution
        break;
}
```

#### 3.2 64-Bin Spectrum Visualizer
Create detailed frequency display using `ctx.audio.bin(0-63)`:
- Logarithmic binning to match ear perception
- Smooth interpolation between adjacent bins
- Color gradient from bass (warm) to treble (cool)

---

## 7. Implementation Priority Matrix

| Feature | Effort | Impact | Priority |
|---------|--------|--------|----------|
| Fix AudioWaveform flicker | 5 min | Medium | **P1 - Do Now** |
| Add percussion to existing effects | 1-2 hr | High | **P1 - Do Now** |
| Create percussion-focused effect | 3-4 hr | High | P2 - Soon |
| Add saliency-driven behavior | 2-3 hr | Medium | P2 - Soon |
| Genre-adaptive effect | 1-2 days | Very High | P3 - Planned |
| 64-bin spectrum visualizer | 4-6 hr | Medium | P3 - Planned |
| Runtime mood configuration | 2 hr | Low | P4 - Backlog |

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

**End of Document**
