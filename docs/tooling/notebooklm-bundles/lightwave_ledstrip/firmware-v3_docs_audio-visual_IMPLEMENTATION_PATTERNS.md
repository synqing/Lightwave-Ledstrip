# Implementation Patterns

**Document Version:** 1.0.0
**Last Updated:** 2025-12-31
**Companion To:** AUDIO_OUTPUT_SPECIFICATIONS.md, VISUAL_PIPELINE_MECHANICS.md, COLOR_PALETTE_SYSTEM.md

This document provides complete, copy-paste ready implementation patterns from proven working effects. Each pattern includes full code examples with annotations.

---

## Table of Contents

1. [Audio-Reactive Speed Pattern](#1-audio-reactive-speed-pattern)
2. [Energy Delta Detection (Onset Triggering)](#2-energy-delta-detection-onset-triggering)
3. [Phase Accumulation Pattern](#3-phase-accumulation-pattern)
4. [CENTER ORIGIN Rendering](#4-center-origin-rendering)
5. [Ripple Spawn System](#5-ripple-spawn-system)
6. [Smoothing Engine Usage](#6-smoothing-engine-usage)
7. [Chroma-Based Color Modulation](#7-chroma-based-color-modulation)
8. [Dual-Strip Rendering](#8-dual-strip-rendering)
9. [Beat-Synchronized Effects](#9-beat-synchronized-effects)
10. [Complete Effect Template](#10-complete-effect-template)

---

## 1. Audio-Reactive Speed Pattern

**Source:** `ChevronWavesEffect.cpp`, `LGPPhotonicCrystalEffect.cpp`

This is the proven pattern for audio-reactive motion without jitter. The key insight: use `heavy_bands` directly into a Spring - **no extra smoothing layers**.

### Critical Architecture

```
WRONG (causes jitter - ~630ms total latency):
  heavyBass() → rolling avg → AsymmetricFollower → Spring
               ↓              ↓                    ↓
             160ms           150ms               ~200ms = 630ms total

CORRECT (smooth motion - ~200ms total latency):
  heavy_bands[1..2] → Spring ONLY
                      ↓
                    ~200ms = natural momentum
```

### Complete Implementation

```cpp
// From ChevronWavesEffect.cpp:111-125
// Source: v2/src/effects/ieffect/ChevronWavesEffect.cpp

// =============================================================================
// AUDIO-REACTIVE SPEED MODULATION (PROVEN PATTERN)
// =============================================================================
// Key insight: heavy_bands are PRE-SMOOTHED by ControlBus (80ms rise / 15ms fall)
// Adding more smoothing layers creates latency and makes motion feel disconnected.
// Spring physics provides natural momentum without extra filtering.

// Member variables (in header):
//   enhancement::Spring m_phaseSpeedSpring;

// In init():
//   m_phaseSpeedSpring.init(50.0f, 1.0f);  // stiffness=50, critically damped
//   m_phaseSpeedSpring.reset(1.0f);        // Start at base speed

// In render():
float heavyEnergy = 0.0f;
#if FEATURE_AUDIO_SYNC
if (ctx.audio.available) {
    // Use heavy_bands directly - they're already smoothed by ControlBus
    // Bands 1 and 2 = 300-1200 Hz (rhythmic bass and low-mids)
    heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                   ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
}
#endif

// Map energy to speed range (0.6-1.8x for stability)
float targetSpeed = 0.6f + 1.2f * heavyEnergy;

// Spring physics adds natural momentum - no jitter, no lag
float dt = ctx.getSafeDeltaSeconds();
float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, dt);

// Hard clamp for stability (prevents runaway values)
if (smoothedSpeed > 2.0f) smoothedSpeed = 2.0f;
if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;

// Use smoothedSpeed in phase advancement (see Pattern #3)
float speedNorm = ctx.speed / 50.0f;
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
```

### Why This Works

| Component | Latency | Purpose |
|-----------|---------|---------|
| `heavy_bands[]` | Pre-smoothed | ControlBus does 80ms rise / 15ms fall |
| Spring physics | ~200ms response | Natural momentum, no overshoot |
| **Total** | **~200ms** | Feels immediate but smooth |

---

## 2. Energy Delta Detection (Onset Triggering)

**Source:** `RippleEffect.cpp`, `ChevronWavesEffect.cpp`

This pattern detects sudden increases in energy above the rolling baseline - essential for triggering effects on beats and onsets.

### Complete Implementation

```cpp
// From ChevronWavesEffect.cpp:29-90
// Source: v2/src/effects/ieffect/ChevronWavesEffect.cpp

// =============================================================================
// 4-HOP ROLLING AVERAGE FOR ENERGY DELTA DETECTION
// =============================================================================
// Key insight: Energy ABOVE average (delta) is more useful than absolute energy.
// Delta > 0 means "something just happened" - perfect for triggering events.

// Member variables (in header):
static constexpr uint8_t CHROMA_HISTORY = 4;
float m_chromaEnergyHist[CHROMA_HISTORY] = {0};
float m_chromaEnergySum = 0.0f;
uint8_t m_chromaHistIdx = 0;
float m_energyAvg = 0.0f;
float m_energyDelta = 0.0f;
uint32_t m_lastHopSeq = 0;
uint8_t m_dominantBin = 0;

// In render() - execute ONLY on new audio hops (every ~16ms):
bool newHop = false;
#if FEATURE_AUDIO_SYNC
if (ctx.audio.available) {
    newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;

        // =================================================================
        // STEP 1: Calculate current energy from 12-bin chroma
        // =================================================================
        const float led_share = 255.0f / 12.0f;  // Scale factor
        float chromaEnergy = 0.0f;
        float maxBinVal = 0.0f;
        uint8_t dominantBin = 0;

        for (uint8_t i = 0; i < 12; ++i) {
            float bin = ctx.audio.controlBus.chroma[i];
            float bright = bin * bin;        // Square for perceptual response
            bright *= 1.5f;                  // Boost factor
            if (bright > 1.0f) bright = 1.0f;

            // Track dominant bin for color offset
            if (bright > maxBinVal) {
                maxBinVal = bright;
                dominantBin = i;
            }
            chromaEnergy += bright * led_share;
        }

        // Normalize to [0.0, 1.0]
        float energyNorm = chromaEnergy / 255.0f;
        if (energyNorm < 0.0f) energyNorm = 0.0f;
        if (energyNorm > 1.0f) energyNorm = 1.0f;

        // =================================================================
        // STEP 2: Update rolling 4-hop average
        // =================================================================
        m_chromaEnergySum -= m_chromaEnergyHist[m_chromaHistIdx];  // Remove old
        m_chromaEnergyHist[m_chromaHistIdx] = energyNorm;           // Add new
        m_chromaEnergySum += energyNorm;
        m_chromaHistIdx = (m_chromaHistIdx + 1) % CHROMA_HISTORY;
        m_energyAvg = m_chromaEnergySum / CHROMA_HISTORY;

        // =================================================================
        // STEP 3: Calculate delta (positive only = onset detection)
        // =================================================================
        m_energyDelta = energyNorm - m_energyAvg;
        if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;  // Only positive

        // Store dominant bin for color modulation
        m_dominantBin = dominantBin;
    }
}
#endif

// Usage examples:
// - Spawn chance: float chance = m_energyDelta * 510.0f + m_energyAvg * 80.0f;
// - Brightness boost: float gain = 0.4f + 0.5f * m_energyAvg + 0.4f * m_energyDelta;
// - Color offset: uint8_t chromaOffset = m_dominantBin * (255 / 12);
```

### Energy Delta Use Cases

| Use Case | Formula | Typical Range |
|----------|---------|---------------|
| Spawn chance | `delta * 510 + avg * 80` | 0-255 |
| Brightness gain | `0.4 + 0.5*avg + 0.4*delta` | 0.4-1.3 |
| Speed boost | `0.6 + 0.8*delta` | 0.6-1.4 |
| Edge sharpness | `base + 4.0*avg` | 2.0-6.0 |

---

## 3. Phase Accumulation Pattern

**Source:** All working effects

The standardized formula for dt-based animation that works at any frame rate.

### The Formula

```cpp
// =============================================================================
// PHASE ACCUMULATION FORMULA (CRITICAL - DO NOT MODIFY)
// =============================================================================
// This formula is proven across all working effects.
//
// Components:
//   speedNorm = ctx.speed / 50.0f       → User's speed slider (0.02-1.0)
//   240.0f                               → Base speed in radians/second
//   smoothedSpeed                        → Audio modulation (0.3-2.0)
//   dt                                   → Frame time in seconds
//
// At speedNorm=1.0 and smoothedSpeed=1.0:
//   Phase advances 240 radians/second = ~38 cycles/second

float speedNorm = ctx.speed / 50.0f;
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;

// High-precision wrap (100 × 2π = 628.318...)
if (m_phase > 628.3f) m_phase -= 628.3f;
```

### Why 628.3?

```
2π radians = 1 complete cycle
100 cycles before wrap = 100 × 2π = 628.318...

Benefits:
- High precision for slow effects (no accumulated error)
- Works with sin(), sinf(), sin8()
- Phase differences remain stable across wraps
```

### Converting Phase to Integer for sin8()

```cpp
// sin8() expects 0-255, so scale appropriately
uint16_t phaseInt = (uint16_t)(m_phase * 0.408f);  // 256 / 628.3 ≈ 0.408

// Use in pattern generation
uint8_t wave = sin8((distFromCenter << 2) - (phaseInt >> 7));
```

---

## 4. CENTER ORIGIN Rendering

**Source:** All LGP effects

All effects **MUST** originate from the center (LEDs 79/80) and propagate outward.

### Helper Functions

```cpp
// From CoreEffects.h - defined as inline functions

// Get distance from center (0 at center, 79 at edges)
inline uint16_t centerPairDistance(uint16_t i) {
    // Center is at 79.5 (between LEDs 79 and 80)
    // Distance is symmetric: LED 0 → 79, LED 79 → 0, LED 80 → 0, LED 159 → 79
    if (i < HALF_LENGTH) {
        return HALF_LENGTH - 1 - i;  // 79, 78, 77, ... 0
    } else {
        return i - HALF_LENGTH;      // 0, 1, 2, ... 79
    }
}

// Set LED pair at distance from center (for CENTER_ORIGIN effects)
#define SET_CENTER_PAIR(ctx, dist, color) do { \
    uint16_t leftIdx = HALF_LENGTH - 1 - (dist);  \
    uint16_t rightIdx = HALF_LENGTH + (dist);      \
    if (leftIdx < HALF_LENGTH) ctx.leds[leftIdx] = (color); \
    if (rightIdx < STRIP_LENGTH) ctx.leds[rightIdx] = (color); \
} while(0)
```

### Standard Render Loop

```cpp
// =============================================================================
// CENTER ORIGIN RENDER LOOP (STANDARD PATTERN)
// =============================================================================

for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    // Distance from center (0 at LED 79/80, 79 at edges)
    uint16_t dist = centerPairDistance(i);

    // Phase offset based on distance (OUTWARD motion when phase increases)
    // sin(k*dist - phase) = outward motion
    // sin(k*dist + phase) = inward motion
    float wave = sinf(dist * frequency - m_phase);

    // Convert to brightness
    uint8_t brightness = (uint8_t)((wave * 0.5f + 0.5f) * 255.0f);

    // Apply palette and brightness
    ctx.leds[i] = ctx.palette.getColor(ctx.gHue + dist, brightness);
}
```

### Dual-Strip with Center Origin

```cpp
// Render Strip 1 and Strip 2 with complementary colors
for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    uint16_t dist = centerPairDistance(i);
    uint8_t palettePos = ctx.gHue + (dist << 1);
    uint8_t brightness = scale8(ctx.brightness, waveValue);

    // Strip 1: Base color
    ctx.leds[i] = ctx.palette.getColor(palettePos, brightness);

    // Strip 2: Complementary (+128 offset)
    if (i + STRIP_LENGTH < ctx.ledCount) {
        ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
            (uint8_t)(palettePos + 128),
            brightness
        );
    }
}
```

---

## 5. Ripple Spawn System

**Source:** `RippleEffect.cpp`

Complete ripple management with audio-reactive spawning, cooldowns, and rendering.

### Data Structures

```cpp
// From RippleEffect.h

struct Ripple {
    float radius;      // Current radius from center
    float speed;       // Expansion speed
    uint8_t hue;       // Palette index
    uint8_t intensity; // Brightness (fades as ripple ages)
    bool active;       // Is ripple currently active?
};

static constexpr uint8_t MAX_RIPPLES = 8;  // Maximum concurrent ripples
Ripple m_ripples[MAX_RIPPLES];
uint8_t m_spawnCooldown = 0;               // Prevents overlapping spawns
```

### Spawn Logic

```cpp
// From RippleEffect.cpp:166-243
// Source: v2/src/effects/ieffect/RippleEffect.cpp

// =============================================================================
// RIPPLE SPAWN PROBABILITY
// =============================================================================
// Key formula: chance = energyDelta * 510 + energyAvg * 80
// - High delta (onset): up to 510 chance
// - High average (sustained): up to 80 chance

float chanceF = energyDelta * 510.0f + energyAvg * 80.0f;
if (chanceF > 255.0f) chanceF = 255.0f;
uint8_t spawnChance = (uint8_t)chanceF;

// =============================================================================
// COOLDOWN-GATED SPAWN
// =============================================================================
if (m_spawnCooldown > 0) {
    m_spawnCooldown--;
}

if (spawnChance > 0 && m_spawnCooldown == 0 && random8() < spawnChance) {
    // Find inactive ripple slot
    for (int i = 0; i < MAX_RIPPLES; i++) {
        if (!m_ripples[i].active) {
            // Speed variation for visual interest
            float speedVariation = 0.5f + (random8() / 255.0f);
            speedVariation += (energyAvg * 0.3f + energyDelta * 0.2f);

            m_ripples[i].radius = 0;  // Start at center
            m_ripples[i].speed = speedScale * speedVariation;
            m_ripples[i].hue = (uint8_t)(dominantBin * (255.0f / 12.0f)) + ctx.gHue;
            m_ripples[i].intensity = (uint8_t)(100.0f + energyAvg * 155.0f);
            m_ripples[i].active = true;

            // Set cooldown (shorter with audio for responsiveness)
            m_spawnCooldown = hasAudio ? 1 : 4;
            break;  // Only spawn one ripple per check
        }
    }
}
```

### Ripple Update and Render

```cpp
// From RippleEffect.cpp:245-285

// =============================================================================
// RIPPLE UPDATE AND RENDER LOOP
// =============================================================================
for (int r = 0; r < MAX_RIPPLES; r++) {
    if (!m_ripples[r].active) continue;

    // Expand ripple
    m_ripples[r].radius += m_ripples[r].speed * (ctx.speed / 10.0f);

    // Deactivate if past edge
    if (m_ripples[r].radius > HALF_LENGTH) {
        m_ripples[r].active = false;
        continue;
    }

    // Draw ripple wavefront (3 LEDs wide)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float wavePos = (float)dist - m_ripples[r].radius;
        float waveAbs = fabsf(wavePos);

        if (waveAbs < 3.0f) {  // 3-LED wavefront width
            // Brightness falls off from center of wavefront
            uint8_t brightness = 255 - (uint8_t)(waveAbs * 85.0f);

            // Edge fade (prevents hard cutoff at strip edge)
            uint8_t edgeFade = (uint8_t)((HALF_LENGTH - m_ripples[r].radius)
                                         * 255.0f / HALF_LENGTH);
            brightness = scale8(brightness, edgeFade);
            brightness = scale8(brightness, m_ripples[r].intensity);

            // Add to radial buffer (accumulative, not overwrite)
            CRGB color = ctx.palette.getColor(
                m_ripples[r].hue + (uint8_t)dist,
                brightness);
            m_radial[dist].r = qadd8(m_radial[dist].r, color.r);
            m_radial[dist].g = qadd8(m_radial[dist].g, color.g);
            m_radial[dist].b = qadd8(m_radial[dist].b, color.b);
        }
    }
}

// Render radial buffer to LED strip (CENTER ORIGIN)
for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
    SET_CENTER_PAIR(ctx, dist, m_radial[dist]);
}
```

---

## 6. Smoothing Engine Usage

**Source:** `SmoothingEngine.h`

Complete guide to the three smoothing primitives.

### ExpDecay (Simple Smoothing)

```cpp
// =============================================================================
// ExpDecay - True Exponential Smoothing
// =============================================================================
// Use for: General value smoothing, position smoothing, color transitions
// Key: lambda = convergence rate (higher = faster)

#include "../enhancement/SmoothingEngine.h"

// Member variable:
enhancement::ExpDecay m_brightnessSmoother{0.0f, 5.0f};  // initial, lambda

// In render():
float targetBrightness = computeBrightness();
float smoothedBrightness = m_brightnessSmoother.update(targetBrightness, dt);

// Alternative: Create from time constant
enhancement::ExpDecay smoother = enhancement::ExpDecay::withTimeConstant(0.2f);
// ^ This creates lambda = 5.0 (reaches 63% in 0.2 seconds)
```

### Spring (Physics-Based)

```cpp
// =============================================================================
// Spring - Critically Damped Physics
// =============================================================================
// Use for: Speed modulation, position with momentum, natural motion
// Key: stiffness controls response speed, critical damping prevents overshoot

// Member variable:
enhancement::Spring m_speedSpring;

// In init():
m_speedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1
m_speedSpring.reset(1.0f);        // Start at base value

// In render():
float targetSpeed = 0.6f + 0.8f * energyInput;
float smoothedSpeed = m_speedSpring.update(targetSpeed, dt);

// Stiffness guide:
// - stiffness=25:  ~400ms response (sluggish, dreamy)
// - stiffness=50:  ~200ms response (balanced, recommended)
// - stiffness=100: ~100ms response (snappy, reactive)
```

### AsymmetricFollower (Fast Attack, Slow Release)

```cpp
// =============================================================================
// AsymmetricFollower - Sensory Bridge Pattern
// =============================================================================
// Use for: Audio-reactive brightness, energy envelopes
// Key: riseTau = fast attack, fallTau = slow release

// Member variable:
enhancement::AsymmetricFollower m_energyFollower{0.0f, 0.05f, 0.30f};
// initial=0.0, riseTau=50ms, fallTau=300ms

// In render():
float smoothedEnergy = m_energyFollower.update(currentEnergy, dt);

// With mood adjustment (0=reactive, 1=smooth):
float moodNorm = ctx.mood / 255.0f;
float smoothedEnergy = m_energyFollower.updateWithMood(currentEnergy, dt, moodNorm);

// Time constant guide:
// - Rise 0.02s / Fall 0.15s: Very reactive (drum tracking)
// - Rise 0.05s / Fall 0.30s: Balanced (general audio, recommended)
// - Rise 0.10s / Fall 0.50s: Smooth (ambient, sustained tones)
```

### Safe Delta Time

```cpp
// Always use safe delta time to prevent physics explosion
float dt = enhancement::getSafeDeltaSeconds(ctx.deltaTimeMs);
// Clamps to [0.001, 0.05] seconds (20-1000 FPS range)

// Or use the context helper:
float dt = ctx.getSafeDeltaSeconds();  // Same clamping
```

---

## 7. Chroma-Based Color Modulation

**Source:** `ChevronWavesEffect.cpp`, `LGPPhotonicCrystalEffect.cpp`

Use the 12-bin chroma analysis to drive color changes based on musical pitch.

### Dominant Bin Detection

```cpp
// =============================================================================
// CHROMA DOMINANT BIN DETECTION
// =============================================================================
// The 12 chroma bins map to musical notes (C, C#, D, ... B)
// Dominant bin = which pitch class has most energy = musical key hint

uint8_t m_dominantBin = 0;
float m_dominantBinSmooth = 0.0f;

// In render() on new hop:
if (newHop) {
    float maxChroma = 0.0f;
    for (uint8_t bin = 0; bin < 12; ++bin) {
        if (ctx.audio.controlBus.chroma[bin] > maxChroma) {
            maxChroma = ctx.audio.controlBus.chroma[bin];
            m_dominantBin = bin;
        }
    }
}

// Smooth the bin transitions (250ms time constant)
float alphaBin = 1.0f - expf(-dt / 0.25f);
m_dominantBinSmooth += ((float)m_dominantBin - m_dominantBinSmooth) * alphaBin;
if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;
```

### Applying to Palette

```cpp
// Map 12 bins to 256 palette positions
uint8_t chromaOffset = (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f));

// Add to base hue for pitch-reactive color
uint8_t hue = ctx.gHue + chromaOffset;
ctx.leds[i] = ctx.palette.getColor(hue, brightness);
```

### Chord-Based Color Shifts

```cpp
// Major/minor chord detection for warm/cool color shifts
int8_t chordShift = 0;
#if FEATURE_AUDIO_SYNC
if (ctx.audio.available && ctx.audio.chordConfidence() > 0.3f) {
    if (ctx.audio.isMajor()) {
        chordShift = +25;   // Warm shift for major chords
    } else if (ctx.audio.isMinor()) {
        chordShift = -25;   // Cool shift for minor chords
    }
}
#endif

uint8_t hue = ctx.gHue + chromaOffset + chordShift;
```

---

## 8. Dual-Strip Rendering

**Source:** All LGP effects

Patterns for rendering to both LED strips with visual relationships.

### Complementary Colors (+128)

```cpp
// =============================================================================
// DUAL-STRIP COMPLEMENTARY PATTERN
// =============================================================================
// Strip 2 shows colors 180 degrees opposite on the palette

for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    uint16_t dist = centerPairDistance(i);
    uint8_t palettePos = ctx.gHue + distFromCenter * 2;

    // Strip 1: Base palette position
    ctx.leds[i] = ctx.palette.getColor(palettePos, brightness);

    // Strip 2: Offset by 128 (complementary)
    if (i + STRIP_LENGTH < ctx.ledCount) {
        ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
            (uint8_t)(palettePos + 128),
            brightness
        );
    }
}
```

### Triadic Colors (+64/+90)

```cpp
// 90-degree offset creates triadic color harmony
ctx.leds[i] = ctx.palette.getColor(hue, brightness);
ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue + 90, brightness);
```

### Independent Zone Rendering

```cpp
// When strips should show different patterns entirely
// Render first strip normally
for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    ctx.leds[i] = renderPattern1(i);
}

// Render second strip with different logic
for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    ctx.leds[i + STRIP_LENGTH] = renderPattern2(i);
}
```

---

## 9. Beat-Synchronized Effects

**Source:** `RippleEffect.cpp`

Using K1 beat tracker for synchronized effects.

### Beat Phase Usage

```cpp
// =============================================================================
// BEAT PHASE ANIMATION
// =============================================================================
// ctx.audio.beatPhase() returns 0.0-1.0 representing position in beat
// 0.0 = on the beat, 0.5 = between beats, 1.0 = next beat

#if FEATURE_AUDIO_SYNC
if (ctx.audio.available) {
    float beatPhase = ctx.audio.beatPhase();

    // Example: Pulse brightness on beat
    float beatPulse = 1.0f - beatPhase;  // High at beat, decays
    beatPulse = beatPulse * beatPulse;   // Square for sharper decay

    // Example: Oscillate size with beat
    float beatSize = 0.5f + 0.5f * sinf(beatPhase * 2.0f * PI);
}
#endif
```

### Snare Hit Triggering

```cpp
// =============================================================================
// PERCUSSION TRIGGER (SNARE HIT)
// =============================================================================
// ctx.audio.isSnareHit() returns true for ONE FRAME on detection

#if FEATURE_AUDIO_SYNC
if (ctx.audio.available && ctx.audio.isSnareHit()) {
    // Trigger one-shot effect
    m_flashIntensity = 1.0f;

    // Or spawn a ripple
    spawnRippleAtCenter();

    // Or increase edge sharpness temporarily
    m_edgeBoost = 5.0f;
}
#endif

// Decay the trigger effect each frame
m_flashIntensity *= 0.88f;  // ~80ms decay at 60fps
```

### Sub-Bass Kick Detection

```cpp
// =============================================================================
// 64-BIN SUB-BASS KICK (BINS 0-5 = 110-155 Hz)
// =============================================================================
// Deep bass that the 12-bin chromagram misses entirely
// Gives powerful punch on bass drops

float kickSum = 0.0f;
for (uint8_t i = 0; i < 6; ++i) {
    kickSum += ctx.audio.bin(i);
}
float kickAvg = kickSum / 6.0f;

// Asymmetric envelope: instant attack, fast decay
if (kickAvg > m_kickPulse) {
    m_kickPulse = kickAvg;      // Instant attack
} else {
    m_kickPulse *= 0.80f;       // ~80ms decay
}

// Use for spawn triggering
if (m_kickPulse > 0.5f && m_spawnCooldown == 0) {
    spawnBassRipple();
}
```

---

## 10. Complete Effect Template

A full template incorporating all proven patterns.

```cpp
/**
 * @file MyAudioReactiveEffect.cpp
 * @brief Template for audio-reactive CENTER ORIGIN effect
 */

#include "MyAudioReactiveEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

MyAudioReactiveEffect::MyAudioReactiveEffect()
    : m_phase(0.0f)
{
}

bool MyAudioReactiveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset phase
    m_phase = 0.0f;
    m_lastHopSeq = 0;

    // Initialize energy history (4-hop rolling average)
    for (uint8_t i = 0; i < ENERGY_HISTORY; ++i) {
        m_energyHist[i] = 0.0f;
    }
    m_energySum = 0.0f;
    m_energyHistIdx = 0;
    m_energyAvg = 0.0f;
    m_energyDelta = 0.0f;

    // Initialize smoothing engines
    m_speedSpring.init(50.0f, 1.0f);  // stiffness=50, critically damped
    m_speedSpring.reset(1.0f);
    m_energyFollower.reset(0.0f);

    // Initialize visual state
    m_collisionFlash = 0.0f;
    m_dominantBin = 0;
    m_dominantBinSmooth = 0.0f;

    return true;
}

void MyAudioReactiveEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // SAFE DELTA TIME
    // =========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();

    // =========================================================================
    // AUDIO PROCESSING (PER-HOP ONLY)
    // =========================================================================
    float speedMult = 1.0f;
    float brightnessGain = 1.0f;
    uint8_t chromaOffset = 0;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // -----------------------------------------------------------------
        // SPEED: heavy_bands → Spring (NO extra smoothing!)
        // -----------------------------------------------------------------
        float heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                             ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
        float targetSpeed = 0.6f + 0.8f * heavyEnergy;
        speedMult = m_speedSpring.update(targetSpeed, dt);
        if (speedMult > 1.6f) speedMult = 1.6f;
        if (speedMult < 0.3f) speedMult = 0.3f;

        // -----------------------------------------------------------------
        // PER-HOP PROCESSING (every ~16ms)
        // -----------------------------------------------------------------
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            float currentEnergy = ctx.audio.heavyBass();

            // Rolling 4-hop average
            m_energySum -= m_energyHist[m_energyHistIdx];
            m_energyHist[m_energyHistIdx] = currentEnergy;
            m_energySum += currentEnergy;
            m_energyHistIdx = (m_energyHistIdx + 1) % ENERGY_HISTORY;
            m_energyAvg = m_energySum / ENERGY_HISTORY;

            // Energy delta (positive only)
            m_energyDelta = currentEnergy - m_energyAvg;
            if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;

            // Dominant chroma bin
            float maxChroma = 0.0f;
            for (uint8_t bin = 0; bin < 12; ++bin) {
                if (ctx.audio.controlBus.chroma[bin] > maxChroma) {
                    maxChroma = ctx.audio.controlBus.chroma[bin];
                    m_dominantBin = bin;
                }
            }
        }

        // -----------------------------------------------------------------
        // BRIGHTNESS: AsymmetricFollower for smooth envelope
        // -----------------------------------------------------------------
        float smoothedAvg = m_energyFollower.updateWithMood(m_energyAvg, dt, moodNorm);
        brightnessGain = 0.4f + 0.6f * smoothedAvg + 0.3f * m_energyDelta;
        if (brightnessGain > 1.5f) brightnessGain = 1.5f;
        if (brightnessGain < 0.3f) brightnessGain = 0.3f;

        // -----------------------------------------------------------------
        // COLLISION FLASH (snare-triggered)
        // -----------------------------------------------------------------
        if (ctx.audio.isSnareHit()) {
            m_collisionFlash = 1.0f;
        }
        m_collisionFlash *= 0.88f;

        // -----------------------------------------------------------------
        // CHROMA COLOR OFFSET (250ms smooth)
        // -----------------------------------------------------------------
        float alphaBin = 1.0f - expf(-dt / 0.25f);
        m_dominantBinSmooth += ((float)m_dominantBin - m_dominantBinSmooth) * alphaBin;
        chromaOffset = (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f));
    }
#endif

    // =========================================================================
    // PHASE ADVANCEMENT
    // =========================================================================
    float speedNorm = ctx.speed / 50.0f;
    m_phase += speedNorm * 240.0f * speedMult * dt;
    if (m_phase > 628.3f) m_phase -= 628.3f;

    // =========================================================================
    // RENDER LOOP (CENTER ORIGIN)
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);

        // Your pattern logic here
        float wave = sinf(dist * 0.25f - m_phase);
        wave = wave * 0.5f + 0.5f;

        // Apply brightness modulation
        uint8_t brightness = (uint8_t)(wave * ctx.brightness * brightnessGain);

        // Apply collision flash
#if FEATURE_AUDIO_SYNC
        if (ctx.audio.available && m_collisionFlash > 0.01f) {
            float flash = m_collisionFlash * expf(-(float)dist * 0.12f);
            brightness = qadd8(brightness, (uint8_t)(flash * 60.0f));
        }
#endif

        // Color with chroma offset
        uint8_t hue = ctx.gHue + chromaOffset + (dist >> 2);

        // Render to both strips
        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(hue + 128), brightness);
        }
    }
}

void MyAudioReactiveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& MyAudioReactiveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "My Audio Reactive Effect",
        "Template with proven audio-reactive patterns",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
```

---

## Related Documentation

- **[AUDIO_OUTPUT_SPECIFICATIONS.md](AUDIO_OUTPUT_SPECIFICATIONS.md)** - Audio data structures and pipeline
- **[VISUAL_PIPELINE_MECHANICS.md](VISUAL_PIPELINE_MECHANICS.md)** - Render architecture
- **[COLOR_PALETTE_SYSTEM.md](COLOR_PALETTE_SYSTEM.md)** - Palette access and correction
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues and fixes
