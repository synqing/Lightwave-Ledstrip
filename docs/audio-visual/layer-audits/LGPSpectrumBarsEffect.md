# Layer Audit: LGPSpectrumBarsEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/LGPSpectrumBarsEffect.cpp`  
**Category**: Audio-Reactive, Continuous  
**Family**: Standing Bar Field

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Standing "bar" fields (Seed B)  
**Driver**: Continuous (Mel bands)  
**Spatial Pattern**: 8 static frequency bands mapped to spatial regions  
**Colour Policy**: Spectral gradient (frequency → hue mapping)  
**Compositing**: Replace mode

---

## Layer Breakdown

### Single Layer: 8-Band Mel Spectrum

**State Variables**:
```cpp
float m_smoothedBands[8];  // EMA-smoothed band amplitudes
uint8_t m_colourOffset = 0;  // Optional hue rotation
```

**Update Cadence**: Per-frame (continuous tracking of audio.bands[])

**Spatial Mapping**:
```cpp
int numBands = 8;
int pixelsPerBand = 80 / numBands;  // 10 pixels per band

for (int i = 0; i < numBands; i++) {
    int startPixel = i * pixelsPerBand;
    int endPixel = (i + 1) * pixelsPerBand;
    // Render band i to pixels [startPixel, endPixel)
}
```

**Frequency → Position Mapping**:
- Band 0 (lowest frequency, ~20-100 Hz): Centre (pixels 0-9)
- Band 7 (highest frequency, ~8-16 kHz): Edge (pixels 70-79)
- Linear spatial mapping (no logarithmic scaling in position)

---

## Audio Signals Used

| Signal | Usage | Range |
|--------|-------|-------|
| `ctx.audio.bands[0..7]` | Amplitude per Mel band | [0.0, 1.0] (normalized) |
| `ctx.audio.spectralFlux` | Not used directly | — |

**Mel Bands**: 8-band filterbank covering ~20 Hz to 16 kHz, log-spaced in frequency (perceptually uniform).

**Band Boundaries** (typical):
1. 20-100 Hz (sub-bass / kick)
2. 100-200 Hz (bass)
3. 200-500 Hz (low mids)
4. 500-1k Hz (mids)
5. 1k-2k Hz (upper mids)
6. 2k-4k Hz (presence)
7. 4k-8k Hz (brilliance)
8. 8k-16k Hz (air)

---

## Temporal Smoothing (Anti-Flicker)

### Exponential Moving Average (EMA)

```cpp
for (int i = 0; i < 8; i++) {
    float alpha = 0.3f;  // Smoothing factor
    m_smoothedBands[i] = m_smoothedBands[i] * (1.0f - alpha) + ctx.audio.bands[i] * alpha;
}
```

**Effect**:
- α = 0.3 → 30% new value, 70% old value per frame
- ~2-3 frame lag (at 120 FPS ≈ 20-25 ms)
- Reduces 20-30 Hz flicker in raw FFT bins

**Trade-off**:
- Lower α (e.g., 0.1): Smoother, but sluggish response to transients
- Higher α (e.g., 0.5): More responsive, but more flicker

---

## Colour Policy

### Spectral Gradient (Frequency → Hue)

```cpp
for (int i = 0; i < 8; i++) {
    uint8_t hue = map(i, 0, 8, 0, 255);  // Linear hue ramp
    // Band 0 (bass) → hue 0 (red)
    // Band 4 (mids) → hue 128 (cyan)
    // Band 7 (treble) → hue 224 (magenta/violet)
    
    float brightness = m_smoothedBands[i];
    CHSV colour(hue, 255, brightness * 255);
    
    // Render to bar region
}
```

**Hue Mapping**:
- Red/orange: Bass (warm, grounded)
- Yellow/green: Low mids (energetic)
- Cyan/blue: Upper mids (cool, airy)
- Violet: Treble (ethereal)

**Saturation Policy**: Fixed at 255 (fully saturated).

**Brightness Policy**: Direct mapping (amplitude → value).

**Visibility Floor**:
```cpp
if (colour.v < 30) colour.v = 0;  // Hard cutoff below LGP threshold
```

---

## LGP Considerations

### Centre-Origin Mapping

Strict compliance:
```cpp
for (int d = startPixel; d < endPixel; d++) {
    SET_CENTER_PAIR(d, colour);
}
```

**Effect on LGP**: Bass (red) appears at the centre, treble (violet) at the edges. The LGP creates "spectral columns" where adjacent bars bleed into each other through scattering, forming a continuous frequency gradient.

### Flicker Guards

**Temporal smoothing**: EMA with α = 0.3 (see above).

**Spatial smoothing**: None (sharp bar boundaries). Consider Gaussian blur across bar edges for softer transitions.

**Visibility floor**: 30/255 threshold (below this, hard cutoff to 0).

### Interference Patterns

**On full-band music** (e.g., electronic, rock):
- All 8 bars active simultaneously
- LGP creates a "rainbow column" effect
- Colours blend additively at bar boundaries (red+green → yellow)

**On sparse music** (e.g., solo piano, acoustic):
- Only a few mid bands active
- LGP shows isolated colour "islands"
- Quieter, more focused visual

---

## Performance Characteristics

**Heap Allocations**: None (8-element array is pre-allocated).

**Per-Frame Cost**:
- 8× EMA updates (trivial)
- 8× bar rendering (10 pixels each, 80 total `SET_CENTER_PAIR` calls)

**Typical FPS Impact**: Negligible (~0.05 ms per frame).

**Heap Delta Validation**: 0 bytes.

---

## Composability and Variations

### Variation 1: Logarithmic Brightness Scaling

Current implementation uses linear amplitude → brightness. For better dynamic range:
```cpp
float brightness_dB = 20.0f * log10(m_smoothedBands[i] + 1e-6f);  // dB scale
float brightness_norm = map(brightness_dB, -60.0f, 0.0f, 0.0f, 1.0f);  // -60 dB to 0 dB
```

**Result**: Quieter bands remain visible; loud bands don't clip as harshly.

---

### Variation 2: Adaptive Bar Width

Instead of fixed 10 pixels per bar, vary width by energy:
```cpp
int barWidth = 5 + (int)(m_smoothedBands[i] * 10);  // 5 to 15 pixels
```

**Result**: Loud bands "swell" in width; quiet bands shrink. Creates dynamic spatial texture.

---

### Variation 3: Hue Rotation by Beat

Rotate hue offset on each beat:
```cpp
if (ctx.audio.kickDetected) {
    m_colourOffset = (m_colourOffset + 32) % 256;  // Shift by 45°
}
uint8_t hue = (baseHue + m_colourOffset) % 256;
```

**Result**: Spectral gradient shifts chromatically on beats, adding rhythmic interest.

---

### Variation 4: Saliency-Driven Saturation

Modulate saturation by harmonic saliency:
```cpp
uint8_t saturation = lerp(128, 255, ctx.audio.audioPeakSaliency);
```

**Result**: Harmonic-rich passages (vocals, instruments) are vivid; noise/percussion is desaturated.

---

## Known Limitations

1. **Sharp bar boundaries**: No spatial smoothing. Consider Gaussian blur or feathering for softer transitions.

2. **Fixed 8 bands**: Hardcoded. Could be parameterised (4/8/16/32 bands) for different visual densities.

3. **No inter-band relationships**: Each bar is independent. Could add "harmonic highlighting" (brighten bands that are harmonically related).

4. **Linear hue mapping**: Doesn't account for perceptual hue non-uniformity. Consider adjusting hue curve for better chromatic balance.

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Standing bar field (Seed B) |
| **Temporal driver** | Continuous tracker (Mel bands) |
| **Spatial basis** | Static frequency → position mapping |
| **Colour policy** | Spectral gradient |
| **Compositing mode** | Replace |
| **Anti-flicker** | EMA smoothing (α = 0.3) |
| **LGP sensitivity** | Medium (bar bleeding creates gradients) |

---

## Related Effects

- [SpectrumAnalyzerEffect](SpectrumAnalyzerEffect.md) — 64-bin variant (higher resolution)
- [LGPBeatPulseEffect](LGPBeatPulseEffect.md) — Event-driven pulses (vs continuous bars)
- [SaliencyAwareEffect](SaliencyAwareEffect.md) — Harmonic-aware glow (vs raw spectrum)

---

**End of Layer Audit: LGPSpectrumBarsEffect**
