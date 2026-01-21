# Layer Audit: LGPChordGlowEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/LGPChordGlowEffect.cpp`  
**Category**: Audio-Reactive, Continuous  
**Family**: Diffuse Glow Field

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Diffuse "glow" fields (Seed C)  
**Driver**: Continuous (chordStability + chordRoot)  
**Spatial Pattern**: Centre-weighted Gaussian glow  
**Colour Policy**: Chord → hue table lookup  
**Compositing**: Replace mode

---

## Layer Breakdown

### Single Layer: Chord-Driven Glow

**State Variables**:
```cpp
float m_displayedStability = 0.0f;  // Attack/release filtered chord stability
uint8_t m_currentHue = 0;           // Smoothed hue (chord root)
```

**Update Cadence**: Per-frame

**Update Equation**:
```cpp
// Slow attack/release to avoid flicker on chord changes
float targetStability = ctx.audio.chordStability;
float rate = 0.1f;  // ~150ms time constant

m_displayedStability += (targetStability - m_displayedStability) * rate;
```

**Hue Update**:
```cpp
uint8_t targetHue = chordToHue(ctx.audio.chordRoot);
m_currentHue += angleDifference(targetHue, m_currentHue) * 0.1f;  // Smooth hue transition
```

---

## Audio Signals Used

| Signal | Usage | Range |
|--------|-------|-------|
| `ctx.audio.chordStability` | Glow intensity | [0.0, 1.0] (0 = chaotic, 1 = stable) |
| `ctx.audio.chordRoot` | Hue selection | Enum (C, C#, D, ..., B, NONE) |

**What is chordStability?**  
A measure of "how clear and stable" the detected chord is:
- **High stability** (0.8-1.0): Clear, sustained chord (e.g., held piano chord, synth pad)
- **Low stability** (0.0-0.2): No clear chord, dissonant, or rapidly changing (e.g., drum solo, noise)

**Detection method** (typical):
```
stability = correlationWithChordTemplate(spectrum) * temporalConsistency(recentChords)
```

**What is chordRoot?**  
The root note of the detected chord (C, D, E, F, G, A, B, and chromatic variants).

---

## Chord → Hue Mapping

**Colour Theory Basis**: Circle of fifths mapped to colour wheel.

```cpp
uint8_t chordToHue(ChordRoot root) {
    switch (root) {
        case C:  return 0;    // Red
        case G:  return 210;  // Blue-violet (5th above C)
        case D:  return 170;  // Cyan (5th above G)
        case A:  return 130;  // Green-cyan (5th above D)
        case E:  return 90;   // Yellow-green (5th above A)
        case B:  return 50;   // Yellow (5th above E)
        case F:  return 20;   // Orange (4th above C)
        // Chromatic variants...
        default: return 0;
    }
}
```

**Why circle of fifths?**  
- Musically consonant intervals (perfect 5th) map to perceptually harmonious hue intervals
- Creates a "harmonic colour palette" where chord progressions feel visually coherent

**Example**:
- C → G progression: Red → Blue-violet (complementary hues, visually striking)
- C → F progression: Red → Orange (analogous hues, smooth transition)

---

## Spatial Rendering

**Centre-Weighted Gaussian**:
```cpp
for (int d = 0; d < 80; d++) {
    float weight = gaussian(d, 20.0f);  // σ = 20 pixels (wide glow)
    float brightness = m_displayedStability * weight;
    
    CHSV colour(m_currentHue, 255, brightness * 255);
    SET_CENTER_PAIR(d, colour);
}
```

**Why centre-weighted?**  
- Chords are "harmonic centres" → visual centre
- Creates "halo" effect where glow is brightest at centre, fades toward edges
- LGP amplifies this with natural scattering (centre LEDs illuminate more LGP volume)

---

## Colour Policy

### Chord-Driven Hue (Semantic Mapping)

**Saturation**: Fixed at 255 (fully saturated) when stable chord is present.

**Brightness**: Directly proportional to `chordStability`.

**Visibility Floor**: None (on chaotic passages, effect naturally fades to black).

---

## LGP Considerations

### Slow Attack/Release

**Attack** (150ms):
- Chord changes "bloom" gradually
- Avoids jarring colour flashes on sudden modulations

**Release** (150ms):
- Chords "linger" briefly after ending
- Creates sustain/reverb-like visual tail

**Why symmetric** (unlike SaliencyAwareEffect's asymmetric envelope)?  
Chord changes are typically deliberate musical events (not percussive transients), so symmetric envelope feels more natural.

### Hue Transitions

**Smooth hue interpolation**:
- Avoids "rainbow flash" during chord changes
- Shortest path around colour wheel (e.g., red → orange → yellow, not red → violet → blue → yellow)

**Effect on LGP**:
- During chord transitions, LGP displays gradient between old and new hue
- Creates "breathing" colour shift that feels organic

---

## Performance Characteristics

**Heap Allocations**: None.

**Per-Frame Cost**:
- 1× stability envelope update
- 1× hue interpolation
- 80× Gaussian evaluations + pixel writes

**Typical FPS Impact**: ~0.1 ms per frame.

---

## Composability and Variations

### Variation 1: Chord Quality (Major/Minor) → Saturation

Modulate saturation by chord quality:
```cpp
if (ctx.audio.chordQuality == MAJOR) {
    saturation = 255;  // Vivid (happy, bright)
} else if (ctx.audio.chordQuality == MINOR) {
    saturation = 180;  // Desaturated (sad, muted)
}
```

**Result**: Major chords appear vivid; minor chords appear pastel.

---

### Variation 2: Chord Tension → Spatial Compression

Compress glow toward centre on tense chords (e.g., diminished):
```cpp
float sigma = lerp(10.0f, 30.0f, 1.0f - ctx.audio.chordTension);
```

**Result**: Consonant chords spread wide; dissonant chords compress tightly at centre.

---

### Variation 3: Multi-Chord Polyphony

Detect multiple simultaneous chords (e.g., bass + treble):
```cpp
uint8_t bassHue = chordToHue(ctx.audio.bassChord);
uint8_t trebleHue = chordToHue(ctx.audio.trebleChord);
// Render bass glow at centre, treble glow at edges
```

**Result**: Polyphonic harmony creates multi-coloured glow.

---

### Variation 4: Chord Progression Memory

Track chord history and create "afterglow":
```cpp
for (int i = 0; i < 3; i++) {  // Last 3 chords
    float age = (currentTime - m_chordHistory[i].timestamp) / 2000.0f;
    float opacity = exp(-age);  // Exponential fade
    renderGlow(m_chordHistory[i].hue, opacity);
}
```

**Result**: Recent chords remain faintly visible, creating harmonic "memory".

---

## Known Limitations

1. **Single chord detection**: Can't handle polyphony (multiple simultaneous chords). See Variation 3.

2. **Fixed Gaussian width**: Doesn't adapt to chord density or complexity.

3. **No chord prediction**: Reacts to current frame only. Could anticipate chord changes based on harmonic rhythm.

4. **Binary stability**: Threshold-based (chord present/absent). Could use fuzzy logic for "partially stable" chords.

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Diffuse glow field (Seed C) |
| **Temporal driver** | Continuous tracker (chord stability) |
| **Spatial basis** | Gaussian envelope (centre-weighted) |
| **Colour policy** | Chord → hue table (semantic mapping) |
| **Compositing mode** | Replace |
| **Anti-flicker** | Attack/release envelope (symmetric, 150ms) |
| **LGP sensitivity** | High (hue transitions create colour gradients) |

---

## Related Effects

- [SaliencyAwareEffect](SaliencyAwareEffect.md) — Harmonic saliency (vs chord detection)
- [StyleAdaptiveEffect](StyleAdaptiveEffect.md) — Style-driven hue (vs chord-driven)

---

**End of Layer Audit: LGPChordGlowEffect**
