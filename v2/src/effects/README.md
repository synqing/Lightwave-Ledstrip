# Effects Subsystem

**LightwaveOS v2 Visual Effects Engine**

This directory contains the complete visual effects system for the Light Guide Plate (LGP) LED display. The system provides 90+ visual effects, multi-zone composition, 12 transition types, and sophisticated color/motion enhancement engines.

---

## Architecture Overview

```
                           +---------------------------+
                           |     RendererActor         |
                           |   (Core 1, 120 FPS)       |
                           +-------------+-------------+
                                         |
                   +---------------------+---------------------+
                   |                     |                     |
                   v                     v                     v
        +------------------+   +------------------+   +------------------+
        | Single Effect    |   | ZoneComposer     |   | TransitionEngine |
        | Rendering        |   | (1-4 zones)      |   | (12 types)       |
        +--------+---------+   +--------+---------+   +--------+---------+
                 |                      |                      |
                 v                      v                      v
        +------------------+   +------------------+   +------------------+
        | IEffect.render() |   | Per-zone buffers |   | Source/Target    |
        | EffectContext    |   | Zone extraction  |   | interpolation    |
        +--------+---------+   +--------+---------+   +--------+---------+
                 |                      |                      |
                 +----------------------+----------------------+
                                        |
                                        v
                           +---------------------------+
                           | Enhancement Pipeline      |
                           | - ColorEngine             |
                           | - MotionEngine            |
                           | - SmoothingEngine         |
                           +-------------+-------------+
                                         |
                                         v
                           +---------------------------+
                           |    leds[320] buffer       |
                           |  syncLedsToStrips()       |
                           |    FastLED.show()         |
                           +---------------------------+
```

---

## Directory Structure

| Directory | Purpose | Key Files |
|-----------|---------|-----------|
| `ieffect/` | 90 IEffect implementations | `*Effect.h/.cpp` |
| `zones/` | Multi-zone orchestration | `ZoneComposer.h`, `ZoneDefinition.h`, `BlendMode.h` |
| `transitions/` | Effect transitions | `TransitionEngine.h`, `TransitionTypes.h`, `Easing.h` |
| `enhancement/` | Color/motion processing | `ColorEngine.h`, `MotionEngine.h`, `SmoothingEngine.h` |
| `utils/` | FastLED optimizations | `FastLEDOptim.h` |
| Root level | Category headers | `LGP*.h`, `CoreEffects.h`, `PatternRegistry.h` |

---

## CRITICAL: CENTER ORIGIN Constraint

**All effects MUST originate from LED 79/80 (center point).**

The hardware design uses a Light Guide Plate (LGP) where LEDs fire into an acrylic waveguide. This creates interference patterns that look correct ONLY when effects radiate from the center.

### Correct Patterns

```cpp
void render(plugins::EffectContext& ctx) override {
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        // USE THIS: Distance from center (0.0 at center, 1.0 at edges)
        float dist = ctx.getDistanceFromCenter(i);

        // Heat decreases from center outward
        uint8_t heat = 255 * (1.0f - dist);
        ctx.leds[i] = ctx.palette.getColor(heat);
    }
}
```

### Forbidden Patterns

```cpp
// WRONG: Linear left-to-right
for (uint16_t i = 0; i < ctx.ledCount; i++) {
    ctx.leds[i] = ColorFromPalette(pal, i);  // NO!
}

// WRONG: Rainbow color cycling
ctx.leds[i] = CHSV(ctx.gHue + i, 255, 255);  // NO! Rainbows are forbidden
```

### EffectContext Helper Methods

| Method | Returns | Use Case |
|--------|---------|----------|
| `getDistanceFromCenter(i)` | 0.0-1.0 | Position-based effects |
| `getSignedPosition(i)` | -1.0 to +1.0 | Directional effects |
| `mirrorIndex(i)` | uint16_t | Symmetric effects |
| `getPhase(hz)` | 0.0-1.0 | Time-based oscillation |
| `getSineWave(hz)` | -1.0 to +1.0 | Smooth oscillation |

---

## IEffect Interface

All effects implement the `lightwaveos::plugins::IEffect` interface:

```cpp
// Location: firmware/v2/src/plugins/api/IEffect.h

class IEffect {
public:
    virtual ~IEffect() = default;

    // Lifecycle
    virtual bool init(EffectContext& ctx) = 0;     // Called once on selection
    virtual void render(EffectContext& ctx) = 0;   // Called at 120 FPS
    virtual void cleanup() = 0;                    // Called on effect change

    // Metadata
    virtual const EffectMetadata& getMetadata() const = 0;

    // Optional: Custom parameters
    virtual uint8_t getParameterCount() const { return 0; }
    virtual const EffectParameter* getParameter(uint8_t index) const { return nullptr; }
    virtual bool setParameter(const char* name, float value) { return false; }
    virtual float getParameter(const char* name) const { return 0.0f; }
};
```

### EffectMetadata

```cpp
struct EffectMetadata {
    const char* name;           // Display name (max 32 chars)
    const char* description;    // Brief description (max 128 chars)
    EffectCategory category;    // FIRE, WATER, NATURE, GEOMETRIC, QUANTUM, etc.
    uint8_t version;            // Effect version
    const char* author;         // Creator name (optional)
};
```

### EffectContext

The `EffectContext` provides all dependencies for effect rendering:

```cpp
struct EffectContext {
    // LED Buffer
    CRGB* leds;              // Buffer to write to
    uint16_t ledCount;       // 320 for dual-strip
    uint16_t centerPoint;    // 80 (center LED)

    // Color
    PaletteRef palette;      // Current palette
    uint8_t gHue;            // Auto-incrementing hue

    // Parameters
    uint8_t brightness;      // 0-255
    uint8_t speed;           // 1-100
    uint8_t mood;            // 0-255 (reactive to smooth)
    uint8_t fadeAmount;      // Trail fade

    // Timing
    uint32_t deltaTimeMs;    // Frame delta
    uint32_t totalTimeMs;    // Total runtime
    uint32_t frameNumber;    // Frame counter

    // Audio (when FEATURE_AUDIO_SYNC enabled)
    AudioContext audio;      // Beat, RMS, spectrum data
};
```

---

## Effect Categories (PatternRegistry)

Effects are organized into 10 families with metadata for runtime discovery:

| Family | Description | Count |
|--------|-------------|-------|
| `INTERFERENCE` | Standing waves, moire, phase collision | 13 |
| `GEOMETRIC` | Tiled boxes, radial rings, fractal zoom | 8 |
| `ADVANCED_OPTICAL` | Chromatic lens, diffraction, caustics | 6 |
| `ORGANIC` | Bioluminescence, mycelium, plankton | 12 |
| `QUANTUM` | Wave function, soliton, entanglement | 11 |
| `COLOR_MIXING` | Warm/cold crystal, ember fade | 12 |
| `PHYSICS_BASED` | Plasma flow, magnetic field | 6 |
| `NOVEL_PHYSICS` | Rogue wave, Turing pattern | 5 |
| `FLUID_PLASMA` | Shockwave, convection, vortex | 5 |
| `MATHEMATICAL` | Mandelbrot, strange attractor | 5 |

### Pattern Tags

Effects can have multiple tags for filtering:

```cpp
namespace PatternTags {
    constexpr uint8_t STANDING = 0x01;       // Standing wave pattern
    constexpr uint8_t TRAVELING = 0x02;      // Traveling wave pattern
    constexpr uint8_t MOIRE = 0x04;          // Moire interference
    constexpr uint8_t DEPTH = 0x08;          // Depth illusion
    constexpr uint8_t SPECTRAL = 0x10;       // Chromatic effect
    constexpr uint8_t CENTER_ORIGIN = 0x20;  // CENTER ORIGIN compliant
    constexpr uint8_t DUAL_STRIP = 0x40;     // Uses dual-strip interference
    constexpr uint8_t PHYSICS = 0x80;        // Physics simulation
}
```

---

## Zone Composition System

The ZoneComposer enables **multi-zone rendering** where different effects run on different LED segments simultaneously.

### Zone Layout (CENTER ORIGIN)

All zones are concentric rings centered around LEDs 79/80:

**3-Zone Configuration (Default):**
```
Strip 1 (160 LEDs):
<--------------- CENTER (79/80) --------------->
+--------+-------------------+-----------------+--------+
| Zone 2 |      Zone 1       |     Zone 0      | Zone 1 |  Zone 2
| 0-19   |     20-64         |     65-94       | 95-139 | 140-159
| 20 LED |     45 LED        |     30 LED      | 45 LED |  20 LED
+--------+-------------------+-----------------+--------+

Zone 0 = 30 LEDs (innermost, center)
Zone 1 = 90 LEDs (middle ring)
Zone 2 = 40 LEDs (outermost, edges)
```

**4-Zone Configuration:**
```
Strip 1 (160 LEDs) - Equal 40 LEDs per zone:
+-------+--------+--------+---------+---------+--------+--------+-------+
| Z3    |   Z2   |   Z1   |   Z0    |   Z0    |   Z1   |   Z2   |  Z3   |
| 0-19  | 20-39  | 40-59  | 60-79   | 80-99   | 100-119| 120-139|140-159|
+-------+--------+--------+---------+---------+--------+--------+-------+
```

### ZoneComposer API

```cpp
class ZoneComposer {
public:
    bool init(RendererActor* renderer);
    void render(CRGB* leds, uint16_t numLeds, CRGBPalette16* palette,
                uint8_t hue, uint32_t frameCount, uint32_t deltaTimeMs,
                const AudioContext* audioCtx = nullptr);

    // Enable/disable zone mode
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Per-zone settings
    void setZoneEffect(uint8_t zone, uint8_t effectId);
    void setZoneBrightness(uint8_t zone, uint8_t brightness);
    void setZoneSpeed(uint8_t zone, uint8_t speed);
    void setZonePalette(uint8_t zone, uint8_t paletteId);
    void setZoneBlendMode(uint8_t zone, BlendMode mode);

    // Presets
    void loadPreset(uint8_t presetId);  // 0-4
};
```

### Blend Modes

Zones composite using 8 blend modes:

| Mode | Behavior |
|------|----------|
| `OVERWRITE` | Replace: pixel = new |
| `ADDITIVE` | Light accumulation (pre-scaled) |
| `MULTIPLY` | Darkening: (pixel * new) / 255 |
| `SCREEN` | Lightening: inverse multiply |
| `OVERLAY` | Multiply if dark, screen if light |
| `ALPHA` | 50/50 mix |
| `LIGHTEN` | Take brighter pixel |
| `DARKEN` | Take darker pixel |

---

## Transition Engine

The TransitionEngine provides 12 CENTER ORIGIN-compliant transitions between effects.

### Transition Types

| Type | Description | Duration |
|------|-------------|----------|
| `FADE` | Crossfade radiating from center | 800ms |
| `WIPE_OUT` | Circular wipe center to edges | 1200ms |
| `WIPE_IN` | Circular wipe edges to center | 1200ms |
| `DISSOLVE` | Random pixel transition | 1500ms |
| `PHASE_SHIFT` | Frequency-based wave morphing | 1400ms |
| `PULSEWAVE` | Concentric energy rings | 2000ms |
| `IMPLOSION` | Particles converge to center | 1500ms |
| `IRIS` | Aperture open/close | 1200ms |
| `NUCLEAR` | Chain reaction explosion | 2500ms |
| `STARGATE` | Wormhole portal | 3000ms |
| `KALEIDOSCOPE` | Symmetric patterns | 1800ms |
| `MANDALA` | Sacred geometry rings | 2200ms |

### Easing Curves

15 easing curves for natural motion:

- `LINEAR`, `IN_QUAD`, `OUT_QUAD`, `IN_OUT_QUAD`
- `IN_CUBIC`, `OUT_CUBIC`, `IN_OUT_CUBIC`
- `IN_ELASTIC`, `OUT_ELASTIC`, `IN_OUT_ELASTIC`
- `IN_BOUNCE`, `OUT_BOUNCE`
- `IN_BACK`, `OUT_BACK`, `IN_OUT_BACK`

### Usage

```cpp
transitionEngine.startTransition(
    sourceBuffer,                    // Old effect
    targetBuffer,                    // New effect
    outputBuffer,                    // Result
    TransitionType::STARGATE,        // Type
    2000,                            // Duration ms
    EasingCurve::OUT_ELASTIC         // Easing
);

// In render loop
if (transitionEngine.isActive()) {
    transitionEngine.update();
}
```

---

## Enhancement Engines

### ColorEngine

Cross-palette blending, temporal rotation, and color diffusion:

```cpp
auto& colorEngine = ColorEngine::getInstance();

// Cross-blend two palettes
colorEngine.enableCrossBlend(true);
colorEngine.setBlendPalettes(HeatColors_p, OceanColors_p);
colorEngine.setBlendFactors(200, 55);  // 80% heat, 20% ocean

// Temporal rotation
colorEngine.enableTemporalRotation(true);
colorEngine.setRotationSpeed(0.5f);  // degrees/frame

// Get enhanced color
CRGB color = colorEngine.getColor(index, brightness);
```

### MotionEngine

Phase control, particle physics, and speed modulation:

```cpp
auto& motionEngine = MotionEngine::getInstance();
motionEngine.enable();

// Phase offset between strips
motionEngine.getPhaseController().setStripPhaseOffset(45.0f);
motionEngine.getPhaseController().enableAutoRotate(30.0f);

// Particle system
auto& momentum = motionEngine.getMomentumEngine();
int id = momentum.addParticle(0.5f, 0.1f, 1.0f, CRGB::Red);
momentum.applyForce(id, 0.05f);
```

### SmoothingEngine

Frame-rate independent smoothing primitives:

```cpp
// True exponential decay (NOT approximation)
ExpDecay smoother{0.0f, 5.0f};
float smoothed = smoother.update(target, dt);

// Asymmetric follower (fast attack, slow decay)
AsymmetricFollower follower{0.0f, 0.05f, 0.30f};
float value = follower.update(target, dt);

// Subpixel rendering for smooth motion
SubpixelRenderer::renderPoint(leds, 160, 45.7f, CRGB::Red, 255);
```

---

## Audio-Reactive Effects

**CRITICAL: Before creating audio-reactive effects, read:**

**[/docs/audio-visual/audio-visual-semantic-mapping.md](/docs/audio-visual/audio-visual-semantic-mapping.md)**

### Key Requirements

1. **Complete Layer Audit Protocol** - Understand ALL layers of the effect
2. **NO rigid frequency-to-visual bindings** - "bass equals expansion" is a trap
3. **Musical saliency analysis** - Respond to what is IMPORTANT, not all signals
4. **Style-adaptive response** - EDM/vocal/ambient need different strategies
5. **Temporal context** - Use history, not just instantaneous values

### AudioContext API

```cpp
void render(plugins::EffectContext& ctx) override {
    if (!ctx.audio.available) {
        // Fall back to time-based animation
        return;
    }

    // Energy metrics
    float energy = ctx.audio.rms();         // 0.0-1.0
    float bass = ctx.audio.bass();          // Low frequency energy
    float treble = ctx.audio.treble();      // High frequency energy

    // Beat tracking
    float phase = ctx.audio.beatPhase();    // 0.0-1.0, wraps each beat
    bool onBeat = ctx.audio.isOnBeat();     // Single-frame pulse
    float bpm = ctx.audio.bpm();            // Current tempo

    // Musical intelligence
    float saliency = ctx.audio.overallSaliency();
    bool rhythmDriven = ctx.audio.isRhythmicDominant();

    // Behavior recommendation
    if (ctx.audio.shouldPulseOnBeat()) {
        // Effect should pulse with rhythm
    } else if (ctx.audio.shouldBreatheWithDynamics()) {
        // Effect should breathe with energy envelope
    }
}
```

---

## Creating New Effects

### Step 1: Create Header

```cpp
// File: ieffect/MyNewEffect.h
#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class MyNewEffect : public plugins::IEffect {
public:
    MyNewEffect();
    ~MyNewEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state
    float m_phase;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
```

### Step 2: Create Implementation

```cpp
// File: ieffect/MyNewEffect.cpp
#include "MyNewEffect.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

MyNewEffect::MyNewEffect() : m_phase(0.0f) {}

bool MyNewEffect::init(plugins::EffectContext& ctx) {
    m_phase = 0.0f;
    return true;
}

void MyNewEffect::render(plugins::EffectContext& ctx) {
    // Apply fade for trails
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // CENTER ORIGIN: Use distance from center
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        float dist = ctx.getDistanceFromCenter(i);

        // Wave emanating from center
        float wave = sinf((dist * 10.0f) - m_phase);
        uint8_t brightness = (uint8_t)((wave + 1.0f) * 127.5f);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(dist * 255), brightness);
    }

    // Advance phase (speed-controlled)
    m_phase += ctx.speed * 0.01f;
}

void MyNewEffect::cleanup() {
    // Nothing to clean up
}

const plugins::EffectMetadata& MyNewEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "My New Effect",
        "Waves radiating from center",
        plugins::EffectCategory::GEOMETRIC,
        1,
        nullptr
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
```

### Step 3: Register Effect

Add to the effect registry in the RendererActor initialization.

### Guidelines

1. **No global variables** - Use `ctx` for everything
2. **No heap allocations in render()** - Pre-allocate in `init()`
3. **CENTER ORIGIN compliance** - Use `ctx.getDistanceFromCenter(i)`
4. **No rainbows** - Keep hue range under 60 degrees
5. **Frame-rate independence** - Use `ctx.deltaTimeMs` for animation speed

---

## Related Documentation

| Document | Purpose |
|----------|---------|
| [docs/audio-visual/audio-visual-semantic-mapping.md](/docs/audio-visual/audio-visual-semantic-mapping.md) | **REQUIRED** for audio-reactive effects |
| [docs/audio-visual/audio-reactive-effects-analysis.md](/docs/audio-visual/audio-reactive-effects-analysis.md) | Audio effect patterns |
| [firmware/v2/docs/audio-visual/VISUAL_PIPELINE_MECHANICS.md](../../../docs/audio-visual/VISUAL_PIPELINE_MECHANICS.md) | Rendering pipeline details |
| [firmware/v2/docs/audio-visual/COLOR_PALETTE_SYSTEM.md](../../../docs/audio-visual/COLOR_PALETTE_SYSTEM.md) | Palette organization |
| [firmware/v2/src/plugins/api/IEffect.h](../plugins/api/IEffect.h) | Interface definition |
| [firmware/v2/src/plugins/api/EffectContext.h](../plugins/api/EffectContext.h) | Context API reference |

---

## File Reference

| File | Lines | Purpose |
|------|-------|---------|
| `PatternRegistry.h` | 312 | Pattern taxonomy and metadata |
| `zones/ZoneComposer.h` | 203 | Multi-zone orchestrator |
| `zones/ZoneDefinition.h` | 154 | Zone segment layouts |
| `zones/BlendMode.h` | 129 | 8 blend modes |
| `transitions/TransitionEngine.h` | 199 | 12 transition types |
| `transitions/TransitionTypes.h` | 115 | Type definitions |
| `transitions/Easing.h` | 170 | 15 easing curves |
| `enhancement/ColorEngine.h` | 282 | Palette blending |
| `enhancement/MotionEngine.h` | 368 | Phase/particle control |
| `enhancement/SmoothingEngine.h` | 332 | Frame-rate independent smoothing |
| `utils/FastLEDOptim.h` | 232 | FastLED optimizations |
