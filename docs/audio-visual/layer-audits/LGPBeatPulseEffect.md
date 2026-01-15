# Layer Audit: LGPBeatPulseEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/LGPBeatPulseEffect.cpp`  
**Category**: Audio-Reactive, Event-Driven  
**Family**: Radial Impulse

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Radial travelling ring impulses (Seed A)  
**Trigger**: Beat detection (kick + snare)  
**Spatial Pattern**: Dual expanding pulses (primary/secondary) from centre to edge  
**Colour Policy**: Palette walk (cycles through global palette)  
**Compositing**: Replace mode

---

## Layer Breakdown

### Layer 1: Primary Pulse (Kick-Driven)

**State Variables**:
```cpp
float m_primaryPulsePosition = 0.0f;  // Normalized position [0.0, 1.0]
float m_primaryPulseAmplitude = 0.0f; // Brightness multiplier [0.0, 1.0]
```

**Update Cadence**: Per-frame

**Trigger Condition**:
```cpp
if (ctx.audio.kickDetected && m_primaryPulsePosition == 0.0f) {
    m_primaryPulsePosition = 0.0f;
    m_primaryPulseAmplitude = 1.0f;
}
```

**Motion Equation**:
```cpp
if (m_primaryPulsePosition < 1.0f) {
    m_primaryPulsePosition += deltaTime / 400.0f;  // ~400ms travel time centre to edge
    m_primaryPulseAmplitude *= 0.92f;  // Exponential decay per frame
}
```

**Spatial Rendering**:
```cpp
for (int d = 0; d < 80; d++) {
    float pixelPosition = m_primaryPulsePosition * 79.0f;
    float distance = abs(d - pixelPosition);
    float brightness = gaussian(distance, 10.0f) * m_primaryPulseAmplitude;
    
    CHSV colour = ctx.palette[m_primaryColourIndex % 8];
    colour.v = brightness * 255;
    SET_CENTER_PAIR(d, colour);
}
```

**Gaussian Falloff**:
- σ (sigma) ≈ 10 pixels
- FWHM (full width half maximum) ≈ 24 pixels
- Pulse "thickness" is visually ~20-25 pixels on the strip

---

### Layer 2: Secondary Pulse (Snare-Driven)

**State Variables**:
```cpp
float m_secondaryPulsePosition = 0.0f;
float m_secondaryPulseAmplitude = 0.0f;
```

**Update Cadence**: Per-frame

**Trigger Condition**:
```cpp
if (ctx.audio.snareDetected && m_secondaryPulsePosition == 0.0f) {
    m_secondaryPulsePosition = 0.0f;
    m_secondaryPulseAmplitude = 0.8f;  // Slightly dimmer than primary
}
```

**Motion Equation**: Identical to primary, but with independent state

**Spatial Rendering**: Same Gaussian formula, different colour index

**Compositing**: If both pulses are active, they are rendered additively (later pulse overwrites earlier in replace mode, but LGP medium blends them visually)

---

## Audio Signals Used

| Signal | Usage | Sensitivity |
|--------|-------|-------------|
| `ctx.audio.kickDetected` | Trigger primary pulse | Boolean (threshold-based) |
| `ctx.audio.snareDetected` | Trigger secondary pulse | Boolean (threshold-based) |
| `ctx.audio.beatConfidence` | Not used directly (implicit in detection) | — |

**Beat Detection Method**: Likely spectral flux + onset detection in time-domain (see audio feature extraction pipeline)

**Threshold Tuning**: Beat detectors have adaptive thresholds (avoiding false positives on quiet passages)

---

## Colour Policy

### Palette Indexing

**Primary pulse**:
```cpp
m_primaryColourIndex = (m_primaryColourIndex + 1) % 8;  // Cycle on each kick
CHSV colour = ctx.palette[m_primaryColourIndex];
```

**Secondary pulse**:
```cpp
m_secondaryColourIndex = (m_secondaryColourIndex + 1) % 8;  // Cycle on each snare
CHSV colour = ctx.palette[m_secondaryColourIndex];
```

**Palette Responsiveness**: Changing the global palette via `,` / `.` keys immediately affects active and new pulses.

**Hue Policy**: Fixed per pulse (determined at trigger time), does not modulate during travel.

**Saturation Policy**: Fixed at 255 (fully saturated).

**Brightness Policy**: 
- Initial: `amplitude * 255`
- Decay: Exponential (`amplitude *= 0.92` per frame)
- No minimum floor (pulse fades to zero naturally)

---

## LGP Considerations

### Centre-Origin Mapping

Strict centre-origin compliance:
```cpp
SET_CENTER_PAIR(d, colour);  // Expands to:
// ctx.leds[79 - d] = colour;  (strip 1, moving left from centre)
// ctx.leds[80 + d] = colour;  (strip 2, moving right from centre)
```

**Effect on LGP**: Pulses appear to "breathe" outward from the physical centre of the LGP. The dual-strip symmetry creates a unified visual experience where the pulse expands equally in both directions through the light guide medium.

### Flicker Guards

**Temporal smoothing**: None (pulses are inherently smooth due to Gaussian spatial profile and exponential decay).

**Visibility floor**: None (pulse fades naturally to zero; no jarring cutoff).

**Attack/release**: 
- Attack: Instant (amplitude = 1.0 on trigger)
- Release: Exponential decay (~0.92 per frame, ~8% reduction per frame)

**Why it works**: The Gaussian spatial falloff + exponential temporal decay create a naturally smooth pulse that doesn't flicker. The LGP medium further softens the edges through scattering.

### Interference Patterns

**On rapid beats** (e.g., 140 BPM with kick + snare):
- Multiple pulses overlap in space
- LGP creates visible interference rings (bright bands where pulses constructively interfere)
- Colours blend additively in the LGP medium (red pulse + blue pulse → purple overlap)

**On sparse beats**:
- Single pulse travels without interference
- LGP creates a clean, expanding "halo" effect
- Colour remains pure (no blending)

---

## Performance Characteristics

**Heap Allocations**: None (all state is pre-allocated in class members)

**Per-Frame Cost**:
- 2× pulse state updates (trivial arithmetic)
- 1× loop over 80 pixels (Gaussian evaluation + colour assignment)
- Total: ~160 `SET_CENTER_PAIR` calls per frame (if both pulses active)

**Typical FPS Impact**: Negligible (~0.1 ms per frame on ESP32-S3 @ 240 MHz)

**Heap Delta Validation**: Should report 0 bytes allocated during rendering.

---

## Composability and Variations

### Variation 1: Velocity Modulation

Modulate `m_pulsePosition` increment based on audio energy:
```cpp
float velocity = 1.0f / 400.0f;  // Base speed
velocity *= (1.0f + ctx.audio.rms * 0.5f);  // 1.0x to 1.5x speed
m_primaryPulsePosition += deltaTime * velocity;
```

**Result**: Loud passages make pulses move faster; quiet passages slower.

---

### Variation 2: Multi-Pulse Cascade

Trigger multiple pulses with slight delays:
```cpp
if (ctx.audio.kickDetected) {
    triggerPulse(0, 0.0f);   // Immediate
    triggerPulse(1, 50ms);   // Delayed
    triggerPulse(2, 100ms);  // More delayed
}
```

**Result**: A "cascade" of overlapping pulses, creating complex interference patterns in the LGP.

---

### Variation 3: Hue Modulation During Travel

Instead of fixed hue, modulate during travel:
```cpp
uint8_t hue = baseHue + (m_pulsePosition * 60);  // 60° hue shift during travel
```

**Result**: Pulse changes colour as it expands (e.g., red at centre → yellow at edge).

---

### Variation 4: Chord-Driven Colour

Replace palette walk with chord-based colour:
```cpp
uint8_t hue = chordToHue(ctx.audio.chordRoot);
```

**Result**: Pulse colour reflects current harmonic content (major = warm, minor = cool).

---

## Known Limitations

1. **No collision detection**: Overlapping pulses simply overwrite each other (replace mode). Consider additive compositing for better overlap handling.

2. **Fixed Gaussian width**: σ = 10 pixels is hardcoded. Consider making this adaptive (e.g., σ proportional to beat strength).

3. **Binary beat detection**: Threshold-based (on/off). Misses nuance of "how hard" the beat hit. Consider amplitude-modulated triggers.

4. **No secondary feature integration**: Ignores spectral content, chord changes, saliency. Could be enhanced with multi-dimensional audio mapping.

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Radial travelling ring (Seed A) |
| **Temporal driver** | Event detector (kick/snare) |
| **Spatial basis** | Gaussian pulse |
| **Colour policy** | Palette walk (global) |
| **Compositing mode** | Replace |
| **Anti-flicker** | Implicit (smooth Gaussian + exponential decay) |
| **LGP sensitivity** | High (interference patterns on rapid beats) |

---

## Related Effects

- [WaveReactiveEffect](WaveReactiveEffect.md) — Continuous RMS-driven variant (vs event-driven)
- [LGPSpectrumBarsEffect](LGPSpectrumBarsEffect.md) — Standing bars (vs travelling pulses)
- [StyleAdaptiveEffect](StyleAdaptiveEffect.md) — Morphs between pulse and glow based on musical style

---

**End of Layer Audit: LGPBeatPulseEffect**
