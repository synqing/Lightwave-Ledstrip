# Troubleshooting Guide

**Document Version:** 1.0.0
**Last Updated:** 2025-12-31
**Companion To:** AUDIO_OUTPUT_SPECIFICATIONS.md, VISUAL_PIPELINE_MECHANICS.md, IMPLEMENTATION_PATTERNS.md

This guide covers common issues encountered when developing audio-visual effects and their solutions, based on real debugging sessions.

---

## Table of Contents

1. [Motion Jitter Issues](#1-motion-jitter-issues)
2. [Motion Direction Problems](#2-motion-direction-problems)
3. [Color and Brightness Issues](#3-color-and-brightness-issues)
4. [Timing and Synchronization](#4-timing-and-synchronization)
5. [Memory and Performance](#5-memory-and-performance)
6. [Audio Pipeline Issues](#6-audio-pipeline-issues)
7. [Quick Reference Tables](#7-quick-reference-tables)

---

## 1. Motion Jitter Issues

### Symptom: Spastic/Jerky Motion

**Description:** Effect motion appears choppy, jittery, or nervous even with music that should produce smooth motion.

**Root Cause:** Too many smoothing layers stacked together, creating accumulated latency and phase conflicts.

**Example of WRONG Architecture:**
```
heavyBass() → rolling avg → AsymmetricFollower → Spring
             ↓              ↓                    ↓
           ~160ms          ~150ms              ~200ms = 630ms TOTAL LATENCY

The smoothing layers fight each other, causing oscillation and jitter.
```

**Solution:** Use `heavy_bands` directly into Spring - NO extra smoothing.

```cpp
// WRONG - causes jitter
float energy = m_rollingAvg.update(ctx.audio.heavyBass(), dt);
energy = m_follower.update(energy, dt);
float smoothedSpeed = m_spring.update(0.6f + 0.8f * energy, dt);

// CORRECT - smooth motion
float heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                     ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
float smoothedSpeed = m_spring.update(0.6f + 0.8f * heavyEnergy, dt);
```

**Why This Works:** `heavy_bands[]` are already smoothed by ControlBus (80ms rise / 15ms fall). The Spring adds natural momentum. Total latency: ~200ms.

---

### Symptom: Motion "Catches Up" in Lurches

**Description:** Effect seems to lag behind the music, then suddenly catches up in jerky bursts.

**Root Cause:** Excessive smoothing latency (600ms+) causes phase to accumulate, then release suddenly.

**Solution:**
1. Check total smoothing chain latency
2. Remove intermediate smoothing layers
3. Use direct `heavy_bands` for speed modulation

---

### Symptom: Motion Lurches on Bass Hits

**Description:** Bass hits cause sudden jumps in animation speed rather than smooth acceleration.

**Root Cause:** Using raw audio bands without Spring physics.

**Solution:** Always use Spring for speed modulation:

```cpp
// Initialize spring with stiffness=50 (critically damped)
m_speedSpring.init(50.0f, 1.0f);
m_speedSpring.reset(1.0f);

// In render:
float targetSpeed = 0.6f + 0.8f * heavyEnergy;
float smoothedSpeed = m_speedSpring.update(targetSpeed, dt);
```

| Spring Stiffness | Response Time | Feel |
|------------------|---------------|------|
| 25 | ~400ms | Sluggish, dreamy |
| 50 | ~200ms | Balanced (recommended) |
| 100 | ~100ms | Snappy, reactive |

---

## 2. Motion Direction Problems

### Symptom: Waves Moving Edge-to-Center Instead of Center-to-Edge

**Description:** Audio-reactive patterns appear to collapse inward instead of expanding outward.

**Root Cause:** Phase sign error in wave equation.

**Correct Formula for OUTWARD Motion:**
```cpp
// OUTWARD motion: sin(k*dist - phase)
// When phase increases, wavefronts move toward larger dist values (edges)
float wave = sinf(distFromCenter * frequency - m_phase);

// INWARD motion: sin(k*dist + phase)
// When phase increases, wavefronts move toward smaller dist values (center)
float wave = sinf(distFromCenter * frequency + m_phase);
```

**Quick Test:** If `m_phase` increases each frame, waves should move OUTWARD with minus sign.

---

### Symptom: Animation Appears Static or Stuck

**Description:** The pattern shows no motion despite audio input.

**Checklist:**
1. **Phase advancing?** Check `m_phase += speedNorm * 240.0f * speedMult * dt;`
2. **speedMult > 0?** Check Spring output isn't clamped to 0
3. **dt correct?** Verify `ctx.getSafeDeltaSeconds()` returns reasonable value (0.008-0.016s)
4. **Audio available?** Check `ctx.audio.available` is true
5. **Phase used in render?** Verify phase is used in wave calculation

---

### Symptom: Pattern Only Animates on First Strip

**Description:** Strip 1 shows animation, Strip 2 is static or different.

**Solution:** Ensure Strip 2 rendering uses same phase and parameters:

```cpp
for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    uint8_t brightness = calculateBrightness(i, m_phase);

    ctx.leds[i] = ctx.palette.getColor(hue, brightness);

    // Strip 2 must use SAME brightness calculation
    if (i + STRIP_LENGTH < ctx.ledCount) {
        ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
            (uint8_t)(hue + 128), brightness);  // Only hue differs
    }
}
```

---

## 3. Color and Brightness Issues

### Symptom: Washed Out / Muddy Colors

**Description:** Colors appear desaturated, grayish, or muddy instead of vibrant.

**Possible Causes:**
1. `PAL_WHITE_HEAVY` palette without ColorCorrectionEngine
2. Brightness multiplied incorrectly
3. Too many `qadd8()` calls accumulating white

**Solutions:**

```cpp
// 1. Check if palette needs correction
if (paletteHasFlag(paletteIndex, PAL_WHITE_HEAVY)) {
    ColorCorrectionEngine::getInstance().correctPalette(palette, flags);
}

// 2. Use scale8 instead of multiplication
uint8_t brightness = scale8(baseBrightness, gainFactor);  // NOT: baseBrightness * gain

// 3. Clear buffer each frame to prevent accumulation
fadeToBlackBy(ctx.leds, ctx.ledCount, 45);  // Higher = faster decay
```

---

### Symptom: Color Correction Breaking Interference Patterns

**Description:** LGP interference effects look wrong after ColorCorrectionEngine was added.

**Root Cause:** Color correction modifies amplitude relationships that interference patterns depend on.

**Solution:** Skip color correction for LGP-sensitive effects:

```cpp
// In PatternRegistry.cpp
bool shouldSkipColorCorrection(uint8_t effectId) {
    // LGP-sensitive effects
    if (isLGPSensitive(effectId)) return true;

    // Stateful effects that read previous buffer
    if (isStatefulEffect(effectId)) return true;

    return false;
}
```

**Effects that should skip:**
- INTERFERENCE family
- ADVANCED_OPTICAL family with CENTER_ORIGIN
- Stateful effects (Confetti, Ripple)
- PHYSICS_BASED, MATHEMATICAL families

---

### Symptom: Colors Flickering Between Two Hues

**Description:** Hue appears to alternate rapidly between two values.

**Root Cause:** Unsmoothed dominant bin jumping between adjacent chroma bins.

**Solution:** Smooth the dominant bin over 250ms:

```cpp
// Don't use raw bin directly
// WRONG: uint8_t chromaOffset = m_dominantBin * (255 / 12);

// Smooth bin transitions
float alphaBin = 1.0f - expf(-dt / 0.25f);  // 250ms time constant
m_dominantBinSmooth += ((float)m_dominantBin - m_dominantBinSmooth) * alphaBin;
uint8_t chromaOffset = (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f));
```

---

## 4. Timing and Synchronization

### Symptom: Effect Runs at Wrong Speed on Different Frame Rates

**Description:** Animation is too fast at 120 FPS, too slow at 60 FPS.

**Root Cause:** Not using delta time for animation.

**Solution:** Always multiply by `dt`:

```cpp
// WRONG - frame-rate dependent
m_phase += 0.1f;  // Too fast at 120 FPS, too slow at 30 FPS

// CORRECT - frame-rate independent
float dt = ctx.getSafeDeltaSeconds();
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
```

---

### Symptom: Beat Sync Drifting Over Time

**Description:** Effect starts synced to music but gradually drifts out of sync.

**Root Cause:** Using raw audio timing instead of K1 beat tracker.

**Solution:** Use `ctx.audio.beatPhase()` for beat-locked animation:

```cpp
// WRONG - accumulates drift
m_beatPhase += dt * estimatedBPM / 60.0f;

// CORRECT - K1 beat tracker handles drift correction
float beatPhase = ctx.audio.beatPhase();  // Always 0.0-1.0
```

---

### Symptom: Audio Lag Behind Video

**Description:** Visual effects respond noticeably late relative to audio.

**Possible Causes:**
1. Too many smoothing layers
2. Using slow smoothing time constants
3. Processing audio at wrong rate

**Checklist:**
- Speed path: `heavy_bands` → Spring only (~200ms)
- Brightness path: single AsymmetricFollower (50ms rise / 300ms fall)
- Not using `heavyBass()` through extra filters

---

## 5. Memory and Performance

### Symptom: Frame Drops During Effect

**Description:** FPS drops below 60 when certain effects are active.

**Common Causes:**

| Cause | Detection | Fix |
|-------|-----------|-----|
| Allocation in render loop | Memory profiler shows heap activity | Pre-allocate all buffers in `init()` |
| Floating point overflow | Values become NaN or Inf | Clamp all computed values |
| Excessive `qadd8()` calls | Profile shows function hotspot | Batch color operations |
| ColorCorrectionEngine | ~1.5ms per frame | Skip for eligible effects |

**Example Fixes:**

```cpp
// WRONG - allocates each frame
void render(EffectContext& ctx) {
    std::vector<float> temp(160);  // Allocation every frame!
}

// CORRECT - pre-allocate
class MyEffect {
    float m_temp[160];  // Allocated once at effect creation
};

// WRONG - unclamped values
m_phase += speedMult * 1000.0f;  // Can grow unbounded

// CORRECT - clamped with wrap
m_phase += speedMult * 240.0f * dt;
if (m_phase > 628.3f) m_phase -= 628.3f;
```

---

### Symptom: Heap Corruption / Random Crashes

**Description:** Effect crashes randomly or corrupts other memory.

**Common Causes:**

1. **Buffer overrun:** Accessing `ctx.leds[i + STRIP_LENGTH]` when `i` already at end
2. **Integer overflow:** `uint8_t` math wrapping unexpectedly
3. **Uninitialized members:** Using member variables before `init()` called

**Defensive Patterns:**

```cpp
// Always check bounds
for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    ctx.leds[i] = color;

    // Check before accessing Strip 2
    if (i + STRIP_LENGTH < ctx.ledCount) {
        ctx.leds[i + STRIP_LENGTH] = color2;
    }
}

// Use constants, not magic numbers
#define STRIP_LENGTH 160
#define HALF_LENGTH 80

// Initialize ALL member variables in init()
bool MyEffect::init(EffectContext& ctx) {
    m_phase = 0.0f;
    m_lastHopSeq = 0;
    // ... initialize everything
    return true;
}
```

---

## 6. Audio Pipeline Issues

### Symptom: No Audio Response at All

**Description:** Effect shows no reaction to music despite audio being present.

**Diagnostic Checklist:**

```cpp
// 1. Check if audio feature is enabled
#if FEATURE_AUDIO_SYNC
    // Audio code here
#endif

// 2. Check audio availability at runtime
if (!ctx.audio.available) {
    // Audio not available - use fallback behavior
    return;
}

// 3. Check hop sequence advancing
static uint32_t lastSeq = 0;
if (ctx.audio.controlBus.hop_seq != lastSeq) {
    lastSeq = ctx.audio.controlBus.hop_seq;
    // New audio data available
}

// 4. Check if audio values are non-zero
float bass = ctx.audio.heavyBass();
if (bass < 0.001f) {
    // Audio may be silent or not connected
}
```

---

### Symptom: Audio Response Too Weak

**Description:** Effect barely responds to loud music.

**Solutions:**

```cpp
// 1. Check gain factors
float brightGain = 0.4f + 0.6f * energyAvg;  // Minimum 0.4
if (brightGain < 0.3f) brightGain = 0.3f;    // Floor

// 2. Use squared energy for more contrast
float energy = ctx.audio.heavyBass();
energy = energy * energy;  // Squared response

// 3. Increase spawn chance multipliers
float chance = energyDelta * 510.0f + energyAvg * 80.0f;  // Aggressive
// was: energyDelta * 200.0f + energyAvg * 40.0f
```

---

### Symptom: Audio Response Too Sensitive / Noisy

**Description:** Effect responds to every tiny audio fluctuation.

**Solutions:**

```cpp
// 1. Use heavy_bands instead of regular bands
float energy = ctx.audio.controlBus.heavy_bands[1];  // Pre-smoothed
// NOT: ctx.audio.controlBus.bands[1]  // Raw, noisy

// 2. Increase smoothing time constants
m_energyFollower = AsymmetricFollower{0.0f, 0.08f, 0.30f};  // Slower
// was: AsymmetricFollower{0.0f, 0.03f, 0.15f}

// 3. Add minimum threshold
if (energyDelta < 0.1f) energyDelta = 0.0f;  // Ignore small changes
```

---

## 7. Quick Reference Tables

### Smoothing Time Constants

| Component | Rise Time | Fall Time | Total Latency |
|-----------|-----------|-----------|---------------|
| heavy_bands (ControlBus) | 80ms | 15ms | Pre-applied |
| Spring (stiffness=50) | ~200ms | ~200ms | ~200ms |
| AsymmetricFollower (default) | 50ms | 300ms | 50-300ms |
| Chroma bin smoothing | 250ms | 250ms | 250ms |

### Effect Category Skip Rules

| Category | Skip ColorCorrection | Reason |
|----------|---------------------|--------|
| INTERFERENCE | Yes | Precise amplitude for wave interference |
| ADVANCED_OPTICAL | Yes (if CENTER_ORIGIN) | LGP-sensitive patterns |
| PHYSICS_BASED | Yes | Physics simulations need exact values |
| MATHEMATICAL | Yes | Mathematical mappings need exact RGB |
| Stateful (Confetti, Ripple) | Yes | Read previous frame buffer |
| All others | No | Benefit from correction |

### Audio Accessor Performance

| Accessor | Cost | When to Use |
|----------|------|-------------|
| `heavyBass()` | ~1µs | Speed modulation |
| `bass()` | ~1µs | Beat tracking |
| `getBand(i)` | ~1µs | Frequency analysis |
| `bin(i)` | ~1µs | 64-bin fine analysis |
| `beatPhase()` | ~1µs | Beat-locked animation |
| `isSnareHit()` | ~1µs | Percussion triggers |
| `chroma[i]` | Direct access | Pitch-based color |

### Wave Motion Quick Reference

| Desired Motion | Phase Sign | Formula |
|---------------|------------|---------|
| Outward (center→edge) | Minus | `sin(k*dist - phase)` |
| Inward (edge→center) | Plus | `sin(k*dist + phase)` |
| Standing wave | No phase | `sin(k*dist) * envelope` |
| Traveling (left→right) | Use position | `sin(k*pos - phase)` |

---

## Emergency Debug Checklist

When an effect isn't working, check these in order:

1. **Does it compile?** Check for syntax errors
2. **Is init() called?** Set breakpoint or add log
3. **Is render() called?** Check effect is registered and active
4. **Is audio available?** Log `ctx.audio.available`
5. **Is hop_seq advancing?** Log `ctx.audio.controlBus.hop_seq`
6. **Is phase advancing?** Log `m_phase` each frame
7. **Are LED values non-zero?** Log sample LED values
8. **Is brightness too low?** Check all scale8/multiply operations
9. **Is palette valid?** Check `ctx.palette.isValid()`
10. **Buffer overrun?** Check all array access bounds

---

## Related Documentation

- **[AUDIO_OUTPUT_SPECIFICATIONS.md](AUDIO_OUTPUT_SPECIFICATIONS.md)** - Audio data structures
- **[VISUAL_PIPELINE_MECHANICS.md](VISUAL_PIPELINE_MECHANICS.md)** - Render architecture
- **[COLOR_PALETTE_SYSTEM.md](COLOR_PALETTE_SYSTEM.md)** - Palette and color correction
- **[IMPLEMENTATION_PATTERNS.md](IMPLEMENTATION_PATTERNS.md)** - Working code examples
