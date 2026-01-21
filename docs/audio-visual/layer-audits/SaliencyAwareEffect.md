# Layer Audit: SaliencyAwareEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/SaliencyAwareEffect.cpp`  
**Category**: Audio-Reactive, Continuous  
**Family**: Diffuse Glow Field

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Diffuse "glow" fields (Seed C)  
**Driver**: Continuous (audioPeakSaliency + RMS)  
**Spatial Pattern**: Dual-layer glow (base + harmonic peaks)  
**Colour Policy**: Energy-driven saturation + palette indexing  
**Compositing**: Additive blend

---

## Layer Breakdown

### Layer 1: Base Glow (RMS-Driven)

**State Variables**:
```cpp
float m_baseGlowIntensity = 0.0f;  // Attack/release filtered RMS
```

**Update Cadence**: Per-frame

**Update Equation**:
```cpp
float targetIntensity = ctx.audio.rms;
float attackRate = 0.2f;
float releaseRate = 0.05f;

if (targetIntensity > m_baseGlowIntensity) {
    m_baseGlowIntensity += (targetIntensity - m_baseGlowIntensity) * attackRate;
} else {
    m_baseGlowIntensity += (targetIntensity - m_baseGlowIntensity) * releaseRate;
}
```

**Spatial Rendering**:
```cpp
// Uniform glow across entire strip
for (int d = 0; d < 80; d++) {
    CHSV colour = ctx.palette[0];
    colour.v = m_baseGlowIntensity * 128;  // 50% max brightness for base layer
    SET_CENTER_PAIR(d, colour);
}
```

---

### Layer 2: Harmonic Peak Glow (Saliency-Driven)

**State Variables**:
```cpp
float m_peakGlowIntensity = 0.0f;  // Attack/release filtered saliency
```

**Update Cadence**: Per-frame

**Update Equation**:
```cpp
float targetIntensity = ctx.audio.audioPeakSaliency;
float attackRate = 0.15f;  // Slower attack than base (more "sticky")
float releaseRate = 0.03f;  // Much slower release (sustain harmonics)

// Same asymmetric envelope as Layer 1
```

**Spatial Rendering**:
```cpp
// Centre-weighted Gaussian glow
for (int d = 0; d < 80; d++) {
    float weight = gaussian(d, 20.0f);  // σ = 20 pixels (wide glow)
    float brightness = m_peakGlowIntensity * weight;
    
    CHSV colour = ctx.palette[1];  // Different palette index than base
    colour.v = brightness * 255;
    
    // Additive compositing
    ctx.leds[79 - d] += colour;
    ctx.leds[80 + d] += colour;
}
```

---

## Audio Signals Used

| Signal | Usage | Layer |
|--------|-------|-------|
| `ctx.audio.rms` | Base glow intensity | Layer 1 |
| `ctx.audio.audioPeakSaliency` | Harmonic peak glow intensity | Layer 2 |

**What is audioPeakSaliency?**  
A measure of "how harmonic/tonal" the audio is at a given moment:
- **High saliency** (0.8-1.0): Clear pitched notes, vocals, melodic instruments
- **Low saliency** (0.0-0.2): Noise, percussion, non-tonal content

**Calculation** (typical):
```
saliency = peakToAverageRatio(spectrum) * harmonicProductSpectrum(spectrum)
```

---

## Colour Policy

### Dual-Palette Indexing

**Base layer**: `palette[0]` (typically warm colour, e.g., amber)  
**Peak layer**: `palette[1]` (typically cool colour, e.g., cyan)

**Rationale**: Colour separation between tonal (peaks) and non-tonal (base) content creates visual distinction.

### Energy-Driven Saturation

```cpp
uint8_t saturation = lerp(128, 255, ctx.audio.rms);
colour.s = saturation;
```

**Effect**:
- Low energy (quiet): Desaturated, pastel
- High energy (loud): Vivid, saturated

---

## LGP Considerations

### Additive Compositing

Unlike replace-mode effects, saliency-aware uses **additive blending**:
```cpp
ctx.leds[i] += colour;  // RGB channels sum
```

**Effect on LGP**:
- Base glow + peak glow = composite colour
- Where layers overlap, colours mix additively (red base + blue peak = magenta composite)
- LGP naturally enhances this through light scattering

### Slow Attack/Release

**Attack** (150ms for peak layer):
- Harmonics "bloom" gradually when notes appear
- Avoids jarring flashes on transients

**Release** (300ms for peak layer):
- Harmonics "linger" after notes end
- Creates sustain/reverb-like visual tail

**Why asymmetric?**  
Perceptual realism: human hearing perceives attack faster than decay; visual should match.

---

## Performance Characteristics

**Heap Allocations**: None.

**Per-Frame Cost**:
- 2× attack/release envelope updates
- 80× base glow pixel writes
- 80× peak glow Gaussian evaluations + additive blends

**Typical FPS Impact**: ~0.15 ms per frame (slightly higher due to Gaussian + additive ops).

---

## Composability and Variations

### Variation 1: Multi-Band Saliency

Run saliency detection per frequency band:
```cpp
for (int i = 0; i < 8; i++) {
    float bandSaliency = calculateSaliency(ctx.audio.bands[i]);
    // Render band-specific peak glow
}
```

**Result**: Bass harmonics (e.g., bass guitar) and treble harmonics (e.g., vocals) are visually separated.

---

### Variation 2: Hue Modulation by Saliency

Instead of fixed palette, modulate hue:
```cpp
uint8_t hue = lerp(0, 60, ctx.audio.audioPeakSaliency);  // Red (non-tonal) → Yellow (tonal)
```

**Result**: Noise/percussion appears red; melodic content appears yellow/orange.

---

### Variation 3: Spatial Saliency Mapping

Map saliency to spatial position (not just intensity):
```cpp
int peakPosition = (int)(ctx.audio.spectralCentroid * 79);  // Centroid → position
// Render peak glow at peakPosition instead of centre
```

**Result**: Bright harmonic content "moves" spatially based on pitch (low = centre, high = edge).

---

## Known Limitations

1. **Single saliency value**: Doesn't distinguish multiple simultaneous harmonics (e.g., chord vs single note).

2. **Fixed spatial envelope**: Gaussian width is hardcoded. Could be adaptive (narrower for single notes, wider for chords).

3. **No temporal prediction**: Reacts to current frame only. Could add anticipation (e.g., pre-glow before beat).

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Diffuse glow field (Seed C) |
| **Temporal driver** | Continuous tracker (RMS + saliency) |
| **Spatial basis** | Gaussian envelope (dual-layer) |
| **Colour policy** | Dual-palette + energy saturation |
| **Compositing mode** | Additive |
| **Anti-flicker** | Attack/release envelopes (slow) |
| **LGP sensitivity** | High (additive colours create complex hues) |

---

## Related Effects

- [LGPChordGlowEffect](LGPChordGlowEffect.md) — Chord-driven variant
- [WaveReactiveEffect](WaveReactiveEffect.md) — Continuous wave (vs static glow)

---

**End of Layer Audit: SaliencyAwareEffect**
