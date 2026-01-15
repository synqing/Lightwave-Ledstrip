# Layer Audit: StyleAdaptiveEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/StyleAdaptiveEffect.cpp`  
**Category**: Audio-Reactive, Adaptive  
**Family**: Morph Field

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Adaptive "morph" fields (Seed D)  
**Driver**: Hybrid (event + continuous, style-aware)  
**Spatial Pattern**: Morphs between "calm diffuse glow" and "aggressive fast pulses"  
**Colour Policy**: Style-driven hue selection  
**Compositing**: Alpha blend (cross-fade between patterns)

---

## Layer Breakdown

### Pattern A: Calm Diffuse Glow

**Active when**: `musicStyle == CALM` or `energyLevel < 0.3`

**Spatial Form**: Centre-weighted Gaussian glow (like SaliencyAwareEffect)

**State Variables**:
```cpp
float m_calmGlowIntensity = 0.0f;
```

**Update**:
```cpp
m_calmGlowIntensity = ctx.audio.rms * (1.0f - ctx.audio.energyHigh);
```

---

### Pattern B: Aggressive Fast Pulses

**Active when**: `musicStyle == AGGRESSIVE` or `energyLevel > 0.7`

**Spatial Form**: Rapid radial pulses (like LGPBeatPulseEffect, but faster)

**State Variables**:
```cpp
float m_aggressivePulsePosition = 0.0f;
float m_aggressivePulseAmplitude = 0.0f;
```

**Update**:
```cpp
if (ctx.audio.kickDetected) {
    m_aggressivePulsePosition = 0.0f;
    m_aggressivePulseAmplitude = 1.0f;
}
m_aggressivePulsePosition += deltaTime / 250.0f;  // Faster than LGPBeatPulse (250ms vs 400ms)
```

---

### Morph Controller

**State Variables**:
```cpp
float m_morphAlpha = 0.0f;  // [0.0 = Pattern A, 1.0 = Pattern B]
```

**Update Equation**:
```cpp
float targetAlpha = 0.0f;

if (ctx.audio.musicStyle == AGGRESSIVE) {
    targetAlpha = ctx.audio.energyHigh * 0.7f + ctx.audio.tempoNormalized * 0.3f;
} else if (ctx.audio.musicStyle == CALM) {
    targetAlpha = (1.0f - ctx.audio.energyHigh) * 0.8f;
} else {  // NEUTRAL
    targetAlpha = 0.5f;  // Equal blend
}

// Smooth transition (avoids jarring jumps)
float transitionRate = 0.002f;  // ~500ms transition time
m_morphAlpha += (targetAlpha - m_morphAlpha) * transitionRate;
```

---

## Audio Signals Used

| Signal | Usage | Effect |
|--------|-------|--------|
| `ctx.audio.musicStyle` | Pattern selection | CALM → glow, AGGRESSIVE → pulses |
| `ctx.audio.energyHigh` | Morph weight | High energy → more Pattern B |
| `ctx.audio.tempoNormalized` | Morph weight (aggressive) | Fast tempo → more Pattern B |
| `ctx.audio.kickDetected` | Pulse trigger (Pattern B) | Triggers pulse |
| `ctx.audio.rms` | Glow intensity (Pattern A) | Brightness modulation |

**What is musicStyle?**  
High-level classification of musical "mood":
- **CALM**: Slow, low energy, minimal percussion (e.g., ambient, ballad)
- **AGGRESSIVE**: Fast, high energy, heavy percussion (e.g., EDM, metal)
- **NEUTRAL**: Everything in between

**Detection method** (typical):
```
style = classifyStyle(tempo, energyDistribution, percussiveRatio, spectralCentroid)
```

---

## Colour Policy

### Style-Driven Hue

```cpp
uint8_t hue = 0;
if (ctx.audio.musicStyle == CALM) {
    hue = 160;  // Cyan/blue (cool, serene)
} else if (ctx.audio.musicStyle == AGGRESSIVE) {
    hue = 0;    // Red (hot, intense)
} else {
    hue = 96;   // Green (neutral)
}

// Smooth hue transition
m_currentHue += angleDifference(hue, m_currentHue) * 0.05f;
```

**Why specific hues?**  
Perceptual associations:
- Red = energy, danger, excitement
- Blue = calm, cool, introspective
- Green = balance, neutral

---

## LGP Considerations

### Alpha Blending

**Rendering**:
```cpp
// Render Pattern A
CRGB patternA = renderCalmGlow();

// Render Pattern B
CRGB patternB = renderAggressivePulse();

// Cross-fade
CRGB output = patternA * (1.0f - m_morphAlpha) + patternB * m_morphAlpha;
SET_CENTER_PAIR(d, output);
```

**Effect on LGP**:
- During transition, both patterns are visible simultaneously
- LGP blends them optically (additive light mixing)
- Creates "breathing" effect as system adapts to musical changes

### Smooth Transitions

**Why slow transition rate (500ms)?**  
- Musical style changes are infrequent (typically every 10-30 seconds)
- Rapid morphing would be distracting
- Slow cross-fade feels organic, like the system is "listening" and adapting

---

## Performance Characteristics

**Heap Allocations**: None.

**Per-Frame Cost**:
- 2× pattern rendering (Pattern A + Pattern B)
- 80× alpha blending operations

**Typical FPS Impact**: ~0.2 ms per frame (highest of all effects due to dual rendering).

---

## Composability and Variations

### Variation 1: N-Way Morph

Extend beyond binary calm/aggressive:
```cpp
enum MusicMood { CALM, NEUTRAL, ENERGETIC, AGGRESSIVE, CHAOTIC };
// Render all 5 patterns, blend with fuzzy logic weights
```

---

### Variation 2: Parameter Space Morphing

Instead of morphing patterns, morph *parameters* of a single pattern:
```cpp
float pulseSpeed = lerp(400ms, 150ms, m_morphAlpha);  // Calm = slow, aggressive = fast
float glowSigma = lerp(30px, 10px, m_morphAlpha);     // Calm = wide, aggressive = narrow
```

**Result**: Smoother visual continuity (same pattern, different "feel").

---

### Variation 3: Anticipatory Morphing

Predict style changes before they happen:
```cpp
if (detectBuildUp(ctx.audio)) {
    m_morphAlpha += 0.01f;  // Pre-morph toward aggressive
}
```

**Result**: Effect "prepares" for drops/climaxes, feels more musically aware.

---

## Known Limitations

1. **Binary morph**: Only two patterns. Real music has more nuance (see Variation 1).

2. **Fixed transition rate**: Could be adaptive (faster transitions on abrupt style changes).

3. **No memory**: Doesn't learn user preferences or adapt over time.

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Morph field (Seed D) |
| **Temporal driver** | Hybrid (event + continuous) |
| **Spatial basis** | Dual (Gaussian glow + radial pulse) |
| **Colour policy** | Style-driven hue |
| **Compositing mode** | Alpha blend |
| **Anti-flicker** | Slow morph rate (500ms transitions) |
| **LGP sensitivity** | High (dual patterns create complex visuals during morph) |

---

## Related Effects

- [SaliencyAwareEffect](SaliencyAwareEffect.md) — Calm glow component
- [LGPBeatPulseEffect](LGPBeatPulseEffect.md) — Aggressive pulse component

---

**End of Layer Audit: StyleAdaptiveEffect**
