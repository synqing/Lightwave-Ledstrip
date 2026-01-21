# Layer Audit: SpectrumAnalyzerEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/SpectrumAnalyzerEffect.cpp`  
**Category**: Audio-Reactive, Continuous  
**Family**: Standing Bar Field

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Standing "bar" fields (Seed B)  
**Driver**: Continuous (64-bin FFT)  
**Spatial Pattern**: 64 FFT bins mapped to spatial regions (higher resolution than LGPSpectrumBarsEffect)  
**Colour Policy**: Spectral gradient (bin → hue mapping)  
**Compositing**: Replace mode

---

## Key Differences from LGPSpectrumBarsEffect

| Aspect | SpectrumAnalyzerEffect | LGPSpectrumBarsEffect |
|--------|----------------------|---------------------|
| **Resolution** | 64 bins (1.25 pixels/bin) | 8 bands (10 pixels/band) |
| **Source** | Raw FFT bins | Mel-scaled bands |
| **Frequency spacing** | Linear | Logarithmic (perceptually uniform) |
| **Visual character** | Dense, "oscilloscope-like" | Chunky, "equalizer-like" |

---

## Layer Breakdown

### Single Layer: 64-Bin FFT Spectrum

**State Variables**:
```cpp
float m_smoothedBins[64];  // EMA-smoothed bin amplitudes
```

**Update Cadence**: Per-frame

**Spatial Mapping**:
```cpp
int numBins = 64;
float pixelsPerBin = 80.0f / 64.0f;  // ~1.25 pixels per bin

for (int i = 0; i < numBins; i++) {
    int pixelIndex = (int)(i * pixelsPerBin);
    // Render bin i to pixel pixelIndex (some pixels may get multiple bins)
}
```

**Frequency → Position Mapping**:
- Bin 0 (~0 Hz, DC): Centre
- Bin 32 (~8 kHz): Mid-point
- Bin 63 (~16 kHz, Nyquist): Edge

---

## Audio Signals Used

| Signal | Usage | Range |
|--------|-------|-------|
| `ctx.audio.bins64[0..63]` | Amplitude per FFT bin | [0.0, 1.0] (linear or dB-scaled) |

**FFT Parameters** (typical):
- Window size: 512 or 1024 samples
- Hop size: 256 samples (50% overlap)
- Sample rate: 44.1 kHz
- Frequency resolution: 44100 / 1024 ≈ 43 Hz per bin

**Bin Frequency**:
```
f_bin[i] = (i * sampleRate) / fftSize
f_bin[0] = 0 Hz
f_bin[32] ≈ 1378 Hz
f_bin[63] ≈ 2713 Hz (only half of Nyquist is typically used)
```

---

## Temporal Smoothing

### EMA (Same as LGPSpectrumBarsEffect)

```cpp
for (int i = 0; i < 64; i++) {
    m_smoothedBins[i] = m_smoothedBins[i] * 0.7f + ctx.audio.bins64[i] * 0.3f;
}
```

---

## Colour Policy

### Spectral Gradient (Bin → Hue)

```cpp
for (int i = 0; i < 64; i++) {
    uint8_t hue = map(i, 0, 64, 0, 255);
    float brightness = m_smoothedBins[i];
    CHSV colour(hue, 255, max(30, brightness * 255));  // 30 = visibility floor
}
```

**Hue Mapping**: Same red→violet progression as LGPSpectrumBarsEffect, but smoother (more bins).

---

## LGP Considerations

### Interference Patterns

**Higher resolution = finer texture**:
- 64 bins create a "grainy" visual texture on the LGP
- Adjacent bins blend through LGP scattering, creating continuous gradients
- Musical overtones (harmonics) produce visible "comb filter" patterns (bright bands at multiples of fundamental frequency)

### Flicker

**More bins = more potential flicker**:
- Raw FFT bins are noisier than Mel bands
- EMA smoothing is critical (α = 0.3 strikes good balance)

---

## Performance Characteristics

**Heap Allocations**: None.

**Per-Frame Cost**:
- 64× EMA updates
- 80× pixel writes (some bins map to same pixel)

**Typical FPS Impact**: ~0.1 ms per frame.

---

## Composability and Variations

### Variation 1: Logarithmic Frequency Mapping

Current implementation uses linear bin → pixel mapping, which over-represents high frequencies. For perceptually balanced mapping:
```cpp
int pixelIndex = (int)(log2(i + 1) / log2(64) * 80);
```

**Result**: More pixels allocated to bass/mids (where musical energy concentrates).

---

### Variation 2: Peak Hold

Add "peak hold" markers (like hardware spectrum analyzers):
```cpp
if (m_smoothedBins[i] > m_peakBins[i]) {
    m_peakBins[i] = m_smoothedBins[i];
} else {
    m_peakBins[i] *= 0.98f;  // Slow decay
}
// Render peak as bright dot above bar
```

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Standing bar field (Seed B) |
| **Temporal driver** | Continuous tracker (FFT bins) |
| **Spatial basis** | Static frequency → position mapping |
| **Colour policy** | Spectral gradient |
| **Compositing mode** | Replace |
| **Anti-flicker** | EMA smoothing (α = 0.3) |
| **LGP sensitivity** | High (fine texture creates complex scattering patterns) |

---

## Related Effects

- [LGPSpectrumBarsEffect](LGPSpectrumBarsEffect.md) — Lower resolution (8 bands)
- [SaliencyAwareEffect](SaliencyAwareEffect.md) — Harmonic-aware (vs raw spectrum)

---

**End of Layer Audit: SpectrumAnalyzerEffect**
