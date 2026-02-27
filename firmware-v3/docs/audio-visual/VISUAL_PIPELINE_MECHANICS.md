# Visual Pipeline Mechanics

**LightwaveOS v2 Audio-Visual Processing Guide - Part 2**

This document covers the visual rendering pipeline, pattern algorithms, propagation mechanics, and frame-rate independent motion systems.

---

## Table of Contents

1. [Render Pipeline Overview](#1-render-pipeline-overview)
2. [Pattern Algorithm Taxonomy](#2-pattern-algorithm-taxonomy)
3. [CENTER ORIGIN Propagation Model](#3-center-origin-propagation-model)
4. [Phase Accumulation Formula](#4-phase-accumulation-formula)
5. [Buffer Architecture](#5-buffer-architecture)
6. [Smoothing Engines](#6-smoothing-engines)
7. [Frame Timing & Delta Time](#7-frame-timing--delta-time)
8. [Stateful Effects & History](#8-stateful-effects--history)
9. [Zone Compositor](#9-zone-compositor)

---

## 1. Render Pipeline Overview

### 120 FPS Render Cycle

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      RENDERER ACTOR (Core 1, Priority 5)                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Target: 120 FPS (8,333 µs per frame)                                        │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ 1. onTick()                                                             ││
│  │    ├── applyPendingEffectParameterUpdates()                             ││
│  │    ├── applyPendingAudioContractTuning()                                ││
│  │    ├── Read latest ControlBusFrame (audio snapshot)                     ││
│  │    └── Advance MusicalGrid (beat phase interpolation)                   ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                               ↓                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ 2. renderFrame()                                                        ││
│  │    ├── Build EffectContext (params, audio, leds, palette)               ││
│  │    ├── Call effect->render(ctx)                                         ││
│  │    │       └── Effect modifies ctx.leds[0-319]                          ││
│  │    └── Effect returns                                                   ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                               ↓                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ 3. processBuffer()                                                      ││
│  │    ├── Check: PatternRegistry::shouldSkipColorCorrection(effectId)?     ││
│  │    │   ├── YES: Skip ColorCorrectionEngine (LGP/stateful/physics)       ││
│  │    │   └── NO:  Apply auto-exposure, gamma, white guardrail             ││
│  │    └── Optional: captureFrame() for TAP A/B/C analysis                  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                               ↓                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ 4. showLeds()                                                           ││
│  │    ├── Copy m_leds[0-159] → m_strip1[0-159]                             ││
│  │    ├── Copy m_leds[160-319] → m_strip2[0-159]                           ││
│  │    └── FastLED.show() → RMT driver → WS2812 @ 800 kHz                   ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  LED data transmission: ~9.6 ms for 320 LEDs (30 bits × 320 × 1.25 µs)      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Source Files

| File | Purpose |
|------|---------|
| `v2/src/core/actors/RendererActor.h` | Render loop, frame timing |
| `v2/src/core/actors/RendererActor.cpp` | Effect invocation, buffer management |
| `v2/src/effects/PatternRegistry.h` | Pattern taxonomy, color correction bypass |
| `v2/src/effects/CoreEffects.h` | CENTER ORIGIN helpers, constants |
| `v2/src/effects/enhancement/SmoothingEngine.h` | Motion smoothing primitives |
| `v2/src/effects/enhancement/ColorCorrectionEngine.h` | Post-processing |

---

## 2. Pattern Algorithm Taxonomy

### 10 Pattern Families

```cpp
// Source: v2/src/effects/PatternRegistry.h (lines 28-40)

enum class PatternFamily : uint8_t {
    INTERFERENCE = 0,      // 13 patterns: Standing waves, moiré, phase collision
    GEOMETRIC = 1,         // 8 patterns: Tiled/radial, fractal, symmetry
    ADVANCED_OPTICAL = 2,  // 6 patterns: Chromatic, diffraction, caustics
    ORGANIC = 3,           // 12 patterns: Bio-inspired (mycelium, aurora)
    QUANTUM = 4,           // 11 patterns: Tunneling, entanglement, solitons
    COLOR_MIXING = 5,      // 12 patterns: Color theory, temperature gradients
    PHYSICS_BASED = 6,     // 6 patterns: Plasma flow, magnetic fields
    NOVEL_PHYSICS = 7,     // 5 patterns: Chladni, gravitational waves
    FLUID_PLASMA = 8,      // 5 patterns: Fire, ocean, convection, vortex
    MATHEMATICAL = 9,      // 5 patterns: Mandelbrot, strange attractors
    UNKNOWN = 255
};
```

### Pattern Tags (Bitfield)

```cpp
// Source: v2/src/effects/PatternRegistry.h (lines 72-81)

namespace PatternTags {
    constexpr uint8_t STANDING = 0x01;        // Standing wave pattern
    constexpr uint8_t TRAVELING = 0x02;       // Traveling wave pattern
    constexpr uint8_t MOIRE = 0x04;           // Moiré interference
    constexpr uint8_t DEPTH = 0x08;           // Depth illusion effect
    constexpr uint8_t SPECTRAL = 0x10;        // Chromatic/dispersive
    constexpr uint8_t CENTER_ORIGIN = 0x20;   // Originates from LEDs 79/80
    constexpr uint8_t DUAL_STRIP = 0x40;      // Uses dual-strip interference
    constexpr uint8_t PHYSICS = 0x80;         // Physics simulation based
}
```

### Family Characteristics

| Family | Count | Key Characteristics | Example Effects |
|--------|-------|---------------------|-----------------|
| **INTERFERENCE** | 13 | Wave superposition, standing waves, moiré | BoxWave, Holographic, ModalResonance |
| **GEOMETRIC** | 8 | Symmetry, tiling, radial patterns | DiamondLattice, ConcentricRings, StarBurst |
| **ADVANCED_OPTICAL** | 6 | Light physics, refraction, diffraction | ChromaticLens, FresnelZones, PhotonicCrystal |
| **ORGANIC** | 12 | Natural patterns, growth, flow | Aurora, PlasmaWaves, NeuralNetwork, Mycelium |
| **QUANTUM** | 11 | Non-classical physics, probabilistic | SolitonWaves, Entanglement, Tunneling |
| **COLOR_MIXING** | 12 | Color theory, temperature, spectrum | RGBPrism, ComplementaryMix, DopplerShift |
| **PHYSICS_BASED** | 6 | Classical physics simulation | PlasmaFlow, MagneticField, Electrostatic |
| **NOVEL_PHYSICS** | 5 | Exotic physics effects | ChladniHarmonics, GravitationalWaveChirp |
| **FLUID_PLASMA** | 5 | Fluid dynamics, combustion | Fire, Ocean, Plasma, Convection |
| **MATHEMATICAL** | 5 | Pure math patterns | Mandelbrot, StrangeAttractor, Kuramoto |

---

## 3. CENTER ORIGIN Propagation Model

### MANDATORY: All Effects Originate from Center

The Light Guide Plate (LGP) hardware creates optical interference patterns. Edge-originating effects look wrong on this hardware. **ALL effects MUST originate from LEDs 79/80 (center point)**.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        STRIP LAYOUT (160 LEDs)                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Edge ◄──────────────────── CENTER ────────────────────► Edge               │
│                                                                              │
│  LED:  0    10   20   30   40   50   60   70  [79|80]  90   100  110  120  159│
│        ├────┼────┼────┼────┼────┼────┼────┼─────┼─────┼────┼────┼────┼────┤  │
│                                       ▲    ▲                                 │
│                                       │    │                                 │
│                               CENTER_LEFT  CENTER_RIGHT                      │
│                                  (79)         (80)                           │
│                                                                              │
│  Distance from center:                                                       │
│    LED 79: dist = 0    (center left)                                         │
│    LED 80: dist = 0    (center right)                                        │
│    LED 0:  dist = 79   (left edge)                                           │
│    LED 159: dist = 79  (right edge)                                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Core Helper Functions

```cpp
// Source: v2/src/effects/CoreEffects.h (lines 20-50)

// === Constants ===
constexpr uint16_t CENTER_LEFT = 79;     // Last LED of left half
constexpr uint16_t CENTER_RIGHT = 80;    // First LED of right half
constexpr uint16_t HALF_LENGTH = 80;     // LEDs per half (0-79 left, 80-159 right)
constexpr uint16_t STRIP_LENGTH = 160;   // LEDs per strip
constexpr uint16_t TOTAL_LEDS = 320;     // Both strips combined

// === Distance from Center (unsigned, 0 at center) ===
// Returns 0 for LEDs 79 and 80, increases toward edges
constexpr uint16_t centerPairDistance(uint16_t index) {
    return (index <= CENTER_LEFT)
           ? (CENTER_LEFT - index)      // Left half: 79-index
           : (index - CENTER_RIGHT);    // Right half: index-80
}

// Examples:
// centerPairDistance(79) = 0   (center left)
// centerPairDistance(80) = 0   (center right)
// centerPairDistance(0)  = 79  (left edge)
// centerPairDistance(159)= 79  (right edge)
// centerPairDistance(40) = 39  (mid-left)

// === Signed Position (for physics, -79.5 to +79.5) ===
// Negative = left of center, positive = right of center
constexpr float centerPairSignedPosition(uint16_t index) {
    return (index <= CENTER_LEFT)
           ? -((float)(CENTER_LEFT - index) + 0.5f)  // Left: -79.5 to -0.5
           : ((float)(index - CENTER_RIGHT) + 0.5f); // Right: +0.5 to +79.5
}

// Examples:
// centerPairSignedPosition(79) = -0.5   (just left of center)
// centerPairSignedPosition(80) = +0.5   (just right of center)
// centerPairSignedPosition(0)  = -79.5  (left edge)
// centerPairSignedPosition(159)= +79.5  (right edge)
```

### Propagation Formulas

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        WAVE PROPAGATION PATTERNS                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  OUTWARD PROPAGATION (waves expand from center to edges):                    │
│  ─────────────────────────────────────────────────────                       │
│                                                                              │
│    brightness = sin8((distFromCenter << 2) - (phaseInt >> 7));              │
│                      ↑ spatial frequency    ↑ temporal phase                 │
│                                                                              │
│    As phase increases, the subtraction causes pattern to move               │
│    toward HIGHER distance values (outward toward edges).                     │
│                                                                              │
│    ◄────────────── ● ──────────────►                                        │
│         ←←←←←      ▲      →→→→→                                             │
│                 CENTER                                                       │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  INWARD PROPAGATION (waves collapse from edges to center):                   │
│  ─────────────────────────────────────────────────────                       │
│                                                                              │
│    brightness = sin8((distFromCenter << 2) + (phaseInt >> 7));              │
│                                              ↑ addition, not subtraction     │
│                                                                              │
│    As phase increases, the addition causes pattern to move                  │
│    toward LOWER distance values (inward toward center).                      │
│                                                                              │
│         →→→→→      ▲      ←←←←←                                             │
│    ──────────────► ● ◄──────────────                                        │
│                 CENTER                                                       │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  STANDING WAVE (stationary pattern, no motion):                              │
│  ─────────────────────────────────────────────                               │
│                                                                              │
│    brightness = sin8(distFromCenter << 2);  // No phase term                 │
│                                                                              │
│    Pattern does not move. Brightness modulation occurs via                   │
│    separate audio-reactive multiplier on amplitude.                          │
│                                                                              │
│         ●───●───●  ▲  ●───●───●                                             │
│         ∿   ∿   ∿  │  ∿   ∿   ∿                                             │
│                 CENTER                                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Complete Render Loop Example

```cpp
// Source: v2/src/effects/ieffect/LGPPhotonicCrystalEffect.cpp (lines 180-250)

void LGPPhotonicCrystalEffect::render(plugins::EffectContext& ctx) {
    // === Phase accumulation (see Section 4) ===
    float speedNorm = ctx.speed / 50.0f;
    m_phase += speedNorm * 240.0f * speedMult * dt;
    if (m_phase > 628.3f) m_phase -= 628.3f;
    uint16_t phaseInt = (uint16_t)(m_phase * 0.408f);

    // === Render loop: iterate over all LEDs ===
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // 1. Compute distance from center (0 at 79/80, 79 at edges)
        uint16_t distFromCenter = centerPairDistance(i);

        // 2. Apply pattern algorithm using distance
        uint8_t cellPosition = distFromCenter % latticeSize;
        bool inBandgap = cellPosition < (latticeSize >> 1);

        // 3. Compute brightness with OUTWARD wave motion
        uint8_t brightness = 0;
        if (inBandgap) {
            // sin8 argument: spatial * 4 - temporal phase
            // Subtraction causes OUTWARD motion as phase increases
            brightness = sin8((distFromCenter << 2) - (phaseInt >> 7));
        } else {
            // Evanescent decay in forbidden zones
            uint8_t decay = 255 - (cellPosition * 50);
            brightness = scale8(sin8((distFromCenter << 1) - (phaseInt >> 8)), decay);
        }

        // 4. Apply audio modulation to brightness
        brightness = scale8(brightness, (uint8_t)(ctx.brightness * brightnessGain));

        // 5. Compute color from palette
        uint8_t palettePos = baseHue + distFromCenter / 4;

        // 6. Write to BOTH strips (Strip 2 gets complementary offset)
        ctx.leds[i] = ctx.palette.getColor(palettePos, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(palettePos + 64),  // +64 = 90° hue shift
                brightness
            );
        }
    }
}
```

---

## 4. Phase Accumulation Formula

### The Standard Formula (CRITICAL)

All working audio-reactive effects use this exact formula:

```cpp
// Source: Multiple effects (ChevronWaves, InterferenceScanner, WaveCollision, PhotonicCrystal)

// === Standard phase accumulation ===
float speedNorm = ctx.speed / 50.0f;          // Normalize UI speed (1-100) to 0-2 range
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;

// === Wrap to prevent float overflow ===
if (m_phase > 628.3f) m_phase -= 628.3f;      // 628.3 = 100 × 2π

// === Convert to integer for sin8() ===
uint16_t phaseInt = (uint16_t)(m_phase * 0.408f);  // Scale to 0-256 range
```

### Why These Constants?

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      PHASE CONSTANT DERIVATION                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  240.0f - Phase velocity multiplier                                          │
│  ──────────────────────────────────                                          │
│                                                                              │
│    At speedNorm=1.0, smoothedSpeed=1.0, dt=1/120 (8.33ms):                   │
│      phase_delta = 1.0 × 240.0 × 1.0 × 0.00833 = 2.0 radians/frame          │
│                                                                              │
│    Full cycle (2π radians) takes:                                            │
│      6.28 / 2.0 = 3.14 frames = ~26ms                                        │
│      = ~38 Hz visual oscillation at 120 FPS                                  │
│                                                                              │
│    This provides comfortable visual speed at mid slider position.            │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  628.3f - Phase wrap point (100 × 2π)                                        │
│  ─────────────────────────────────────                                       │
│                                                                              │
│    Using 2π directly (6.28f) would cause frequent wrapping, losing           │
│    precision in float operations.                                            │
│                                                                              │
│    100 × 2π = 628.3f allows 100 full cycles before wrap, preserving          │
│    float precision much longer (important for long-running effects).         │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  0.408f - Phase to sin8 conversion factor                                    │
│  ─────────────────────────────────────────                                   │
│                                                                              │
│    sin8() expects input 0-255 (wraps internally)                             │
│    628.3 × 0.408 ≈ 256                                                       │
│    So m_phase 0..628.3 maps to phaseInt 0..256                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Speed Modulation (Audio-Reactive)

```cpp
// Source: v2/src/effects/ieffect/ChevronWavesEffect.cpp (lines 111-119)

// CORRECT: Use heavy_bands directly into Spring (no extra smoothing!)
float heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                     ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
float targetSpeed = 0.6f + 0.8f * heavyEnergy;  // Range: 0.6x to 1.4x
float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, dt);

// Apply to phase
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
```

**Speed Range Guidelines:**
- Conservative: 0.6x to 1.4x (most effects)
- Moderate: 0.5x to 1.5x (high-energy effects)
- Aggressive: 0.3x to 1.6x (dramatic effects, with clamping)

---

## 5. Buffer Architecture

### Memory Layout

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         LED BUFFER ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  m_leds[320] - Unified Effect Buffer                                         │
│  ┌─────────────────────────────────┬─────────────────────────────────────┐  │
│  │       Strip 1 (0-159)           │       Strip 2 (160-319)             │  │
│  │  [0] [1] ... [79|80] ... [159]  │  [160] [161] ... [239|240] ... [319]│  │
│  └─────────────────────────────────┴─────────────────────────────────────┘  │
│                                                                              │
│  Effects write directly to m_leds[0-319]. The buffer proxy pattern allows    │
│  effects to use full 320-LED indexing regardless of zone system.             │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  m_strip1[160], m_strip2[160] - Hardware Output Buffers                      │
│  ┌─────────────────────────────────┐  ┌─────────────────────────────────┐  │
│  │  Physical Strip 1 (GPIO 4)      │  │  Physical Strip 2 (GPIO 5)      │  │
│  │  [0] [1] ... [159]              │  │  [0] [1] ... [159]              │  │
│  └─────────────────────────────────┘  └─────────────────────────────────┘  │
│                                                                              │
│  showLeds() copies:                                                          │
│    m_leds[0-159]   → m_strip1[0-159]                                         │
│    m_leds[160-319] → m_strip2[0-159]                                         │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  Zone Compositor Buffers                                                     │
│  ┌─────────────────────────────────┐  ┌─────────────────────────────────┐  │
│  │  m_tempBuffer[320]              │  │  m_outputBuffer[320]            │  │
│  │  Full effect render temp        │  │  Composited zone result         │  │
│  └─────────────────────────────────┘  └─────────────────────────────────┘  │
│                                                                              │
│  Each zone effect renders to m_tempBuffer, then its segment is extracted     │
│  and composited to m_outputBuffer with the zone's blend mode.                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Buffer Sizes

| Buffer | Size | Memory | Purpose |
|--------|------|--------|---------|
| `m_leds[]` | 320 × CRGB | 960 bytes | Unified effect buffer |
| `m_strip1[]` | 160 × CRGB | 480 bytes | Hardware Strip 1 output |
| `m_strip2[]` | 160 × CRGB | 480 bytes | Hardware Strip 2 output |
| `m_tempBuffer[]` | 320 × CRGB | 960 bytes | Zone compositor temp |
| `m_outputBuffer[]` | 320 × CRGB | 960 bytes | Zone compositor output |
| `m_radial[]` | 80 × CRGB | 240 bytes | Stateful effect history |

**Total LED buffers:** ~4 KB

---

## 6. Smoothing Engines

### SmoothingEngine.h Primitives

```cpp
// Source: v2/src/effects/enhancement/SmoothingEngine.h

namespace lightwaveos {
namespace effects {
namespace enhancement {

// ═══════════════════════════════════════════════════════════════════════════
// ExpDecay - True Frame-Rate Independent Exponential Smoothing
// ═══════════════════════════════════════════════════════════════════════════

struct ExpDecay {
    float value = 0.0f;
    float lambda = 5.0f;  // Convergence rate (higher = faster)

    float update(float target, float dt) {
        // TRUE frame-rate independent formula
        // DO NOT use the approximation: dt / (tau + dt)
        float alpha = 1.0f - expf(-lambda * dt);
        value += (target - value) * alpha;
        return value;
    }

    void reset(float v) { value = v; }

    // Factory: create with time constant in seconds
    static ExpDecay withTimeConstant(float tauSeconds) {
        return ExpDecay{0.0f, 1.0f / tauSeconds};
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Spring - Critically Damped Second-Order System
// ═══════════════════════════════════════════════════════════════════════════

struct Spring {
    float position = 0.0f;
    float velocity = 0.0f;
    float stiffness = 100.0f;  // k: spring constant
    float damping = 20.0f;     // c: damping coefficient
    float mass = 1.0f;         // m: mass

    void init(float stiffnessVal = 100.0f, float massVal = 1.0f) {
        stiffness = stiffnessVal;
        mass = massVal;
        // Critical damping: c = 2√(km) → fastest approach without overshoot
        damping = 2.0f * sqrtf(stiffness * mass);
        position = 0.0f;
        velocity = 0.0f;
    }

    float update(float target, float dt) {
        // F = -k*x - c*v (spring + damper)
        float displacement = position - target;
        float acceleration = (-stiffness * displacement - damping * velocity) / mass;

        // Euler integration
        velocity += acceleration * dt;
        position += velocity * dt;

        return position;
    }

    void reset(float v) {
        position = v;
        velocity = 0.0f;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// AsymmetricFollower - Sensory Bridge Pattern (Fast Attack, Slow Release)
// ═══════════════════════════════════════════════════════════════════════════

struct AsymmetricFollower {
    float value = 0.0f;
    float riseTau = 0.05f;   // Rise time constant (seconds)
    float fallTau = 0.30f;   // Fall time constant (seconds)

    AsymmetricFollower() = default;
    AsymmetricFollower(float initial, float rise, float fall)
        : value(initial), riseTau(rise), fallTau(fall) {}

    float update(float target, float dt) {
        float tau = (target > value) ? riseTau : fallTau;
        float alpha = 1.0f - expf(-dt / tau);
        value += (target - value) * alpha;
        return value;
    }

    // Mood-adaptive variant (Sensory Bridge knob pattern)
    // mood=0: reactive (fast), mood=1: smooth (slow)
    float updateWithMood(float target, float dt, float moodNorm) {
        float adjRiseTau = riseTau * (1.0f + moodNorm);       // 1x to 2x
        float adjFallTau = fallTau * (0.5f + 0.5f * moodNorm); // 0.5x to 1x

        float tau = (target > value) ? adjRiseTau : adjFallTau;
        float alpha = 1.0f - expf(-dt / tau);
        value += (target - value) * alpha;
        return value;
    }

    void reset(float v) { value = v; }
};

} // namespace enhancement
} // namespace effects
} // namespace lightwaveos
```

### When to Use Each

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      SMOOTHING ENGINE SELECTION GUIDE                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ExpDecay                                                                    │
│  ────────                                                                    │
│  Use for: General smoothing where symmetric rise/fall is acceptable         │
│  Examples: Color transitions, slow parameter changes, ambient motion         │
│  Parameters: lambda (higher = faster) or tau (time constant in seconds)     │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  Spring                                                                      │
│  ──────                                                                      │
│  Use for: Speed modulation, natural momentum, avoiding lurching             │
│  Examples: ChevronWaves speed, InterferenceScanner speed, PhotonicCrystal   │
│  Parameters:                                                                 │
│    stiffness = 50.0f (proven value across effects)                          │
│    mass = 1.0f (standard)                                                   │
│    damping = auto-calculated for critical damping (2√km)                    │
│                                                                              │
│  Time constant ≈ √(m/k) = √(1/50) ≈ 0.14 seconds                            │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  AsymmetricFollower                                                          │
│  ──────────────────                                                          │
│  Use for: Audio-reactive brightness, energy tracking, visual intensity      │
│  Examples: Brightness modulation (not speed!), collision flash decay         │
│  Parameters:                                                                 │
│    riseTau = 0.05-0.08s (fast attack)                                       │
│    fallTau = 0.20-0.30s (slow release)                                      │
│                                                                              │
│  WARNING: Do NOT use for speed modulation! Causes jitter when combined      │
│           with Spring (too many smoothing layers).                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Critical Damping Derivation

```cpp
// For a spring-damper system: m*a + c*v + k*x = 0
// Characteristic equation: m*s² + c*s + k = 0
// Roots: s = (-c ± √(c² - 4mk)) / 2m

// Critical damping occurs when discriminant = 0:
// c² - 4mk = 0
// c = 2√(mk)

// For stiffness=50, mass=1:
// damping = 2 * sqrt(50 * 1) = 2 * 7.07 = 14.14

// Time constant (settling time):
// τ = 2m/c = 2*1/14.14 = 0.14 seconds = 140ms
```

---

## 7. Frame Timing & Delta Time

### Safe Delta Time Calculation

```cpp
// Source: v2/src/effects/CoreEffects.h (lines 60-70)

inline float getSafeDeltaSeconds(float deltaTimeMs) {
    float dt = deltaTimeMs * 0.001f;  // Convert ms to seconds

    // Clamp to safe range
    if (dt < 0.001f) dt = 0.001f;   // Minimum 1ms (prevent division issues)
    if (dt > 0.05f) dt = 0.05f;     // Maximum 50ms (20 FPS floor)

    return dt;
}

// Usage in effects:
void render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();
    // OR: float dt = getSafeDeltaSeconds(ctx.deltaTimeMs);

    m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
    // ...
}
```

### Render Timing Constants

```cpp
// Source: v2/src/core/actors/RendererActor.h (lines 30-40)

struct LedConfig {
    static constexpr uint16_t TARGET_FPS = 120;
    static constexpr uint32_t FRAME_TIME_US = 1000000 / TARGET_FPS;  // 8333µs
    static constexpr uint8_t DEFAULT_BRIGHTNESS = 96;
    static constexpr uint8_t MAX_BRIGHTNESS = 160;  // Power budget limit
    static constexpr uint8_t DEFAULT_SPEED = 10;
    static constexpr uint8_t MAX_SPEED = 100;
};
```

### Frame Timing Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         FRAME TIMING @ 120 FPS                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Frame N                     Frame N+1                    Frame N+2          │
│  ├──────────────────────────┼──────────────────────────┼─────────────       │
│  │         8.33 ms          │         8.33 ms          │                    │
│  │                          │                          │                    │
│  │  ┌─────┐ ┌─────┐ ┌─────┐│  ┌─────┐ ┌─────┐ ┌─────┐│                    │
│  │  │Tick │ │Rndr │ │Show ││  │Tick │ │Rndr │ │Show ││                    │
│  │  │1ms  │ │2ms  │ │5ms  ││  │1ms  │ │2ms  │ │5ms  ││                    │
│  │  └─────┘ └─────┘ └─────┘│  └─────┘ └─────┘ └─────┘│                    │
│  │                          │                          │                    │
│  ▼                          ▼                          ▼                    │
│  t=0ms                      t=8.33ms                   t=16.67ms            │
│                                                                              │
│  Tick:   ~1ms  (audio sync, parameter updates)                               │
│  Render: ~2ms  (effect calculation, ~200 LEDs/ms)                            │
│  Show:   ~5ms  (FastLED.show(), RMT transmission)                            │
│  Margin: ~0.3ms (timing adjustment)                                          │
│                                                                              │
│  LED transmission: 320 LEDs × 30 bits × 1.25µs/bit = 12ms theoretical        │
│                    Actual: ~5ms due to RMT buffering                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Stateful Effects & History

### Effects That Read Previous Frame

Some effects require reading the previous frame's buffer for propagation or decay. These effects **MUST skip ColorCorrectionEngine** because it mutates the buffer in-place.

```cpp
// Source: v2/src/effects/PatternRegistry.cpp (lines 180-200)

bool isStatefulEffect(uint8_t effectId) {
    // These effects read ctx.leds from previous frame
    switch (effectId) {
        case 3:  // Confetti - accumulates random sparkles
        case 8:  // Ripple - radial propagation
            return true;
        default:
            return false;
    }
}
```

### RippleEffect History Pattern

```cpp
// Source: v2/src/effects/ieffect/RippleEffect.cpp

class RippleEffect : public plugins::IEffect {
private:
    // === Radial History Buffer ===
    CRGB m_radial[HALF_LENGTH];      // 80 LEDs (left half representation)
    CRGB m_radialAux[HALF_LENGTH];   // Auxiliary buffer for transformations

    // === Active Ripples ===
    struct Ripple {
        float radius;         // Current position (0-80)
        float speed;          // Expansion velocity
        uint8_t hue;         // Color
        uint8_t intensity;   // Brightness
        bool active;
    };
    static constexpr uint8_t MAX_RIPPLES = 5;
    Ripple m_ripples[MAX_RIPPLES];

    // === Decay Rate ===
    static constexpr uint8_t FADE_AMOUNT = 45;  // Aggressive decay per frame
};

void RippleEffect::render(plugins::EffectContext& ctx) {
    // 1. Fade existing radial buffer (prevents white blooming)
    for (uint8_t i = 0; i < HALF_LENGTH; ++i) {
        m_radial[i].fadeToBlackBy(FADE_AMOUNT);
    }

    // 2. Update active ripples
    for (uint8_t r = 0; r < MAX_RIPPLES; ++r) {
        if (m_ripples[r].active) {
            // Expand radius
            m_ripples[r].radius += m_ripples[r].speed * dt;

            // Write to radial buffer at current position
            uint8_t pos = (uint8_t)m_ripples[r].radius;
            if (pos < HALF_LENGTH) {
                CRGB color = ColorFromPalette(*ctx.palette, m_ripples[r].hue);
                m_radial[pos] = blend(m_radial[pos], color, m_ripples[r].intensity);
            } else {
                m_ripples[r].active = false;  // Reached edge
            }
        }
    }

    // 3. Mirror radial buffer to full strip
    for (uint16_t i = 0; i < STRIP_LENGTH; ++i) {
        uint16_t dist = centerPairDistance(i);
        ctx.leds[i] = m_radial[dist];
    }
}
```

### Energy Delta Detection (Spawn Trigger)

```cpp
// Source: v2/src/effects/ieffect/RippleEffect.cpp (lines 100-130)

// 4-hop rolling average for energy baseline
static constexpr uint8_t ENERGY_HISTORY = 4;
float m_energyHist[ENERGY_HISTORY];
float m_energySum = 0.0f;
uint8_t m_energyHistIdx = 0;
float m_energyAvg = 0.0f;
float m_energyDelta = 0.0f;

void updateEnergyTracking(float currentEnergy) {
    // Rolling 4-hop average
    m_energySum -= m_energyHist[m_energyHistIdx];
    m_energyHist[m_energyHistIdx] = currentEnergy;
    m_energySum += currentEnergy;
    m_energyHistIdx = (m_energyHistIdx + 1) % ENERGY_HISTORY;
    m_energyAvg = m_energySum / ENERGY_HISTORY;

    // Delta = energy ABOVE average (onset detection)
    m_energyDelta = currentEnergy - m_energyAvg;
    if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;
}

// Spawn chance calculation
float chanceF = m_energyDelta * 510.0f + m_energyAvg * 80.0f;
if (chanceF > 255.0f) chanceF = 255.0f;
uint8_t spawnChance = (uint8_t)chanceF;

if (random8() < spawnChance) {
    spawnRipple(0, ctx.gHue);  // Spawn at center
}
```

---

## 9. Zone Compositor

### Zone System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ZONE COMPOSITOR FLOW                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Zone Layout (4 zones, symmetric around center):                             │
│                                                                              │
│  LED:  0────────39 40───────79 80──────119 120─────159                       │
│        ├─────────┤ ├─────────┤ ├─────────┤ ├─────────┤                       │
│        │ Zone 3  │ │ Zone 2  │ │ Zone 1  │ │ Zone 0  │                       │
│        │ Outer L │ │ Inner L │ │ Inner R │ │ Outer R │                       │
│        └─────────┘ └─────────┘ └─────────┘ └─────────┘                       │
│                           ▲▲                                                 │
│                       CENTER (79|80)                                         │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  Render Process:                                                             │
│                                                                              │
│  1. For each enabled zone:                                                   │
│     ┌─────────────────────────────────────────────────────────────────────┐ │
│     │ a. Set zone-specific parameters (effect, speed, brightness, palette)│ │
│     │ b. Render effect to m_tempBuffer[320] (full strip render)           │ │
│     │ c. Extract zone segment from m_tempBuffer                           │ │
│     │ d. Composite to m_outputBuffer using zone's blend mode              │ │
│     └─────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
│  2. Copy m_outputBuffer to main LED buffer                                   │
│                                                                              │
│  ───────────────────────────────────────────────────────────────────────    │
│                                                                              │
│  Blend Modes:                                                                │
│    REPLACE     - Zone overwrites underlying                                  │
│    ADD         - Additive blending (brighter)                                │
│    MULTIPLY    - Multiplicative (darker)                                     │
│    SCREEN      - Inverse multiply (lighter)                                  │
│    OVERLAY     - Contrast enhancement                                        │
│    SOFT_LIGHT  - Subtle overlay                                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### ZoneComposer Implementation

```cpp
// Source: v2/src/effects/zones/ZoneComposer.h (lines 30-80)

struct ZoneState {
    uint8_t effectId;           // Effect to render (0-76)
    uint8_t brightness;         // Zone brightness (0-255)
    uint8_t speed;              // Zone speed (1-100)
    uint8_t paletteId;          // Palette ID (0 = use global)
    BlendMode blendMode;        // Compositing mode
    bool enabled;               // Zone enabled flag
};

class ZoneComposer {
public:
    void render(CRGB* leds, uint16_t numLeds, CRGBPalette16* palette,
                uint8_t hue, uint32_t frameCount) {
        // Clear output buffer
        memset(m_outputBuffer, 0, sizeof(m_outputBuffer));

        for (uint8_t z = 0; z < m_zoneCount; ++z) {
            if (!m_zones[z].enabled) continue;

            // 1. Render zone's effect to temp buffer
            renderZone(z, m_tempBuffer, numLeds, palette, hue, frameCount);

            // 2. Extract zone segment
            extractZoneSegment(z, m_tempBuffer, m_zoneBuffer);

            // 3. Composite with blend mode
            compositeZone(z, m_zoneBuffer);
        }

        // 4. Copy final result to output
        memcpy(leds, m_outputBuffer, numLeds * sizeof(CRGB));
    }

private:
    ZoneState m_zones[MAX_ZONES];
    CRGB m_tempBuffer[TOTAL_LEDS];
    CRGB m_outputBuffer[TOTAL_LEDS];
    CRGB m_zoneBuffer[HALF_LENGTH];  // Single zone segment
};
```

---

## See Also

- [AUDIO_OUTPUT_SPECIFICATIONS.md](AUDIO_OUTPUT_SPECIFICATIONS.md) - Audio data available to effects
- [COLOR_PALETTE_SYSTEM.md](COLOR_PALETTE_SYSTEM.md) - Palette library and color operations
- [IMPLEMENTATION_PATTERNS.md](IMPLEMENTATION_PATTERNS.md) - Complete code examples
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Motion and timing issues
