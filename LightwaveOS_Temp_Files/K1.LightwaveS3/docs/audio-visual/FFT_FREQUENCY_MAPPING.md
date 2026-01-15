# FFT Frequency Mapping

**Status:** ✅ IMPLEMENTED (Phase B Complete)
**Last Updated:** 2026-01-07

## Overview

LightwaveOS v2 provides **64-bin FFT analysis** via `ctx.audio.bin(index)` in effect rendering.
Bins are **semitone-spaced** from 55 Hz (A1) to 2093 Hz (C7), optimized for musical content.

The FFT is computed by `GoertzelAnalyzer` using adaptive window sizes and Hann windowing,
then exposed through `ControlBusFrame.bins64[]` to effects via `EffectContext::AudioContext`.

## Bin-to-Frequency Table

| Bin Index | Frequency (Hz) | Musical Note | Range Description |
|-----------|----------------|--------------|-------------------|
| 0-7       | 55-82          | A1-E2        | Sub-bass / kick drum fundamentals |
| 8-15      | 87-123         | F2-B2        | Bass / bass guitar |
| 16-23     | 131-185        | C3-F#3       | Low-mids / male vocals |
| 24-31     | 196-277        | G3-C#4       | Mids / snare fundamentals |
| 32-39     | 294-415        | D4-G#4       | Upper mids / female vocals |
| 40-47     | 440-622        | A4-D#5       | Presence / cymbals |
| 48-55     | 659-932        | E5-A#5       | Treble / hihat |
| 56-63     | 988-2093       | B5-C7        | Air / shimmer |

### Complete Frequency Table

```cpp
// Semitone formula: freq = 55 * pow(2, bin/12.0)
// Bins 0-63 = A1 to C7 (5.25 octaves)

Bin  0:   55.00 Hz (A1)      Bin 32:  293.66 Hz (D4)
Bin  1:   58.27 Hz (A#1)     Bin 33:  311.13 Hz (D#4)
Bin  2:   61.74 Hz (B1)      Bin 34:  329.63 Hz (E4)
Bin  3:   65.41 Hz (C2)      Bin 35:  349.23 Hz (F4)
Bin  4:   69.30 Hz (C#2)     Bin 36:  369.99 Hz (F#4)
Bin  5:   73.42 Hz (D2)      Bin 37:  391.99 Hz (G4)
Bin  6:   77.78 Hz (D#2)     Bin 38:  415.30 Hz (G#4)
Bin  7:   82.41 Hz (E2)      Bin 39:  440.00 Hz (A4)
Bin  8:   87.31 Hz (F2)      Bin 40:  466.16 Hz (A#4)
Bin  9:   92.50 Hz (F#2)     Bin 41:  493.88 Hz (B4)
Bin 10:   98.00 Hz (G2)      Bin 42:  523.25 Hz (C5)
Bin 11:  103.83 Hz (G#2)     Bin 43:  554.36 Hz (C#5)
Bin 12:  110.00 Hz (A2)      Bin 44:  587.33 Hz (D5)
Bin 13:  116.54 Hz (A#2)     Bin 45:  622.25 Hz (D#5)
Bin 14:  123.47 Hz (B2)      Bin 46:  659.25 Hz (E5)
Bin 15:  130.81 Hz (C3)      Bin 47:  698.46 Hz (F5)
Bin 16:  138.59 Hz (C#3)     Bin 48:  739.99 Hz (F#5)
Bin 17:  146.83 Hz (D3)      Bin 49:  783.99 Hz (G5)
Bin 18:  155.56 Hz (D#3)     Bin 50:  830.61 Hz (G#5)
Bin 19:  164.81 Hz (E3)      Bin 51:  880.00 Hz (A5)
Bin 20:  174.61 Hz (F3)      Bin 52:  932.33 Hz (A#5)
Bin 21:  185.00 Hz (F#3)     Bin 53:  987.77 Hz (B5)
Bin 22:  196.00 Hz (G3)      Bin 54: 1046.50 Hz (C6)
Bin 23:  207.65 Hz (G#3)     Bin 55: 1108.73 Hz (C#6)
Bin 24:  220.00 Hz (A3)      Bin 56: 1174.66 Hz (D6)
Bin 25:  233.08 Hz (A#3)     Bin 57: 1244.51 Hz (D#6)
Bin 26:  246.94 Hz (B3)      Bin 58: 1318.51 Hz (E6)
Bin 27:  261.63 Hz (C4)      Bin 59: 1396.91 Hz (F6)
Bin 28:  277.18 Hz (C#4)     Bin 60: 1479.98 Hz (F#6)
Bin 29:  293.66 Hz (D4)      Bin 61: 1567.98 Hz (G6)
Bin 30:  311.13 Hz (D#4)     Bin 62: 1661.22 Hz (G#6)
Bin 31:  329.63 Hz (E4)      Bin 63: 2093.00 Hz (C7)
```

## Usage Examples

### Example 1: Bass-Responsive Expansion (Musical Saliency-Aware)

```cpp
void LGPPhotonicCrystalEffect::render(plugins::EffectContext& ctx) {
    // DON'T: Naive bass response - responds to ALL bass energy
    // float bass = ctx.audio.bin(8);  // Just 87 Hz

    // DO: Musical saliency-aware bass - only responds when bass is IMPORTANT
    float bass = 0.0f;
    if (ctx.audio.harmonicSaliency() > 0.5f || ctx.audio.rhythmicSaliency() > 0.6f) {
        // Bass is salient (chord root or rhythmic driver) - average bass range
        for (uint8_t i = 0; i < 16; i++) {
            bass += ctx.audio.bin(i);
        }
        bass /= 16.0f;
    }

    // Scale ripple frequency based on bass energy
    float rippleHz = 2.0f + bass * 3.0f;  // 2-5 Hz range

    // Use ripple frequency to drive expansion/contraction
    float ripplePhase = fmodf(ctx.t * rippleHz, 1.0f);
    uint8_t radius = 40 + bass * 40;  // 40-80 LED range
}
```

### Example 2: Spectral Visualization (8-Band Grouping)

```cpp
void SpectrumAnalyzerEffect::render(plugins::EffectContext& ctx) {
    const uint8_t BANDS = 8;
    const uint8_t BINS_PER_BAND = 8;

    for (uint8_t band = 0; band < BANDS; band++) {
        // Average 8 bins per band (64 bins / 8 bands)
        float magnitude = 0.0f;
        for (uint8_t b = 0; b < BINS_PER_BAND; b++) {
            magnitude += ctx.audio.bin(band * BINS_PER_BAND + b);
        }
        magnitude /= BINS_PER_BAND;

        // Draw vertical bar from center origin (LED 79/80)
        uint8_t height = magnitude * 80;  // 0-80 LEDs (half strip)
        drawBarFromCenter(ctx, band, height);
    }
}

// Helper function for center-origin bar drawing
void drawBarFromCenter(plugins::EffectContext& ctx, uint8_t band, uint8_t height) {
    // Map band to horizontal position across strip
    uint8_t xPos = (band * 160) / 8;  // 0, 20, 40, 60, 80, 100, 120, 140

    // Draw from center (79/80) outward
    for (uint8_t i = 0; i < height; i++) {
        uint16_t ledIndex1 = 79 - i;  // Left side
        uint16_t ledIndex2 = 80 + i;  // Right side

        if (ledIndex1 < 160) ctx.leds[ledIndex1] = CHSV(band * 32, 255, 255);
        if (ledIndex2 < 160) ctx.leds[ledIndex2] = CHSV(band * 32, 255, 255);
    }
}
```

### Example 3: Spectral Centroid (Brightness Mapping)

```cpp
void HarmonyRippleEffect::render(plugins::EffectContext& ctx) {
    // Compute spectral centroid = weighted average of frequency content
    float spectralCentroid = 0.0f;
    float totalMag = 0.0f;

    for (uint8_t i = 0; i < 64; i++) {
        float mag = ctx.audio.bin(i);
        spectralCentroid += mag * i;  // Weight by bin index
        totalMag += mag;
    }

    if (totalMag > 0.01f) {
        spectralCentroid /= totalMag;  // Normalize to [0, 63]

        // Map centroid to hue: low freq = warm (red), high freq = cool (blue)
        uint8_t hue = map(spectralCentroid, 0, 63, 0, 160);  // Red to blue

        // Apply hue shift to all LEDs
        for (uint16_t i = 0; i < ctx.numLEDs; i++) {
            ctx.leds[i].hue += hue;
        }
    }
}
```

### Example 4: Onset Detection via Spectral Delta

```cpp
void OnsetReactiveEffect::render(plugins::EffectContext& ctx) {
    // Track previous bin values for delta computation
    static float prevBins[64] = {0};

    // Compute spectral flux = sum of positive deltas
    float spectralFlux = 0.0f;
    for (uint8_t i = 0; i < 64; i++) {
        float delta = ctx.audio.bin(i) - prevBins[i];
        if (delta > 0.0f) {
            spectralFlux += delta;  // Only positive changes (onsets)
        }
        prevBins[i] = ctx.audio.bin(i);
    }

    // Detect onset: flux exceeds threshold
    if (spectralFlux > 0.5f) {
        triggerFlash();  // Visual response to onset
    }
}
```

## Critical Guidelines

### 1. Musical Saliency First, FFT Second

**ALWAYS read `AUDIO_VISUAL_SEMANTIC_MAPPING.md` before using FFT!**

❌ **WRONG**: Rigid frequency → visual binding
```cpp
float bass = ctx.audio.bin(8);
radius = bass * 80;  // Always expand on bass
```

✓ **CORRECT**: Saliency-aware response
```cpp
if (ctx.audio.harmonicSaliency() > 0.5f) {
    float bass = averageBins(ctx, 0, 15);
    radius = bass * 80;
}
```

**Why:** Not all bass energy is musically important. A bass note might be:
- Chord root (harmonically salient) → expand
- Passing tone (not salient) → ignore
- Rhythmic pulse (rhythmically salient) → sync to beat

### 2. Style-Adaptive Processing

Different music styles require different FFT smoothing strategies:

| Style | FFT Smoothing | Rationale |
|-------|---------------|-----------|
| RHYTHMIC_DRIVEN | None (raw) | Preserve transients for beat tracking |
| HARMONIC_DRIVEN | 300ms EMA | Smooth chord progressions |
| MELODIC_DRIVEN | 150ms EMA | Follow melody contour |
| TEXTURE_DRIVEN | 500ms EMA | Emphasize timbre evolution |
| DYNAMIC_DRIVEN | Onset-triggered | Capture dynamic swells |

**Implementation:**
```cpp
void render(plugins::EffectContext& ctx) {
    // Get detected music style
    auto style = ctx.audio.musicStyle();

    // Adjust smoothing based on style
    float alpha = 0.3f;  // Default
    if (style == MusicStyle::HARMONIC_DRIVEN) {
        alpha = 0.1f;  // Slower smoothing (300ms EMA)
    } else if (style == MusicStyle::RHYTHMIC_DRIVEN) {
        alpha = 1.0f;  // No smoothing (raw)
    }

    // Apply smoothing to bins
    for (uint8_t i = 0; i < 64; i++) {
        m_smoothedBins[i] += alpha * (ctx.audio.bin(i) - m_smoothedBins[i]);
    }
}
```

### 3. Avoid Single-Bin Decisions

Single bins are noisy due to spectral leakage and windowing artifacts. **Always average adjacent bins:**

```cpp
// BAD: Noisy single-bin response
float snare = ctx.audio.bin(28);

// GOOD: Average 3 adjacent bins
float snare = (ctx.audio.bin(27) + ctx.audio.bin(28) + ctx.audio.bin(29)) / 3.0f;

// BETTER: Average 8-bin range
float snare = 0.0f;
for (uint8_t i = 24; i < 32; i++) {
    snare += ctx.audio.bin(i);
}
snare /= 8.0f;
```

### 4. Temporal Context (Onset Detection)

Never react to instantaneous values - track history for onset detection:

```cpp
class MyEffect {
private:
    float m_prevBins[64] = {0};

public:
    void render(plugins::EffectContext& ctx) {
        float currentMag = ctx.audio.bin(index);
        float delta = currentMag - m_prevBins[index];
        m_prevBins[index] = currentMag;

        // Detect rising edge (onset)
        if (delta > 0.3f) {
            triggerVisualEvent();
        }
    }
};
```

## Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Update Rate** | 31.25 Hz | Every 512 samples @ 16 kHz |
| **Latency** | ~32 ms | AudioNode → RendererNode pipeline delay |
| **Frequency Resolution** | Semitone (~6%) | Musical note spacing |
| **Dynamic Range** | 0.0 - 1.0 | Normalized magnitude |
| **CPU Cost** | Already paid | GoertzelAnalyzer runs in AudioNode |
| **Memory Cost** | 256 bytes | 64 × 4 bytes/float in ControlBusFrame |
| **Frequency Range** | 55 Hz - 2093 Hz | A1 - C7 (5.25 octaves) |
| **Window Sizing** | Adaptive per bin | 64-2000 samples (4ms-125ms) |
| **Windowing Function** | Hann (4096-entry LUT) | Reduces spectral leakage |

### Interlaced Processing

GoertzelAnalyzer uses **interlaced processing by default** to reduce CPU load:
- **Enabled**: Only odd or even bins computed each frame (~50% CPU reduction)
- **Latency**: 2-frame delay for full spectrum update (64ms @ 31.25 Hz)
- **Disable if needed**: `analyzer.setInterlacedProcessing(false)` in AudioNode

## Implementation Details

### Data Flow Pipeline

```
GoertzelAnalyzer.analyze64()
    ↓ (64 raw magnitudes)
AudioNode.processHop()
    ↓ (dB mapping, activity gating, clamping)
ControlBusRawInput.bins64[]
    ↓ (copy to frame)
ControlBusFrame.bins64[]
    ↓ (published via SnapshotBuffer)
EffectContext::AudioContext.bin(index)
    ↓ (effect rendering)
Visual output
```

### Code References

| Component | File | Lines |
|-----------|------|-------|
| **FFT Computation** | `src/audio/GoertzelAnalyzer.cpp` | 255-320 |
| **Data Population** | `src/audio/AudioNode.cpp` | 796-820 |
| **Contract Definition** | `src/audio/contracts/ControlBus.h` | 66-69, 106-109 |
| **Effect API** | `src/plugins/api/EffectContext.h` | 206-223 |

## Troubleshooting

### FFT Returns All Zeros

**Symptom:** `ctx.audio.bin(i)` always returns 0.0

**Causes:**
1. **FEATURE_AUDIO_SYNC disabled** - FFT requires audio capture enabled
2. **Silence detection active** - Check `ctx.audio.silentScale()` > 0.0
3. **Activity gating** - Check `ctx.audio.rms()` > noise floor
4. **Not enough samples** - Wait ~125ms for first valid frame

**Debug:**
```cpp
Serial.printf("RMS: %.3f, Silent: %d, Bin[32]: %.3f\n",
    ctx.audio.rms(), ctx.audio.isSilent(), ctx.audio.bin(32));
```

### FFT Flickers/Dropouts

**Symptom:** Bins jump to zero briefly then recover

**Cause:** Activity gating during quiet passages

**Fix:** Check musical saliency before reacting to magnitude drops:
```cpp
if (ctx.audio.overallSaliency() > 0.3f) {
    float mag = ctx.audio.bin(i);
    // Use magnitude
}
```

### High-Frequency Bins Always Zero

**Symptom:** Bins 48-63 (740 Hz - 2093 Hz) always zero

**Cause:** No treble content in audio source, or mic frequency response

**Test:** Play pure tone at 1000 Hz, check bins 54-56 (1046-1175 Hz)

## See Also

- **[AUDIO_VISUAL_SEMANTIC_MAPPING.md](AUDIO_VISUAL_SEMANTIC_MAPPING.md)** - Musical intelligence principles (MANDATORY READ)
- **[src/audio/GoertzelAnalyzer.h](../../src/audio/GoertzelAnalyzer.h)** - FFT implementation details
- **[src/audio/contracts/ControlBus.h](../../src/audio/contracts/ControlBus.h)** - Audio data contracts
- **[src/plugins/api/EffectContext.h](../../src/plugins/api/EffectContext.h)** - Effect API reference
