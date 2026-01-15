# Layer Audit: WaveReactiveEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/WaveReactiveEffect.cpp`  
**Category**: Audio-Reactive, Continuous  
**Family**: Radial Impulse

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Radial travelling ring (Seed A)  
**Driver**: Continuous (RMS + spectral flux)  
**Spatial Pattern**: Single expanding wave from centre to edge  
**Colour Policy**: Fixed palette index (palette[0])  
**Compositing**: Replace mode

---

## Key Differences from LGPBeatPulseEffect

| Aspect | WaveReactiveEffect | LGPBeatPulseEffect |
|--------|-------------------|-------------------|
| **Trigger** | Continuous (RMS/flux) | Event (kick/snare) |
| **Wave count** | Single continuous wave | Dual discrete pulses |
| **Motion** | Smooth, always moving | Discrete, triggered |
| **Velocity** | RMS-modulated | Fixed |

---

## Layer Breakdown

### Single Layer: Continuous Wave

**State Variables**:
```cpp
float m_wavePosition = 0.0f;      // Normalized position [0.0, 1.0]
float m_waveAmplitude = 0.0f;     // Current amplitude [0.0, 1.0]
float m_smoothedRMS = 0.0f;       // EMA-smoothed RMS
float m_smoothedFlux = 0.0f;      // EMA-smoothed spectral flux
```

**Update Cadence**: Per-frame

**Motion Equation**:
```cpp
// Update smoothed audio features
m_smoothedRMS = m_smoothedRMS * 0.8f + ctx.audio.rms * 0.2f;
m_smoothedFlux = m_smoothedFlux * 0.8f + ctx.audio.spectralFlux * 0.2f;

// Modulate velocity by RMS
float baseVelocity = 1.0f / 500.0f;  // ~500ms travel time
float velocity = baseVelocity * (0.5f + m_smoothedRMS * 1.5f);  // 0.5x to 2.0x

// Update position
m_wavePosition += deltaTime * velocity;
if (m_wavePosition > 1.0f) m_wavePosition = 0.0f;  // Wrap

// Update amplitude (driven by flux)
m_waveAmplitude = m_smoothedFlux;
```

**Spatial Rendering**:
```cpp
for (int d = 0; d < 80; d++) {
    float pixelPosition = m_wavePosition * 79.0f;
    float distance = abs(d - pixelPosition);
    float brightness = gaussian(distance, 12.0f) * m_waveAmplitude;
    
    CHSV colour = ctx.palette[0];
    colour.v = brightness * 255;
    SET_CENTER_PAIR(d, colour);
}
```

---

## Audio Signals Used

| Signal | Usage | Effect |
|--------|-------|--------|
| `ctx.audio.rms` | Velocity modulation | Loud → faster wave |
| `ctx.audio.spectralFlux` | Amplitude modulation | Changing → brighter wave |

**Why flux for amplitude?**  
Spectral flux measures "how much the spectrum is changing," which correlates with perceptual "energy" and "excitement" better than raw RMS (which can be high even on static tones).

---

## Colour Policy

### Fixed Palette Index

```cpp
CHSV colour = ctx.palette[0];  // Always uses first palette colour
```

**Rationale**: Simplicity; single coherent colour creates unified wave visual. Contrast with LGPBeatPulseEffect's palette walk.

**Palette Responsiveness**: Changing palette immediately affects the wave colour.

---

## LGP Considerations

### Continuous vs Discrete

**Continuous wave** (WaveReactiveEffect):
- Always present (no "off" state)
- Smooth, meditative visual
- Good for ambient/electronic music

**Discrete pulses** (LGPBeatPulseEffect):
- Punctuated, rhythmic visual
- Good for percussive music

### Interference

**On sustained music** (e.g., pad synths, drones):
- Wave moves slowly, creating "breathing" LGP glow
- High flux passages (e.g., filter sweeps) make wave brighten

**On rhythmic music**:
- Wave "chases" the beat (velocity increases on loud sections)
- Less visually distinct than discrete beat pulses

---

## Performance Characteristics

**Heap Allocations**: None.

**Per-Frame Cost**: Same as LGPBeatPulseEffect (~0.1 ms).

---

## Composability and Variations

### Variation 1: Multi-Band Waves

Run multiple waves, one per frequency band:
```cpp
for (int i = 0; i < 8; i++) {
    float velocity = baseVelocity * ctx.audio.bands[i];
    m_bandWavePositions[i] += deltaTime * velocity;
    // Render each wave with different hue
}
```

**Result**: Bass moves slower (red), treble moves faster (violet).

---

### Variation 2: Bidirectional Wave

Make wave propagate both inward and outward:
```cpp
if (ctx.audio.spectralFlux > threshold) {
    m_direction = OUTWARD;
} else {
    m_direction = INWARD;
}
```

**Result**: Wave "breathes" in and out, controlled by musical dynamics.

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Radial travelling ring (Seed A) |
| **Temporal driver** | Continuous tracker (RMS + flux) |
| **Spatial basis** | Gaussian pulse |
| **Colour policy** | Fixed palette index |
| **Compositing mode** | Replace |
| **Anti-flicker** | EMA smoothing (α = 0.2 for RMS/flux) |
| **LGP sensitivity** | Medium (single wave creates clean glow) |

---

## Related Effects

- [LGPBeatPulseEffect](LGPBeatPulseEffect.md) — Event-driven variant
- [SaliencyAwareEffect](SaliencyAwareEffect.md) — Harmonic-aware glow

---

**End of Layer Audit: WaveReactiveEffect**
