# Chroma Argmax Fix -- Architectural Specification

**Document version:** 1.0
**Date:** 2026-02-21
**Scope:** Firmware v2 audio-reactive effect pipeline
**Status:** Implemented, both build variants verified

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Solution Architecture](#2-solution-architecture)
3. [Implementation Details](#3-implementation-details)
4. [Migration Pattern](#4-migration-pattern)
5. [Effects Inventory](#5-effects-inventory)
6. [Special Cases](#6-special-cases)
7. [Verification](#7-verification)
8. [Remaining Work (Out of Scope)](#8-remaining-work-out-of-scope)
9. [Mathematical Appendix](#9-mathematical-appendix)

---

## 1. Problem Statement

### 1.1 The Argmax Pattern

LightwaveOS derives a "dominant musical pitch" from 12 chroma bins (one per semitone: C, C#, D, ... , B) published by the audio pipeline at hop cadence (50 Hz for the 12.8 kHz build, 125 Hz for the 32 kHz build). Prior to this fix, **26 audio-reactive effects** used an argmax (winner-takes-all) selection to choose a single bin index as the dominant pitch:

```cpp
// Broken pattern: argmax selection
uint8_t dominantBin = 0;
float maxVal = chroma[0];
for (uint8_t i = 1; i < 12; ++i) {
    if (chroma[i] > maxVal) {
        maxVal = chroma[i];
        dominantBin = i;
    }
}
uint8_t hue = dominantBin * 21;  // 256 / 12 ~ 21.25 per bin
```

The argmax selects whichever chroma bin has the single highest magnitude at any given hop. The result is an integer bin index (0--11) which is then mapped to a palette hue (0--255) by multiplying by approximately 21.25.

### 1.2 Why Argmax Causes Visual Discontinuities

When two chroma bins have similar energy levels -- a common occurrence during chord transitions, sustained harmonics, or frequency modulation -- the argmax winner can flip between bins from one hop to the next. A bin flip of `N` semitones produces an instantaneous hue jump of `N * 21.25` units on the 0--255 palette scale.

Consider a common case: the dominant pitch oscillates between bin 3 (D#) and bin 9 (A), a distance of 6 semitones. This produces a hue jump of approximately 128 units -- exactly half the hue wheel. On a dual-strip Light Guide Plate where strip 2 renders at `hue + 128`, a jump of ~128 on strip 1 causes the strips to appear to swap colours, producing a jarring full-frame colour inversion.

Even modest bin flips (1--2 semitones, i.e. 21--42 hue units) are visually disruptive at 50--125 Hz hop rates, as the human eye is extremely sensitive to colour discontinuities across large LED areas.

### 1.3 The Dual-Strip Amplification Problem

LightwaveOS drives two parallel 160-LED WS2812 strips on a Light Guide Plate. The standard colour scheme renders strip 2 at a +128 hue offset from strip 1, creating a complementary colour pair. This design choice amplifies the argmax problem:

- A 128-unit hue jump on strip 1 moves it to where strip 2 was.
- Simultaneously, strip 2 (at `hue + 128`) jumps to where strip 1 was.
- The visual result is a full colour swap between strips -- extremely jarring.

### 1.4 The Secondary Problem: Linear EMA on a Circular Quantity

Several effects attempted to mitigate the argmax discontinuity by applying a **linear exponential moving average** to the bin index:

```cpp
// Broken secondary pattern: linear smooth of circular quantity
float smoothedBin = smoothedBin * 0.88f + dominantBin * 0.12f;
uint8_t hue = static_cast<uint8_t>(smoothedBin * 21.25f);
```

This approach introduces a different but equally severe artefact: **rainbow sweeps at the boundary**. When the dominant bin transitions from 11 (B) to 0 (C) -- a single semitone step in musical pitch -- the linear EMA does not understand that these values are adjacent on a circle. Instead, it interpolates linearly from 11 down through 10, 9, 8, ... 1, 0, sweeping through the entire hue wheel. The visual result is a full rainbow cascade every time the dominant pitch crosses the B/C boundary.

The ChevronWaves effect family was considered the "gold standard" for audio reactivity prior to this audit, yet it contained exactly this linear-EMA-on-circular-quantity defect (smoothing a bin index `m_dominantBinSmooth` linearly).

### 1.5 Scope of the Problem

**26 effects** were affected, representing the majority of audio-reactive effects in the system. These effects collectively account for the entire musically-driven colour selection path in LightwaveOS. The bug was systemic: it was embedded in shared utility functions (`dominantChromaBin12()`, `chromaBinToHue()`) that were copy-pasted across multiple files, as well as in inline argmax loops within individual effect render functions.

---

## 2. Solution Architecture

### 2.1 Circular Weighted Mean

The fix replaces the discrete argmax with a **circular weighted mean**. Rather than selecting a single bin, the algorithm treats the 12 chroma bins as weights on a unit circle (spaced at 30-degree intervals) and computes the angle of the resultant vector:

```
theta = atan2( sum(w_i * sin(2*pi*i/12)), sum(w_i * cos(2*pi*i/12)) )
```

This produces a continuous angle in [0, 2*pi) that varies smoothly as the chroma energy distribution shifts. There is no discontinuity when the dominant bin changes -- the angle simply tracks the centre of mass of the chroma distribution.

### 2.2 Precomputed Sin/Cos Lookup Table

Computing `sin(2*pi*i/12)` and `cos(2*pi*i/12)` for i = 0..11 would require 24 trigonometric evaluations per frame per effect. Since these values are fixed (12 equally-spaced positions), they are precomputed as `constexpr` lookup tables:

```cpp
static constexpr float kCos[12] = {
     1.000000f,  0.866025f,  0.500000f,  0.000000f, -0.500000f, -0.866025f,
    -1.000000f, -0.866025f, -0.500000f,  0.000000f,  0.500000f,  0.866025f
};
static constexpr float kSin[12] = {
     0.000000f,  0.500000f,  0.866025f,  1.000000f,  0.866025f,  0.500000f,
     0.000000f, -0.500000f, -0.866025f, -1.000000f, -0.866025f, -0.500000f
};
```

The per-frame cost is reduced to 24 multiply-accumulates (for the weighted sum), one `atan2f()`, and one `expf()` -- a negligible cost on the ESP32-S3.

### 2.3 Circular EMA Temporal Smoother

Even with the circular weighted mean, the instantaneous angle can shift noticeably between frames when the chroma distribution changes rapidly (e.g. during a chord change or transient). A **circular exponential moving average** provides temporal smoothing:

```
theta_smooth = theta_prev + alpha * shortest_arc(theta_new, theta_prev)
```

where `shortest_arc` wraps the difference to [-pi, pi], ensuring the EMA always interpolates through the **shorter path** around the hue circle. This completely eliminates the rainbow-sweep artefact that plagued the linear EMA approach.

### 2.4 The Shortest-Arc Principle

The circular EMA always takes the shorter of the two possible arcs between the new angle and the previous angle. If the difference exceeds pi radians (180 degrees), it wraps through the opposite direction. This guarantees:

- A 1-semitone step (30 degrees) is smoothed as a 30-degree arc, not a 330-degree sweep.
- The 11-to-0 bin transition (B to C) is smoothed as a 30-degree arc, not a 330-degree sweep in the opposite direction.
- No hue path ever exceeds 180 degrees, regardless of the chroma distribution.

### 2.5 Time Constant and Frame-Rate Independence

The EMA alpha is derived from delta time and a configurable time constant (tau):

```
alpha = 1 - exp(-dt / tau)
```

This formulation is frame-rate independent: the smoothing behaviour is identical whether the effect runs at 60 fps, 120 fps, or any other rate. The default time constant is **tau = 0.20 seconds**, which provides:

- Fast enough to track chord changes within approximately 200 ms.
- Slow enough to eliminate per-hop jitter from competing chroma bins.
- The tau parameter is exposed as an argument to `circularChromaHueSmoothed()`, allowing effects to tune responsiveness (0.12f = reactive, 0.25f = smooth, 0.40f = very stable).

### 2.6 The dtDecay() Utility

A secondary fix created the `dtDecay()` utility for frame-rate-independent exponential decay:

```cpp
static inline float dtDecay(float value, float rate60fps, float dt) {
    return value * powf(rate60fps, dt * 60.0f);
}
```

This utility was created to address a related class of bugs (bare `value *= 0.88f` patterns without dt-correction), but is **not yet applied** to the 19 effects that need it. It is included in `ChromaUtils.h` for future use.

---

## 3. Implementation Details

### 3.1 ChromaUtils.h API

The complete fix is contained in a single header file at:

```
firmware/v2/src/effects/ieffect/ChromaUtils.h
```

**Namespace:** `lightwaveos::effects::chroma`

#### Functions

| Function | Purpose | Per-frame cost |
|----------|---------|----------------|
| `circularChromaHue(chroma[12])` | Instantaneous circular weighted mean. Returns `uint8_t` hue (0--255). No temporal smoothing. | 1x `atan2f` |
| `circularChromaHueSmoothed(chroma[12], &prevAngle, dt, tau)` | Circular weighted mean + circular EMA. Returns `uint8_t` hue (0--255). **This is the primary API.** | 1x `atan2f` + 1x `expf` |
| `circularEma(newAngle, prevAngle, alpha)` | Low-level circular EMA on arbitrary radians. Shortest-arc interpolation. | Arithmetic only |
| `dtDecay(value, rate60fps, dt)` | Frame-rate-independent exponential decay. Created for future use. | 1x `powf` |

#### Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `kCos[12]` | Precomputed cos(i * 2*pi/12) | Lookup table for weighted sum |
| `kSin[12]` | Precomputed sin(i * 2*pi/12) | Lookup table for weighted sum |
| `TWO_PI_F` | 6.2831853f | 2*pi |
| `PI_F` | 3.1415927f | pi |

### 3.2 Per-Effect State Requirement

Each effect that uses `circularChromaHueSmoothed()` must maintain a persistent `float` member for the circular EMA state:

```cpp
// In effect header (.h):
float m_chromaAngle = 0.0f;  // Circular EMA state for ChromaUtils (radians)

// In effect init():
m_chromaAngle = 0.0f;

// In effect render():
uint8_t hue = effects::chroma::circularChromaHueSmoothed(
    ctx.audio.controlBus.chroma, m_chromaAngle, rawDt, 0.20f);
```

The `m_chromaAngle` parameter is modified in place by each call and stores the smoothed angle in radians [0, 2*pi). It must be initialised to `0.0f` in `init()` and must not be reset between frames.

### 3.3 Include Path

Because `ChromaUtils.h` resides in the same directory as all `ieffect` source files, effects include it with a bare relative path:

```cpp
#include "ChromaUtils.h"
```

### 3.4 Chroma Source Selection

Different effects use different chroma sources from the ControlBus, depending on their smoothing requirements:

| Source | Path | Character |
|--------|------|-----------|
| `ctx.audio.controlBus.chroma` | Standard chroma | Moderately smoothed, suitable for most effects |
| `ctx.audio.controlBus.heavy_chroma` | Heavy (extra-smoothed) chroma | Additional attack/release smoothing, better for effects that need maximum stability |

The Enhanced effect variants (LGP Wave Collision Enhanced, LGP Interference Scanner Enhanced, etc.) typically use `heavy_chroma` for additional stability, while base effects use standard `chroma`.

---

## 4. Migration Pattern

Three distinct broken patterns were identified and replaced across the 26 affected effects.

### 4.1 Pattern A: Static Utility Functions (dominantChromaBin12 + chromaBinToHue)

**Before:**

Several files contained duplicated static utility functions:

```cpp
// Found duplicated in 6+ files
static uint8_t dominantChromaBin12(const float chroma[12]) {
    uint8_t best = 0;
    float maxVal = chroma[0];
    for (uint8_t i = 1; i < 12; ++i) {
        if (chroma[i] > maxVal) {
            maxVal = chroma[i];
            best = i;
        }
    }
    return best;
}

static uint8_t chromaBinToHue(uint8_t bin) {
    return bin * (255 / 12);  // ~21.25 per bin
}

// Usage in render():
uint8_t hue = chromaBinToHue(dominantChromaBin12(chroma));
```

**After:**

```cpp
#include "ChromaUtils.h"

// In header: add m_chromaAngle member
float m_chromaAngle = 0.0f;

// In render():
uint8_t hue = effects::chroma::circularChromaHueSmoothed(
    ctx.audio.controlBus.chroma, m_chromaAngle, rawDt, 0.20f);
```

**Changes:**
- Removed duplicated `dominantChromaBin12()` and `chromaBinToHue()` functions.
- Added `#include "ChromaUtils.h"`.
- Added `float m_chromaAngle = 0.0f;` member to effect class.
- Replaced all call sites with single `circularChromaHueSmoothed()` call.

### 4.2 Pattern B: Inline Argmax + Linear EMA (m_dominantBinSmooth)

**Before:**

```cpp
// In render():
uint8_t dominantBin = 0;
float maxVal = chroma[0];
for (uint8_t i = 1; i < 12; ++i) {
    if (chroma[i] > maxVal) { maxVal = chroma[i]; dominantBin = i; }
}
m_dominantBinSmooth = m_dominantBinSmooth * 0.88f + dominantBin * 0.12f;
uint8_t hue = static_cast<uint8_t>(m_dominantBinSmooth * 21.25f);
```

**After:**

```cpp
uint8_t hue = effects::chroma::circularChromaHueSmoothed(
    ctx.audio.controlBus.chroma, m_chromaAngle, rawDt, 0.20f);
```

**Changes:**
- Removed `m_dominantBinSmooth` member variable (linear EMA state).
- Removed inline argmax loop.
- Added `float m_chromaAngle = 0.0f;` member (circular EMA state).
- Single function call replaces both the argmax AND the linear smooth.
- The ChevronWaves and ChevronWavesEnhanced effects were the canonical examples of this pattern.

### 4.3 Pattern C: Raw Inline Argmax (No Smoothing)

**Before:**

```cpp
// In render() -- worst case, every hop produces a potential hue jump
uint8_t maxBin = 0;
for (uint8_t i = 1; i < 12; ++i) {
    if (chroma[i] > chroma[maxBin]) maxBin = i;
}
uint8_t hue = maxBin * 21;  // immediate use, no smoothing at all
```

**After:**

```cpp
uint8_t hue = effects::chroma::circularChromaHueSmoothed(
    ctx.audio.controlBus.chroma, m_chromaAngle, rawDt, 0.20f);
```

**Changes:**
- Removed inline argmax loop.
- Added `float m_chromaAngle = 0.0f;` member.
- `circularChromaHueSmoothed()` provides both the circular mean AND temporal smoothing in one call.

---

## 5. Effects Inventory

### 5.1 Summary

| Pattern | Count | Description |
|---------|-------|-------------|
| A | 6 | Static utility functions (dominantChromaBin12 + chromaBinToHue) |
| B | 4 | Inline argmax + linear EMA (m_dominantBinSmooth or similar) |
| C | 6 | Raw inline argmax (no smoothing) |
| D | 10 | LGP Experimental Audio Pack (selectMusicalHue with Schmitt trigger + smoothHue/smoothNoteCircular) |
| -- | **26** | **Total effects modified** |

### 5.2 Pattern A Effects (Static Utility Functions Removed)

| Effect Name | ID | Files Modified | Notes |
|-------------|-----|----------------|-------|
| Ripple | 8 | `RippleEffect.cpp`, `RippleEffect.h` | Added `m_chromaAngle`. Uses `ctx.audio.controlBus.chroma`. |
| Ripple Enhanced | 97 | `RippleEnhancedEffect.cpp`, `RippleEnhancedEffect.h` | Added `m_chromaAngle`. Uses `ctx.audio.controlBus.chroma`. Keeps chromagram AsymmetricFollower smoothing. |
| Ripple (ES tuned) | 106 | `RippleEsTunedEffect.cpp`, `RippleEsTunedEffect.h` | Added `m_chromaAngle`. Uses `ctx.audio.controlBus.chroma`. |
| LGP Beat Pulse | 69 | `LGPBeatPulseEffect.cpp`, `LGPBeatPulseEffect.h` | Added `m_chromaAngle`. Uses `selectChroma12()` helper for backend-agnostic chroma selection. |
| LGP Bass Breath | 71 | `LGPBassBreathEffect.cpp`, `LGPBassBreathEffect.h` | Added `m_chromaAngle`. Uses `selectChroma12()`. Tau set to 0.35f (more stable for breathing effect). |
| LGP Spectrum Bars | 70 | `LGPSpectrumBarsEffect.cpp`, `LGPSpectrumBarsEffect.h` | Added `m_chromaAngle`. Uses `selectChroma12()`. |

### 5.3 Pattern B Effects (Linear EMA Replaced)

| Effect Name | ID | Files Modified | Removed Members | Notes |
|-------------|-----|----------------|-----------------|-------|
| Chevron Waves | 18 | `ChevronWavesEffect.cpp`, `ChevronWavesEffect.h` | `m_dominantBin` kept for energy calc; `m_chromaAngle` + `m_chromaHue` added | Was considered "gold standard" -- still had linear EMA on circular quantity. |
| Chevron Waves Enhanced | 90 | `ChevronWavesEffectEnhanced.cpp`, `ChevronWavesEffectEnhanced.h` | `m_dominantBin` kept for energy calc; `m_chromaAngle` + `m_chromaHue` added | Enhanced variant; uses `heavy_chroma`. Includes chromagram AsymmetricFollower per-bin smoothing. |
| LGP Wave Collision | 17 | `LGPWaveCollisionEffect.cpp`, `LGPWaveCollisionEffect.h` | `m_chromaAngle` replaces chroma-to-hue pipeline | Uses `ctx.audio.controlBus.chroma`. |
| LGP Wave Collision Enhanced | 96 | `LGPWaveCollisionEffectEnhanced.cpp`, `LGPWaveCollisionEffectEnhanced.h` | `m_chromaAngle` replaces chroma-to-hue pipeline | Uses `heavy_chroma`. Includes chromagram AsymmetricFollower per-bin smoothing. |

### 5.4 Pattern C Effects (Raw Argmax Replaced)

| Effect Name | ID | Files Modified | Notes |
|-------------|-----|----------------|-------|
| Juggle | 5 | `JuggleEffect.cpp`, `JuggleEffect.h` | Added `m_chromaAngle`. Uses `ctx.audio.controlBus.chroma`. |
| BPM Enhanced | 88 | `BPMEnhancedEffect.cpp`, `BPMEnhancedEffect.h` | Added `m_chromaAngle`. Uses smoothed chroma via per-bin AsymmetricFollowers fed from `heavy_chroma`. |
| LGP Star Burst | 24 | `LGPStarBurstEffect.cpp`, `LGPStarBurstEffect.h` | Added `m_chromaAngle`. Uses `ctx.audio.controlBus.chroma`. |
| LGP Star Burst Enhanced | 95 | `LGPStarBurstEffectEnhanced.cpp`, `LGPStarBurstEffectEnhanced.h` | Added `m_chromaAngle`. Uses `heavy_chroma`. |
| LGP Photonic Crystal | 33 | `LGPPhotonicCrystalEffect.cpp`, `LGPPhotonicCrystalEffect.h` | Added `m_chromaAngle`. Uses `ctx.audio.controlBus.chroma`. |
| LGP Photonic Crystal Enhanced | 92 | `LGPPhotonicCrystalEffectEnhanced.cpp`, `LGPPhotonicCrystalEffectEnhanced.h` | Added `m_chromaAngle`. Uses `heavy_chroma`. |

### 5.5 Pattern D Effects (LGP Experimental Audio Pack)

The LGP Experimental Audio Pack (IDs 152--161) uses a different architecture: a shared `selectMusicalHue()` function with Schmitt trigger hysteresis and circular hue smoothing (`smoothHue`). This pack was written with the circular chroma fix already incorporated via the `dominantNoteFromBins()` function, which internally uses `effects::chroma::kCos` / `effects::chroma::kSin` for circular weighted mean over 12 note positions. Hue smoothing uses `smoothHue()` which wraps correctly around the 0--255 boundary.

| Sub-Effect Name | ID | Chroma Method | Notes |
|-----------------|-----|---------------|-------|
| LGP Flux Rift | 152 | `selectMusicalHue()` + `smoothHue()` | Schmitt trigger on chord confidence. |
| LGP Beat Prism | 153 | `selectMusicalHue()` + `smoothHue()` | +8 hue offset from base. |
| LGP Harmonic Tide | 154 | `dominantNoteFromBins()` + `smoothNoteCircular()` | Special case: smooths note index circularly, not hue directly. See Section 6.2. |
| LGP Bass Quake | 155 | `selectMusicalHue()` + `smoothHue()` | +10 hue offset. |
| LGP Treble Net | 156 | `selectMusicalHue()` + `smoothHue()` | +116 hue offset. |
| LGP Rhythmic Gate | 157 | `selectMusicalHue()` + `smoothHue()` | +30 hue offset. |
| LGP Spectral Knot | 158 | `selectMusicalHue()` + `smoothHue()` | +44 hue offset. |
| LGP Saliency Bloom | 159 | `selectMusicalHue()` + `smoothHue()` | +14 hue offset. |
| LGP Transient Lattice | 160 | `selectMusicalHue()` + `smoothHue()` | +62 hue offset. |
| LGP Wavelet Mirror | 161 | `selectMusicalHue()` + `smoothHue()` | +30 hue offset. |

### 5.6 Additional Effects Using ChromaUtils (Non-Pattern-A/B/C)

| Effect Name | ID | Usage | Notes |
|-------------|-----|-------|-------|
| LGP Interference Scanner | 16 | `circularChromaHueSmoothed()` | Uses `heavy_chroma`. |
| LGP Interference Scanner Enhanced | 91 | `circularChromaHueSmoothed()` | Uses `heavy_chroma`. |
| LGP Holographic (ES tuned) | 108 | `circularChromaHueSmoothed()` | Derives base hue from persisted `m_chromaAngle`; decays angle when audio absent. |
| LGP Perlin Interference Weave | 80 | `circularChromaHueSmoothed()` | Tau set to 0.30f for Perlin-appropriate stability. |
| Heartbeat (ES tuned) | 107 | `circularChromaHueSmoothed()` | Tau 0.25f. Decays `m_chromaAngle` when audio absent (dt-corrected). |
| Audio Waveform | 72 | `circularChromaHueSmoothed()` | Uses smoothed chroma from waveform pipeline. |
| LGP Star Burst (Narrative) | 74 | `circularEma()` directly | Uses `circularEma()` for key root bin smoothing, not `circularChromaHueSmoothed()`. See Section 6.3. |

---

## 6. Special Cases

### 6.1 LGP Experimental Audio Pack: Schmitt Trigger Hysteresis

The 10 sub-effects in `LGPExperimentalAudioPack.cpp` (IDs 152--161) use a two-tier hue selection strategy via the shared `selectMusicalHue()` function:

1. **Chord-detection path** (high confidence): When chord detection confidence >= 0.40, use the chord root note from the chord detector as the hue source.
2. **Bin-derived path** (low confidence): When chord detection confidence <= 0.25, fall back to `dominantNoteFromBins()` which uses circular weighted mean over `bins64Adaptive` data.

The transition between these two paths uses a **Schmitt trigger** with:
- **Enter threshold:** 0.40 (confidence must exceed 0.40 to switch to chord root)
- **Exit threshold:** 0.25 (confidence must drop below 0.25 to revert to bin-derived)

This hysteresis band of 0.15 prevents rapid toggling between the two hue sources when chord confidence hovers around a threshold. Prior to this fix, a hard threshold at 0.35 with no hysteresis caused the hue source to flicker between chord-root and bin-derived paths.

The `dominantNoteFromBins()` function itself uses `effects::chroma::kCos` and `effects::chroma::kSin` from `ChromaUtils.h` to compute a circular weighted mean over accumulated note scores:

```cpp
float cx = 0.0f, sy = 0.0f;
for (uint8_t i = 0; i < 12; ++i) {
    cx += scores[i] * effects::chroma::kCos[i];
    sy += scores[i] * effects::chroma::kSin[i];
}
float angle = atan2f(sy, cx);
```

### 6.2 LGP Harmonic Tide: Circular Smoothing in Note Domain

The Harmonic Tide sub-effect (ID 154) is unique among the pack: rather than smoothing in the hue domain (0--255), it smooths in the **note index domain** (0--11) using `smoothNoteCircular()`:

```cpp
static inline float smoothNoteCircular(float current, float target, float dt, float tauS) {
    float delta = target - current;
    while (delta > 6.0f) delta -= 12.0f;
    while (delta < -6.0f) delta += 12.0f;
    const float next = current + delta * expAlpha(dt, tauS);
    float wrapped = fmodf(next, 12.0f);
    if (wrapped < 0.0f) wrapped += 12.0f;
    return wrapped;
}
```

This uses the same shortest-arc principle as `circularEma()`, but with a period of 12 instead of 2*pi. The smoothed note index is then mapped to hue via the `NOTE_HUES[12]` lookup table which provides perceptually-spaced hue values for each musical note.

### 6.3 LGP Star Burst (Narrative): Direct circularEma() Usage

The Narrative variant (ID 74) does not use `circularChromaHueSmoothed()`. Instead, it uses the lower-level `circularEma()` directly to smooth a key root bin angle:

```cpp
const float keyAlpha = expAlpha(dtAudio, 0.35f);
const float targetAngle = static_cast<float>(m_keyRootBin) *
    (effects::chroma::TWO_PI_F / 12.0f);
m_keyRootAngle = effects::chroma::circularEma(
    targetAngle, m_keyRootAngle, keyAlpha);
```

This effect has a phrase-gated narrative state machine (REST/BUILD/HOLD/RELEASE) that commits chord changes at phrase boundaries rather than continuously. The `circularEma()` smooths the transition when a new key root is committed, preventing the rainbow sweep that would occur with linear interpolation from bin 11 to bin 0.

### 6.4 Chevron Waves: The "Gold Standard" Fallacy

ChevronWavesEffect (ID 18) and ChevronWavesEnhancedEffect (ID 90) were previously considered the highest-quality audio-reactive effects in the system. However, the audit revealed that both contained the Pattern B defect: a linear EMA (`m_dominantBinSmooth`) on the circular bin index quantity. The visual consequence was that chord changes crossing the B/C boundary triggered a full rainbow sweep through all 12 hue positions.

The fix upgraded both to use `circularChromaHueSmoothed()`, maintaining the existing per-bin AsymmetricFollower chromagram smoothing and energy tracking while replacing only the hue derivation path.

### 6.5 Audio-Absent Decay

Several effects implement a graceful hue decay when audio becomes unavailable. Two approaches are used:

1. **HeartbeatEsTunedEffect** and **LGPHolographicEsTunedEffect**: Decay `m_chromaAngle` toward zero using dt-corrected multiplication:
   ```cpp
   m_chromaAngle *= powf(0.995f, rawDt * 60.0f);
   ```
   This causes the hue to drift slowly back to a default value rather than freezing at the last audio-driven hue.

2. **Most other effects**: Simply freeze `m_chromaAngle` at its last value and use `ctx.gHue` (the global rotating hue) as a fallback.

---

## 7. Verification

### 7.1 Build Verification

Both build variants compile successfully after the fix:

| Build Environment | Sample Rate | Hop Rate | Status |
|-------------------|-------------|----------|--------|
| `esp32dev_audio_esv11` | 32 kHz | 125 Hz | Compiles successfully |
| `esp32dev_audio` | 12.8 kHz | 50 Hz | Compiles successfully |

### 7.2 Flash Impact

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| Flash usage | ~32.1% of 6.5 MB | ~32.1% of 6.5 MB | +5.5 KB (negligible) |

The flash increase comes from:
- `ChromaUtils.h` inlined functions (small: one `atan2f` call site per effect, plus the 12-entry lookup tables, which are `static constexpr` and shared across compilation units).
- Removal of duplicated `dominantChromaBin12()` / `chromaBinToHue()` functions partially offsets the increase.

### 7.3 RAM Impact

| Metric | Per Effect | Total (26 effects) |
|--------|------------|-------------------|
| `m_chromaAngle` (float) | 4 bytes | 104 bytes |
| Removed `m_dominantBinSmooth` (where applicable) | -4 bytes | ~-16 bytes |
| Net additional DRAM | ~4 bytes avg | ~88 bytes |

Some effects added `m_chromaAngle` while also retaining `m_dominantBin` for energy calculations, and some added `m_chromaHue` as an intermediate. The worst-case total additional DRAM consumption is approximately **208 bytes** (26 effects x 8 bytes).

### 7.4 Per-Frame CPU Cost

| Operation | Count per frame | Cost |
|-----------|----------------|------|
| `atan2f()` | 1 per active effect | ~150 cycles on ESP32-S3 |
| `expf()` | 1 per active effect | ~100 cycles on ESP32-S3 |
| Multiply-accumulate (12 bins) | 24 per active effect | ~24 cycles |

At 120 fps with approximately 3 active effects (typical for zone-based rendering), the total additional cost is approximately **360 trig calls per second**, consuming roughly **100,000 cycles/second** out of the ESP32-S3's 240 MHz budget (~0.04% CPU). This is negligible.

The previous argmax pattern required zero trig calls but had O(12) comparisons per effect per frame -- comparable to the new pattern's O(12) multiply-accumulate loop.

---

## 8. Remaining Work (Out of Scope)

The following defects were identified during the same audit session but are **not addressed** by this fix. They are documented here for future reference.

### 8.1 Bug Class B: Per-Frame Decay Without dt-Correction (19 Effects)

**Pattern:** `value *= 0.88f` (or similar constant) applied every frame without accounting for delta time. At 120 fps, this decays faster than at 60 fps.

**Fix:** Replace with `value = dtDecay(value, 0.88f, dt)` using the utility created in `ChromaUtils.h`.

**Affected effects:** 19 effects across the codebase. The `dtDecay()` utility was created for this purpose but has not yet been applied.

### 8.2 BeatPulseRenderUtils.h addWhiteSaturating Overflow

**Pattern:** `uint8_t` arithmetic overflow in the `addWhiteSaturating()` function causes colour values to wrap from 255 to 0, producing momentary colour pops during beat events.

**Fix:** Use saturating addition (e.g., `qadd8()` from FastLED) or clamp before cast.

### 8.3 Perlin Noise Advection dt-Correction (5 Effects)

**Pattern:** Perlin noise coordinate stepping uses `m_noiseX += speed * 0.5f` without dt scaling, causing noise advection speed to vary with frame rate.

**Fix:** Multiply noise step by `dt * 60.0f` to normalise to 60 fps equivalent.

**Affected effects:** LGP Perlin Veil, LGP Perlin Shocklines, LGP Perlin Caustics, LGP Perlin Interference Weave, and their ambient variants.

### 8.4 LGPBeatPulseEffect m_smoothBeatPhase

**Pattern:** Linear smooth of a wrapping [0, 1) value (`beatPhase`). When beat phase wraps from ~0.99 to ~0.01, the linear smooth sweeps backward through the entire range.

**Fix:** Apply circular smoothing with period 1.0 (same shortest-arc principle as `circularEma`, but in [0, 1) domain).

### 8.5 LGPAudioTestEffect

**Pattern:** Unsmoothed band data drives LED brightness directly, causing visible flicker at hop boundaries.

**Fix:** Apply AsymmetricFollower or similar smoothing to band values before mapping to brightness.

### 8.6 SbWaveform310RefEffect

**Pattern:** Per-hop follower constants are not dt-corrected, causing smoothing behaviour to vary between the 50 Hz and 125 Hz hop rate builds.

**Fix:** Convert follower alpha values to tau-based formulation with dt-independence.

---

## 9. Mathematical Appendix

### 9.1 Circular Weighted Mean

Given 12 chroma bin magnitudes `w_0, w_1, ..., w_11`, map each bin to a position on the unit circle at angle `theta_i = 2*pi*i / 12`:

```
C = sum_{i=0}^{11} w_i * cos(theta_i)
S = sum_{i=0}^{11} w_i * sin(theta_i)

theta = atan2(S, C)

if theta < 0:
    theta += 2*pi
```

The result `theta` is in [0, 2*pi) and represents the angle of the weighted centroid of the chroma distribution on the circle. If all energy is concentrated in a single bin `k`, then `theta = 2*pi*k / 12` exactly. If energy is spread across adjacent bins, `theta` falls between them proportionally.

**Hue conversion:** `hue = floor(theta * 255 / (2*pi))`

### 9.2 Circular EMA (Shortest-Arc Interpolation)

Given a new angle `theta_new`, a previous smoothed angle `theta_prev`, and a smoothing factor `alpha`:

```
delta = theta_new - theta_prev

// Wrap to shortest arc [-pi, pi]
if delta > pi:
    delta -= 2*pi
if delta < -pi:
    delta += 2*pi

theta_smooth = theta_prev + alpha * delta

// Wrap result to [0, 2*pi)
if theta_smooth < 0:
    theta_smooth += 2*pi
if theta_smooth >= 2*pi:
    theta_smooth -= 2*pi
```

The key insight is the wrapping of `delta` to [-pi, pi]. This ensures the interpolation always takes the **shorter** of the two possible arcs around the circle. Without this wrapping, interpolation from bin 11 (theta ~ 5.76 rad) to bin 0 (theta ~ 0 rad) would sweep backward through 5.76 radians (330 degrees) instead of forward through 0.52 radians (30 degrees).

### 9.3 EMA Alpha from Time Constant

The smoothing factor `alpha` is derived from the time constant `tau` (in seconds) and the frame delta time `dt` (in seconds):

```
alpha = 1 - exp(-dt / tau)
```

Properties:
- When `dt << tau`: `alpha ~ dt / tau` (small steps, heavy smoothing).
- When `dt >> tau`: `alpha -> 1` (snap to new value).
- Frame-rate independent: the same `tau` produces the same smoothing behaviour regardless of frame rate.
- `tau = 0.20s` means the value reaches ~63.2% of a step change in 200 ms.

### 9.4 dt-Corrected Exponential Decay

Given a per-frame decay rate `r` calibrated at 60 fps, the dt-corrected version is:

```
value_new = value_old * r^(dt * 60)
```

This ensures that the total decay over any time interval is constant regardless of frame rate:
- At 60 fps (dt = 1/60): `value *= r^1 = r` (original behaviour).
- At 120 fps (dt = 1/120): `value *= r^0.5` (square root of r per frame, but applied twice as often, yielding the same result over 1 second).
- At 30 fps (dt = 1/30): `value *= r^2` (squared per frame, but applied half as often).

### 9.5 Hue Domain Sizes

| Domain | Range | Period | Used By |
|--------|-------|--------|---------|
| Radians (circular mean) | [0, 2*pi) | 2*pi | `circularChromaHue()`, `circularEma()`, `m_chromaAngle` |
| Palette hue | [0, 255] | 256 (uint8_t wrapping) | All effect render paths, `ctx.palette.getColor()` |
| Note index | [0, 12) | 12 | `smoothNoteCircular()`, `NOTE_HUES[]` lookup |
| Bin index (legacy) | [0, 11] integer | 12 | **Eliminated** (was the argmax output) |

The conversion from radians to palette hue is: `hue = theta * 255 / (2*pi)`.

---

## Appendix A: File Manifest

### New Files

| File | Purpose |
|------|---------|
| `firmware/v2/src/effects/ieffect/ChromaUtils.h` | Shared circular chroma utilities (the core of this fix) |

### Modified Files (Headers -- m_chromaAngle Added)

| File | Effect ID |
|------|-----------|
| `firmware/v2/src/effects/ieffect/JuggleEffect.h` | 5 |
| `firmware/v2/src/effects/ieffect/RippleEffect.h` | 8 |
| `firmware/v2/src/effects/ieffect/LGPInterferenceScannerEffect.h` | 16 |
| `firmware/v2/src/effects/ieffect/LGPWaveCollisionEffect.h` | 17 |
| `firmware/v2/src/effects/ieffect/ChevronWavesEffect.h` | 18 |
| `firmware/v2/src/effects/ieffect/LGPStarBurstEffect.h` | 24 |
| `firmware/v2/src/effects/ieffect/LGPPhotonicCrystalEffect.h` | 33 |
| `firmware/v2/src/effects/ieffect/LGPBeatPulseEffect.h` | 69 |
| `firmware/v2/src/effects/ieffect/LGPSpectrumBarsEffect.h` | 70 |
| `firmware/v2/src/effects/ieffect/LGPBassBreathEffect.h` | 71 |
| `firmware/v2/src/effects/ieffect/AudioWaveformEffect.h` | 72 |
| `firmware/v2/src/effects/ieffect/LGPPerlinInterferenceWeaveEffect.h` | 80 |
| `firmware/v2/src/effects/ieffect/BPMEnhancedEffect.h` | 88 |
| `firmware/v2/src/effects/ieffect/ChevronWavesEffectEnhanced.h` | 90 |
| `firmware/v2/src/effects/ieffect/LGPInterferenceScannerEffectEnhanced.h` | 91 |
| `firmware/v2/src/effects/ieffect/LGPPhotonicCrystalEffectEnhanced.h` | 92 |
| `firmware/v2/src/effects/ieffect/LGPStarBurstEffectEnhanced.h` | 95 |
| `firmware/v2/src/effects/ieffect/LGPWaveCollisionEffectEnhanced.h` | 96 |
| `firmware/v2/src/effects/ieffect/RippleEnhancedEffect.h` | 97 |
| `firmware/v2/src/effects/ieffect/RippleEsTunedEffect.h` | 106 |
| `firmware/v2/src/effects/ieffect/HeartbeatEsTunedEffect.h` | 107 |
| `firmware/v2/src/effects/ieffect/LGPHolographicEsTunedEffect.h` | 108 |

### Modified Files (Headers -- Indirect ChromaUtils Usage)

| File | Effect ID | Notes |
|------|-----------|-------|
| `firmware/v2/src/effects/ieffect/LGPStarBurstNarrativeEffect.h` | 74 | Uses `m_keyRootAngle` for `circularEma()` |
| `firmware/v2/src/effects/ieffect/LGPExperimentalAudioPack.h` | 152--161 | Uses `m_chordGateOpen` + `m_hue` (circular hue smoothing via `smoothHue`) |

### Modified Files (Source -- Render Path Updated)

All 22 `.cpp` files corresponding to the headers above, plus:

| File | Notes |
|------|-------|
| `firmware/v2/src/effects/ieffect/LGPExperimentalAudioPack.cpp` | `dominantNoteFromBins()` uses `kCos`/`kSin` from ChromaUtils; `selectMusicalHue()` uses Schmitt trigger |
| `firmware/v2/src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp` | Uses `circularEma()` for key root smoothing |

---

## Appendix B: Glossary

| Term | Definition |
|------|-----------|
| **Argmax** | Selection of the index with the maximum value from an array. Produces a discrete integer output. |
| **Chroma bin** | One of 12 frequency-domain energy measurements, each corresponding to a musical pitch class (C, C#, D, ..., B). |
| **Circular weighted mean** | The angle of the resultant vector when chroma magnitudes are treated as weights on equally-spaced positions around a unit circle. |
| **Circular EMA** | Exponential moving average that respects circular topology by always interpolating through the shorter arc. |
| **Shortest arc** | The shorter of two possible paths between two angles on a circle. Never exceeds 180 degrees. |
| **Hop** | A single audio analysis frame. At 12.8 kHz sample rate with 256-sample hops, this occurs at 50 Hz. At 32 kHz with 256-sample hops, 125 Hz. |
| **heavy_chroma** | Extra-smoothed chroma values from the ControlBus, with slower attack/release than standard chroma. Preferred by Enhanced effect variants. |
| **Schmitt trigger** | A hysteresis-based threshold that uses separate enter and exit thresholds to prevent oscillation near the boundary. |
| **Tau (time constant)** | The time in seconds for an EMA to reach ~63.2% of a step change. Higher tau = slower/smoother response. |
| **dt-corrected** | A computation that accounts for variable frame timing to produce consistent behaviour regardless of frame rate. |
| **LGP** | Light Guide Plate. The physical substrate for the LED strips. Used as a prefix for effects designed for dual-strip LGP rendering. |
