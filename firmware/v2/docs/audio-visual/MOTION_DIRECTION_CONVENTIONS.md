# Motion Direction Conventions

**Authoritative Reference for Wave Propagation in LightwaveOS Effects**

This document establishes the canonical conventions for motion direction, phase relationships, and dt-correctness in all LED effects.

---

## 1. Wave Equation Sign Convention

### The Rule

```
OUTWARD motion (centre → edge):  sin(k*dist - ω*phase)
INWARD motion (edge → centre):   sin(k*dist + ω*phase)
```

Where:
- `dist` = distance from centre (0 at LEDs 79/80, increasing toward edges)
- `phase` = time-varying phase that **increases monotonically**
- `k` = spatial frequency (radians per LED)
- `ω` = angular velocity multiplier

### Why This Works

When `phase` increases each frame:
- **Minus sign (-)**: Constant phase surfaces move toward larger `dist` (OUTWARD)
- **Plus sign (+)**: Constant phase surfaces move toward smaller `dist` (INWARD)

### Quick Verification

> "If `m_phase` increases and you want waves moving OUTWARD, use the **minus** sign."

---

## 2. Centre Origin Geometry

### Physical Layout

```
Strip 1:  LED 0 ←───── LED 79 | LED 80 ─────→ LED 159
                       [CENTRE]
Strip 2:  LED 160 ←─── LED 239 | LED 240 ──→ LED 319
                       [CENTRE]
```

### Distance Mapping

```cpp
// From CoreEffects.h
centerPairDistance(79)  = 0   // Left centre
centerPairDistance(80)  = 0   // Right centre
centerPairDistance(0)   = 79  // Left edge
centerPairDistance(159) = 79  // Right edge
```

### Normalised Distance

```cpp
float dist01 = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);
// dist01 = 0.0 at centre, 1.0 at edges
```

---

## 3. Direction Selection Checklist

When implementing a new effect, choose direction based on:

| Condition | Direction | Sign |
|-----------|-----------|------|
| Metaphor suggests expansion (fountain, explosion, sunrise) | OUTWARD | `-` |
| Metaphor suggests contraction (collapse, gravity well, inhale) | INWARD | `+` |
| Interference pattern required (lattice, standing waves) | BOTH | Mixed |
| Following existing family convention | Match family | Match |
| Audio-reactive pulse (beat response) | Usually OUTWARD | `-` |

### Beat Pulse Family Convention

- **Stack**: Amplitude-driven, INWARD contraction (ring at edge contracts to centre)
- **Shockwave**: Amplitude-driven, OUTWARD expansion (ring at centre expands to edge)
- **Shockwave In**: Amplitude-driven, INWARD (same as Stack but different visual character)

---

## 4. DT-Correctness Requirements

### The Problem

Frame-rate dependent code looks correct at one FPS but breaks at others:
```cpp
// WRONG: Speed depends on frame rate
m_phase += 0.016f;  // Assumes 60 FPS
```

### The Solution

All motion must use delta-time scaling:
```cpp
// CORRECT: Frame-rate independent
const float dt = ctx.getSafeDeltaSeconds();
m_phase += speed * dt;
```

### Exponential Decay/Smoothing

For per-frame multipliers like `value *= 0.94`, use:
```cpp
// dt-correct decay (tuned for 60 FPS reference)
value *= powf(0.94f, dt * 60.0f);
```

For exponential smoothing (EMA filters):
```cpp
// dt-correct smoothing
const float smooth = 1.0f - powf(1.0f - smoothFactor, dt * 60.0f);
value += (target - value) * smooth;
```

### Exponential Decay (Continuous)

For mathematically precise decay:
```cpp
// Continuous exponential decay
const float decayRate = 1000.0f / decayMs;
value *= expf(-decayRate * dt);
```

---

## 5. Phase Accumulation Pattern

### Canonical Formula

```cpp
float speedNorm = ctx.speed / 50.0f;  // Normalise speed knob
m_phase += speedNorm * baseRate * dt;

// Wrap to prevent overflow (100 cycles = 628.3 radians)
if (m_phase > 628.3f) m_phase -= 628.3f;
```

### For sin8() (FastLED 8-bit sine)

```cpp
// Convert radians to 0-255 range
uint16_t phaseInt = static_cast<uint16_t>(m_phase * 40.58f);  // 255 / (2π)
uint8_t sineVal = sin8(phaseInt);
```

---

## 6. Bidirectional Effects (Interference)

### When to Use Both Directions

Use mixed signs when:
1. Creating standing wave patterns (nodes that don't travel)
2. Simulating interference (constructive/destructive)
3. Creating depth illusions (parallax layers)
4. Implementing physically-based phenomena (Chladni, Fabry-Perot)

### Example: Diamond Lattice

```cpp
float wave1 = sinf((dist01 + m_phase) * freq * TWO_PI);  // Inward
float wave2 = sinf((dist01 - m_phase) * freq * TWO_PI);  // Outward
float lattice = wave1 * wave2;  // Interference pattern
```

---

## 7. LGP-Specific Considerations

### Dual-Strip Phase Relationships

The LGP's dual edge-lit strips can be driven with phase offsets:

| Relationship | Strip 2 Offset | Visual Effect |
|--------------|----------------|---------------|
| In-phase | 0 | Uniform brightness |
| Anti-phase | π (180°) | Standing wave nodes on alternating edges |
| Quadrature | π/2 (90°) | Circular/rotating appearance |

### Spatial Frequency for Standing Waves

```cpp
// Control "box count" (number of standing wave nodes)
const float boxesPerSide = 6.0f;  // 3-12 typical
const float spatialFreq = boxesPerSide * PI / HALF_LENGTH;
float pattern = sinf(dist * spatialFreq + motionPhase);
```

### Exploiting Interference

The LGP creates natural interference when opposing edges emit light into the plate. Effects should:
1. Use centre-origin geometry (never linear 0→159 sweeps)
2. Consider how strip1/strip2 patterns combine optically
3. Use spatial frequencies that create visible box/node patterns

---

## 8. Common Mistakes

### Mistake 1: Wrong Sign for Intent

```cpp
// BUG: Says "outward" but uses wrong sign
float wave = sinf(dist * freq + m_phase);  // Actually moves INWARD
```

**Fix:** Use `-` for outward motion.

### Mistake 2: Frame-Rate Dependent Motion

```cpp
// BUG: Runs 2x fast at 120 FPS vs 60 FPS
m_phase += 0.05f;
```

**Fix:** Multiply by `dt` and scale appropriately.

### Mistake 3: Mismatched Layer Velocities

```cpp
// BUG: Layers moving at incompatible speeds create visual noise
float layer1 = sinf(dist * 3.0f - m_phase * 30.0f);   // Fast
float layer2 = sinf(dist * 10.0f - m_phase * 0.3f);   // Slow
```

**Fix:** Use coherent velocity ratios (1:1, 2:1, etc.) or document intentional mismatch.

### Mistake 4: Wagon Wheel Aliasing

```cpp
// BUG: 3:1 frequency ratio causes apparent reverse motion
float carrier = sinf(dist * k - m_phase);
float modulation = sinf(dist * 3.0f * k - m_phase);  // Aliasing!
```

**Fix:** Use frequency ratios that don't alias (avoid 3:1, 5:1 near Nyquist).

---

## 9. Reference Implementations

### Correct OUTWARD (Beat Pulse Shockwave)

```cpp
const float ringPos = 1.0f - htmlCentre;  // Expands from 0 to 1
const float wave = 1.0f - fminf(1.0f, fabsf(dist01 - ringPos) * 3.0f);
```

### Correct INWARD (Beat Pulse Stack)

```cpp
const float ringPos = htmlCentre;  // Contracts from ~0.6 to 0
const float wave = 1.0f - fminf(1.0f, fabsf(dist01 - ringPos) * 3.0f);
```

### Correct DT-Decay (BeatPulseHTML)

```cpp
static inline float decayMul(float dtSeconds) {
    return powf(0.94f, dtSeconds * 60.0f);
}
```

### Correct Standing Wave (LGPBoxWave)

```cpp
const float spatialFreq = boxesPerSide * PI / HALF_LENGTH;
float pattern = sinf(dist * spatialFreq + motionPhase);
pattern = tanhf(pattern * 2.0f);  // Sharpen to square wave
```

---

## 10. Validation

Before merging a new effect, verify:

- [ ] Phase increases monotonically (no random jumps)
- [ ] Sign matches intended direction (test visually)
- [ ] All motion uses `ctx.getSafeDeltaSeconds()`
- [ ] Decay/smoothing uses `powf()` or `expf()` with dt
- [ ] Multi-layer effects have coherent velocity relationships
- [ ] Centre-origin geometry used (no linear sweeps)
- [ ] Spatial frequencies don't cause aliasing
