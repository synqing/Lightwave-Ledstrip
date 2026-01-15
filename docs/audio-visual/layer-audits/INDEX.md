# Audio-Reactive Effects: Layer Audit Index

**Version**: 1.0  
**Last Updated**: 2026-01-11  
**Firmware Path**: `firmware/v2/src/effects/ieffect/`

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md)

---

## Overview

This directory contains **per-effect layer audits** for all current audio-reactive effects in LightwaveOS firmware. Each audit documents:

- **Logic seed** (mathematical/algorithmic basis)
- **Layer breakdown** (state variables, update cadence, rendering)
- **Audio signals used** (which features drive which behaviours)
- **Colour policy** (palette usage, hue/saturation/brightness mapping)
- **LGP considerations** (centre-origin compliance, flicker guards, interference patterns)
- **Performance characteristics** (heap allocations, FPS impact)
- **Composability variations** (how to extend/modify the effect)

---

## Quick Reference: Logic Seeds

| Effect | Logic Seed | Temporal Driver | Spatial Pattern | Use Case |
|--------|-----------|-----------------|-----------------|----------|
| [LGPBeatPulseEffect](#lgpbeatpulseeffect) | Radial travelling rings | Event (kick/snare) | Dual expanding pulses | Percussive music |
| [LGPSpectrumBarsEffect](#lgpspectrumbarseffect) | Standing bars | Continuous (Mel bands) | 8 static frequency bars | Frequency visualisation |
| [SpectrumAnalyzerEffect](#spectrumanalyzereffect) | Standing bars | Continuous (FFT bins) | 64 static frequency bars | High-resolution spectrum |
| [WaveReactiveEffect](#wavereactiveeffect) | Radial travelling ring | Continuous (RMS/flux) | Single expanding wave | Ambient/electronic music |
| [SaliencyAwareEffect](#saliencyawareeffect) | Diffuse glow | Continuous (saliency) | Dual-layer glow | Melodic/harmonic music |
| [StyleAdaptiveEffect](#styleadaptiveeffect) | Morph field | Hybrid (style-aware) | Glow ↔ pulse morph | Mixed genre / adaptive |
| [LGPChordGlowEffect](#lgpchordgloweffect) | Diffuse glow | Continuous (chord) | Centre-weighted glow | Harmonic/chordal music |
| [LGPAudioTestEffect](#lgpaudioteesteffect) | Test pattern | Continuous (multi) | 8 diagnostic regions | Debugging / calibration |

---

## Effect Audits

### LGPBeatPulseEffect

**File**: [LGPBeatPulseEffect.md](LGPBeatPulseEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/LGPBeatPulseEffect.cpp`

**Core Logic Seed**: Radial travelling "ring" impulses (Seed A)

**Mathematical Form**:
```
position(t) = position(t-1) + velocity * dt
brightness(d) = gaussian(d - position(t), sigma) * amplitude
```

**Key Characteristics**:
- **Dual pulses**: Primary (kick) + secondary (snare)
- **Gaussian falloff**: σ ≈ 10 pixels, FWHM ≈ 24 pixels
- **Exponential decay**: `amplitude *= 0.92` per frame
- **Palette walk**: Cycles through global palette on each beat

**When to Use**: Percussive, rhythmic music (EDM, hip-hop, rock)

**Performance**: ~0.1 ms/frame, 0 bytes heap

---

### LGPSpectrumBarsEffect

**File**: [LGPSpectrumBarsEffect.md](LGPSpectrumBarsEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/LGPSpectrumBarsEffect.cpp`

**Core Logic Seed**: Standing "bar" fields (Seed B)

**Mathematical Form**:
```
For each band i in 0..7:
  brightness[i] = EMA(melBands[i], alpha=0.3)
  hue[i] = map(i, 0, 7, 0, 255)  // Spectral gradient
```

**Key Characteristics**:
- **8 Mel bands**: Log-spaced frequency bins (perceptually uniform)
- **Spectral gradient**: Bass (red) at centre → treble (violet) at edges
- **EMA smoothing**: α = 0.3 for anti-flicker
- **Visibility floor**: Hard cutoff at brightness < 30

**When to Use**: Full-band music, frequency analysis, "equalizer" aesthetic

**Performance**: ~0.05 ms/frame, 0 bytes heap

---

### SpectrumAnalyzerEffect

**File**: [SpectrumAnalyzerEffect.md](SpectrumAnalyzerEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/SpectrumAnalyzerEffect.cpp`

**Core Logic Seed**: Standing "bar" fields (Seed B), high resolution variant

**Mathematical Form**: Same as LGPSpectrumBarsEffect, but 64 bins instead of 8.

**Key Characteristics**:
- **64 FFT bins**: ~1.25 pixels per bin (dense texture)
- **Linear frequency spacing**: (vs Mel bands' logarithmic spacing)
- **Oscilloscope-like**: Fine-grained frequency detail
- **Harmonic overtones visible**: Musical notes create "comb filter" patterns

**When to Use**: Technical audio analysis, synth/electronic music with rich harmonics

**Performance**: ~0.1 ms/frame, 0 bytes heap

---

### WaveReactiveEffect

**File**: [WaveReactiveEffect.md](WaveReactiveEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/WaveReactiveEffect.cpp`

**Core Logic Seed**: Radial travelling ring (Seed A), continuous variant

**Mathematical Form**:
```
velocity = baseVelocity * (0.5 + RMS * 1.5)  // 0.5x to 2.0x modulation
position += velocity * dt
amplitude = spectralFlux
```

**Key Characteristics**:
- **Single continuous wave**: Always moving (no discrete triggers)
- **RMS-driven velocity**: Loud → faster, quiet → slower
- **Flux-driven amplitude**: Changing spectrum → brighter
- **Fixed palette index**: Always uses `palette[0]`

**When to Use**: Ambient, electronic, pad-heavy music (less percussive than beat pulse)

**Performance**: ~0.1 ms/frame, 0 bytes heap

---

### SaliencyAwareEffect

**File**: [SaliencyAwareEffect.md](SaliencyAwareEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/SaliencyAwareEffect.cpp`

**Core Logic Seed**: Diffuse "glow" fields (Seed C), dual-layer

**Mathematical Form**:
```
Layer 1 (base):    brightness = RMS * uniform_distribution
Layer 2 (peaks):   brightness = saliency * gaussian(d, sigma=20)
Composite:         output = layer1 + layer2  (additive blend)
```

**Key Characteristics**:
- **Dual-layer glow**: Base (RMS) + peaks (saliency)
- **Additive compositing**: Colours mix where layers overlap
- **Slow attack/release**: 150ms attack (peaks), 300ms release (sustain)
- **Energy saturation**: Quiet = desaturated, loud = vivid

**When to Use**: Melodic, harmonic music (vocals, piano, string sections)

**Performance**: ~0.15 ms/frame, 0 bytes heap

---

### StyleAdaptiveEffect

**File**: [StyleAdaptiveEffect.md](StyleAdaptiveEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/StyleAdaptiveEffect.cpp`

**Core Logic Seed**: Adaptive "morph" fields (Seed D)

**Mathematical Form**:
```
alpha = f(musicStyle, energyHigh, tempo)  // Morph weight [0.0, 1.0]
output = patternA * (1 - alpha) + patternB * alpha

patternA = calmDiffuseGlow(RMS, saliency)
patternB = aggressiveFastPulses(beats, energy)
```

**Key Characteristics**:
- **Pattern morphing**: Calm glow ↔ aggressive pulses
- **Style-driven**: CALM → blue glow, AGGRESSIVE → red pulses
- **Slow transition**: 500ms cross-fade (smooth adaptation)
- **Alpha blending**: Both patterns visible during morph

**When to Use**: Mixed-genre playlists, music with dynamic range (build-ups/drops)

**Performance**: ~0.2 ms/frame (dual rendering), 0 bytes heap

---

### LGPChordGlowEffect

**File**: [LGPChordGlowEffect.md](LGPChordGlowEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/LGPChordGlowEffect.cpp`

**Core Logic Seed**: Diffuse "glow" fields (Seed C), chord-driven

**Mathematical Form**:
```
brightness = chordStability * gaussian(d, sigma=20)
hue = chordToHue(chordRoot)  // Circle of fifths → colour wheel mapping
```

**Key Characteristics**:
- **Chord detection**: Root note → hue (C=red, G=blue-violet, etc.)
- **Stability gating**: Clear chords = bright, dissonant = dim
- **Centre-weighted glow**: Brightest at centre, fades to edges
- **Symmetric attack/release**: 150ms (chord changes "bloom")

**When to Use**: Harmonic/chordal music (piano, guitar, synth pads)

**Performance**: ~0.1 ms/frame, 0 bytes heap

---

### LGPAudioTestEffect

**File**: [LGPAudioTestEffect.md](LGPAudioTestEffect.md)  
**Source**: `firmware/v2/src/effects/ieffect/LGPAudioTestEffect.cpp`

**Core Logic Seed**: Diagnostic "test" patterns (Seed E)

**Mathematical Form**:
```
For each region i in 0..7:
  featureValue = getAudioFeature(i)  // RMS, flux, beats, saliency, etc.
  brightness[region_i] = featureValue * 255
```

**Key Characteristics**:
- **8 diagnostic regions**: RMS, flux, beats, saliency, chords, energy
- **Cyclic hue**: All regions share same hue (changes over time)
- **No smoothing**: Raw feature values (for debugging)
- **Hard boundaries**: Sharp region edges (diagnostic clarity)

**When to Use**: Debugging audio pipeline, tuning thresholds, calibrating input levels

**Performance**: ~0.05 ms/frame, 0 bytes heap

---

## Common Patterns Across Effects

### Anti-Flicker Strategies

| Strategy | Effects Using It | Parameters |
|----------|------------------|------------|
| **EMA smoothing** | LGPSpectrumBarsEffect, SpectrumAnalyzerEffect | α = 0.3 |
| **Attack/release envelope** | SaliencyAwareEffect, LGPChordGlowEffect, WaveReactiveEffect | Attack: 100-200ms, Release: 50-300ms |
| **Visibility floor** | LGPSpectrumBarsEffect, SpectrumAnalyzerEffect | brightness < 30 → 0 |
| **Gaussian spatial falloff** | LGPBeatPulseEffect, WaveReactiveEffect | σ = 8-12 pixels (natural smoothing) |

### Centre-Origin Compliance

**All effects use `SET_CENTER_PAIR(d, colour)` macro**, which expands to:
```cpp
ctx.leds[79 - d] = colour;  // Strip 1 (left from centre)
ctx.leds[80 + d] = colour;  // Strip 2 (right from centre)
```

This ensures centre-origin topology compliance for LGP interference patterns.

### Performance Budget

| Effect | Per-Frame Cost | Heap Delta | Notes |
|--------|---------------|-----------|-------|
| LGPBeatPulseEffect | ~0.10 ms | 0 bytes | Dual pulse rendering |
| LGPSpectrumBarsEffect | ~0.05 ms | 0 bytes | Simple bar rendering |
| SpectrumAnalyzerEffect | ~0.10 ms | 0 bytes | 64 bins (more work) |
| WaveReactiveEffect | ~0.10 ms | 0 bytes | Single wave rendering |
| SaliencyAwareEffect | ~0.15 ms | 0 bytes | Dual-layer + additive blend |
| StyleAdaptiveEffect | ~0.20 ms | 0 bytes | Dual rendering + alpha blend |
| LGPChordGlowEffect | ~0.10 ms | 0 bytes | Gaussian glow |
| LGPAudioTestEffect | ~0.05 ms | 0 bytes | Simplest effect |

**Total budget**: 120 FPS target = 8.33 ms/frame. Audio effects consume <2% of budget.

---

## Composability Matrix

**Which effects work well together?** (Additive compositing)

| Background Effect | Foreground Effect | Result |
|-------------------|------------------|--------|
| LGPSpectrumBarsEffect | LGPBeatPulseEffect | Bars provide spectral context, pulses add percussive accents |
| SaliencyAwareEffect | LGPChordGlowEffect | Harmonic glow + chord-specific colour |
| WaveReactiveEffect | LGPBeatPulseEffect | Continuous wave + discrete beat punctuation |

**Incompatible combinations** (Both use replace mode, later wins):
- LGPBeatPulseEffect + WaveReactiveEffect (both radial pulses)
- LGPSpectrumBarsEffect + SpectrumAnalyzerEffect (both standing bars)

---

## Future Expansion

**New effects to consider**:
1. **Multi-band wave cascade** (Variation of WaveReactiveEffect)
2. **Harmonic highlight** (Detect overtone series, brighten related bins)
3. **Rhythm complexity visualiser** (Polyrhythm → layered offset pulses)
4. **Stereo field panner** (Left/right channel → spatial bias on dual strips)
5. **Transition engine integration** (Cross-fade between effects on section changes)

**See Master Reference** for full taxonomy and synthesis strategies: [AUDIO_REACTIVE_PATTERNS_MASTER.md](../AUDIO_REACTIVE_PATTERNS_MASTER.md)

---

## Contributing New Effects

**Checklist for new audio-reactive effects**:

1. ✅ **Choose logic seed**: Radial pulse, standing bar, diffuse glow, morph field, or hybrid?
2. ✅ **Select temporal driver**: Event (beats) or continuous (RMS/flux/saliency)?
3. ✅ **Define spatial basis**: Gaussian, bars, uniform, or custom?
4. ✅ **Design colour policy**: Palette walk, spectral gradient, semantic mapping, or fixed?
5. ✅ **Implement anti-flicker**: EMA, attack/release, visibility floor, or implicit smoothing?
6. ✅ **Test centre-origin compliance**: Use `SET_CENTER_PAIR` macro, verify on hardware
7. ✅ **Validate heap delta**: Run `validate <effectId>` serial command, ensure 0 bytes allocated
8. ✅ **Document layer audit**: Create `YourEffect.md` in this directory
9. ✅ **Update this index**: Add entry to tables above

---

## Related Documentation

- [Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) — Complete taxonomy and synthesis guide
- [Audio-Reactive Effects Analysis](../audio-reactive-effects-analysis.md) — Design philosophy
- [Audio-Visual Semantic Mapping](../audio-visual-semantic-mapping.md) — High-level feature mapping

---

**End of Layer Audit Index**
