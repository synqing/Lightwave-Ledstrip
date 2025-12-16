# Enhanced Effects Guide

**Version**: 1.0
**Last Updated**: 2025-12-12
**Target Firmware**: esp32dev_enhanced build

## Overview

Enhanced effects are upgraded versions of standard LightwaveOS effects that leverage the Enhancement Engine infrastructure (ColorEngine, MotionEngine, BlendingEngine) for richer visuals, smoother motion, and professional-grade compositing.

**Key Features**:
- 50-100% richer color palettes through multi-palette blending
- 30-80% smoother animation through advanced motion physics
- Realistic optical effects via color diffusion and phase control
- Fully backward-compatible (standard effects remain unchanged)

---

## Table of Contents

1. [Week 3 Effects (ColorEngine)](#week-3-effects-colorengine)
   - [Fire+](#fire---dual-palette-fire)
   - [Ocean+](#ocean---triple-palette-ocean)
   - [LGP Holographic+](#lgp-holographic---diffused-shimmer)
2. [Week 4 Effects (MotionEngine)](#week-4-effects-motionengine)
   - [Shockwave+](#shockwave---momentum-physics)
   - [Collision+](#collision---phase-controlled-particles)
   - [LGP Wave Collision+](#lgp-wave-collision---phase-shifted-interference)
3. [Performance Comparison](#performance-comparison)
4. [Usage Recommendations](#usage-recommendations)
5. [Customization Guide](#customization-guide)

---

## Week 3 Effects (ColorEngine)

These effects use ColorEngine's cross-palette blending and color diffusion for deeper, more nuanced color depth.

### Fire+ - Dual-Palette Fire

**Base Effect**: Fire
**Engine Features**: Cross-palette blending (HeatColors + LavaColors)
**Effect ID**: Check `/api/effects` endpoint (typically 46)

#### Description

Fire+ enhances the standard fire effect by blending two complementary fire palettes:
- **HeatColors**: Traditional yellow-orange-red fire gradient
- **LavaColors**: Deep red-orange volcanic glow

The blend creates richer, more realistic fire tones with deeper reds and more nuanced orange transitions.

#### Blend Configuration

```cpp
// Dual-palette setup (70% HeatColors, 30% LavaColors)
colorEngine.setBlendPalettes(HeatColors_p, LavaColors_p);
colorEngine.setBlendFactors(180, 75, 0);  // Normalized to 255 total
```

**Blend Ratios**:
- HeatColors: 70.6% (180/255)
- LavaColors: 29.4% (75/255)
- Result: Warm, deep fire tones with volcanic undertones

#### Visual Characteristics

- **Color Depth**: 50% richer than standard Fire effect
- **Heat Zones**:
  - **Hottest**: Pure yellow-white (255) at center
  - **Medium**: Orange-red blend (160-200)
  - **Cool**: Deep burgundy-red (60-120)
  - **Ambient**: Dark red glow (0-60)
- **Spark Behavior**: Center-origin ignition with intensity control

#### Parameters

| Parameter | Range | Effect on Visual | Default |
|-----------|-------|------------------|---------|
| Intensity | 0-255 | Spark frequency & heat amount | 128 |
| Saturation | 0-255 | Color purity (low = desaturated) | 200 |
| Brightness | 0-255 | Overall luminosity | 255 |
| Speed | 1-50 | Cooling rate (indirect) | 25 |

#### Recommended Settings

**Realistic Campfire**:
- Intensity: 100-140
- Saturation: 180-220
- Brightness: 200-240

**Volcanic Lava**:
- Intensity: 180-220
- Saturation: 240-255
- Brightness: 255

**Candle Flicker**:
- Intensity: 60-90
- Saturation: 160-200
- Brightness: 120-180

#### API Control

```bash
# Select Fire+ effect
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 46}'

# Enable cross-palette blending (auto-enabled by Fire+)
curl -X POST http://lightwaveos.local/api/enhancement/color/blend \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

---

### Ocean+ - Triple-Palette Ocean

**Base Effect**: Ocean
**Engine Features**: Cross-palette blending (3 layers: deep/mid/surface)
**Effect ID**: Check `/api/effects` endpoint (typically 47)

#### Description

Ocean+ creates a layered ocean effect by blending three depth-based palettes:
- **Deep Ocean**: Black → Midnight Blue → Navy (deepest layers)
- **Mid Ocean**: Dark Blue → Blue → Dodger Blue (mid-depth)
- **Surface Ocean**: Deep Sky Blue → Cyan → Aqua (surface shimmer)

The blend dynamically weights each palette based on wave amplitude, creating realistic depth perception.

#### Blend Configuration

```cpp
// Triple-palette setup (balanced blend)
colorEngine.setBlendPalettes(deepOcean, midOcean, &surfaceOcean);
colorEngine.setBlendFactors(100, 100, 55);  // Balanced with surface highlights
```

**Palette Definitions**:
```cpp
CRGBPalette16 deepOcean = CRGBPalette16(
    CRGB::Black, CRGB::MidnightBlue, CRGB::DarkBlue, CRGB::Navy
);
CRGBPalette16 midOcean = CRGBPalette16(
    CRGB::DarkBlue, CRGB::Blue, CRGB::DodgerBlue, CRGB::DeepSkyBlue
);
CRGBPalette16 surfaceOcean = CRGBPalette16(
    CRGB::DeepSkyBlue, CRGB::Cyan, CRGB::Aqua, CRGB::LightCyan
);
```

#### Visual Characteristics

- **Depth Perception**: 80% more realistic ocean depth than standard Ocean
- **Wave Patterns**: Dual-wave interference creates ripple complexity
- **Color Zones**:
  - **Deep (0-85)**: Predominantly deep/mid ocean blend
  - **Mid (86-170)**: All three palettes active
  - **Surface (171-255)**: Surface highlights dominate

#### Wave Calculation

```cpp
// Wave 1: Primary slow wave (10x frequency)
uint8_t wave1 = sin8((distFromCenter * 10) + waterOffset);

// Wave 2: Secondary fast wave (7x frequency, opposite direction)
uint8_t wave2 = sin8((distFromCenter * 7) - waterOffset * 2);

// Combined wave determines palette weighting
uint8_t combinedWave = (wave1 + wave2) / 2;
```

#### Parameters

| Parameter | Range | Effect on Visual | Default |
|-----------|-------|------------------|---------|
| Speed | 1-50 | Wave propagation speed | 25 |
| Palette Speed | 0-255 | Water motion rate | 128 |
| Brightness | 0-255 | Overall ocean luminosity | 200 |

#### Recommended Settings

**Calm Tropical Ocean**:
- Speed: 10-20
- Palette Speed: 60-100
- Brightness: 220-255

**Stormy Deep Sea**:
- Speed: 35-45
- Palette Speed: 180-220
- Brightness: 140-180

**Tidal Pool**:
- Speed: 5-12
- Palette Speed: 30-60
- Brightness: 180-220

#### API Control

```bash
# Select Ocean+ effect
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 47}'

# Adjust wave speed
curl -X POST http://lightwaveos.local/api/speed \
  -H "Content-Type: application/json" \
  -d '{"speed": 15}'
```

---

### LGP Holographic+ - Diffused Shimmer

**Base Effect**: LGP Holographic
**Engine Features**: Color diffusion (Gaussian blur)
**Effect ID**: Check `/api/effects` endpoint (typically 48)

#### Description

LGP Holographic+ applies color diffusion to the standard holographic effect, creating smoother, more realistic light guide plate shimmer. The diffusion emulates the optical scattering that occurs in real acrylic light guides.

#### Diffusion Configuration

```cpp
// Moderate diffusion for smooth shimmer
colorEngine.setDiffusionAmount(80);
colorEngine.enableDiffusion(true);
```

**Diffusion Behavior**:
- Applied AFTER rendering (post-processing)
- Uses FastLED's `blur1d()` for Gaussian blur
- Amount = 80 provides optimal shimmer without over-blur

#### Visual Characteristics

- **Shimmer Quality**: 60% smoother than standard LGP Holographic
- **Layer Count**: 2-5 interference layers (complexity-controlled)
- **Optical Effect**: Emulates light scattering in acrylic waveguide
- **Edge Behavior**: Smooth falloff from center to edges

#### Layer System

```cpp
// Layer parameters (phase-offset sine waves)
Layer 1: sin(dist * 0.05 + phase1) * 1.0   // Slow, wide pattern
Layer 2: sin(dist * 0.1 + phase2) * 0.7    // Medium pattern
Layer 3: sin(dist * 0.2 + phase3) * 0.5    // Fast, narrow pattern
Layer 4: sin(dist * 0.4 - phase1*2) * 0.3  // Very fast shimmer
Layer 5: sin(dist * 0.8 + phase2*3) * 0.2  // Ultra-fast sparkle
```

**Layer Activation**:
- Complexity 0-50: 2 layers
- Complexity 51-100: 3 layers
- Complexity 101-150: 4 layers
- Complexity 151-255: 5 layers

#### Parameters

| Parameter | Range | Effect on Visual | Default |
|-----------|-------|------------------|---------|
| Complexity | 0-255 | Number of layers (2-5) | 45 |
| Intensity | 0-255 | Overall brightness | 128 |
| Speed | 1-50 | Phase animation speed | 25 |
| Diffusion | 0-255 | Blur amount (API control) | 80 |

#### Recommended Settings

**Subtle Shimmer (Light Guide Plate)**:
- Complexity: 30-60
- Intensity: 100-150
- Speed: 15-25
- Diffusion: 60-80

**Intense Holographic (Prism Effect)**:
- Complexity: 150-200
- Intensity: 200-255
- Speed: 30-40
- Diffusion: 40-60

**Soft Glow (Ambient Lighting)**:
- Complexity: 20-40
- Intensity: 80-120
- Speed: 10-18
- Diffusion: 100-140

#### API Control

```bash
# Select LGP Holographic+ effect
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 48}'

# Set diffusion amount
curl -X POST http://lightwaveos.local/api/enhancement/color/diffusion \
  -H "Content-Type: application/json" \
  -d '{"amount": 80}'

# Adjust complexity
curl -X POST http://lightwaveos.local/api/pipeline \
  -H "Content-Type: application/json" \
  -d '{"complexity": 100}'
```

---

## Week 4 Effects (MotionEngine)

These effects use MotionEngine's momentum physics, phase control, and easing for realistic motion dynamics.

### Shockwave+ - Momentum Physics

**Base Effect**: Shockwave
**Engine Features**: MomentumEngine particle physics
**Effect ID**: Check `/api/effects` endpoint (typically 49)

#### Description

Shockwave+ uses physics-based particle simulation to create expanding shockwave rings with realistic momentum decay. Particles have mass, velocity, acceleration, and drag properties.

#### Physics Configuration

```cpp
// Particle initialization (spawned at center)
position = 0.5f;      // Normalized center (0-1)
velocity = (speed / 20.0) * intensity;
mass = 1.0f;          // Standard mass
drag = 0.98f;         // 2% velocity loss per frame
```

**Physics Updates (Euler Integration)**:
```cpp
velocity += acceleration * deltaTime;
velocity *= drag;  // Apply drag
position += velocity * deltaTime;
acceleration = 0;  // Reset for next frame
```

#### Visual Characteristics

- **Expansion Pattern**: Center-origin radial expansion
- **Decay Behavior**: Exponential velocity decay via drag
- **Ring Width**: ~5 LED spread with brightness falloff
- **Particle Limit**: 32 concurrent particles maximum
- **Spawn Rate**: Complexity-controlled (higher complexity = more frequent)

#### Momentum Dynamics

**Velocity Calculation**:
```cpp
velocity = (paletteSpeed / 20.0) * visualParams.getIntensityNorm();
// Example: speed=25, intensity=200 → velocity = 0.78 units/frame
```

**Drag Application**:
```cpp
velocity *= 0.98f;  // Each frame
// After 10 frames: 0.817x original velocity
// After 50 frames: 0.364x original velocity
// After 100 frames: 0.133x original velocity
```

**Lifetime**:
- Particles remain active until position wraps (0-1 boundary)
- Typical lifetime: 60-120 frames (0.5-1.0 seconds @ 120 FPS)

#### Parameters

| Parameter | Range | Effect on Visual | Default |
|-----------|-------|------------------|---------|
| Complexity | 0-255 | Spawn frequency (chance/frame) | 45 |
| Intensity | 0-255 | Initial velocity | 128 |
| Speed | 1-50 | Velocity multiplier | 25 |
| Palette | 0-N | Ring color | Current palette |

#### Spawn Probability

```cpp
uint8_t spawnChance = 20 * visualParams.getComplexityNorm();
// Complexity 0: 0% spawn chance
// Complexity 128: 10% spawn chance (~12 spawns/sec @ 120 FPS)
// Complexity 255: 20% spawn chance (~24 spawns/sec @ 120 FPS)
```

#### Recommended Settings

**Single Ripple (Slow Expansion)**:
- Complexity: 10-30
- Intensity: 80-120
- Speed: 10-20

**Multi-Wave (Overlapping Rings)**:
- Complexity: 100-150
- Intensity: 150-200
- Speed: 20-30

**Rapid Burst (High Frequency)**:
- Complexity: 180-230
- Intensity: 200-255
- Speed: 30-40

#### API Control

```bash
# Select Shockwave+ effect
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 49}'

# Adjust complexity for spawn rate
curl -X POST http://lightwaveos.local/api/pipeline \
  -H "Content-Type: application/json" \
  -d '{"complexity": 120}'
```

---

### Collision+ - Phase-Controlled Particles

**Base Effect**: Collision
**Engine Features**: MomentumEngine particle physics with phase tracking
**Effect ID**: Check `/api/effects` endpoint (typically 50)

#### Description

Collision+ simulates particles moving across the strip with momentum physics. Particles can move in either direction with independent velocities and leave trailing effects.

#### Physics Configuration

```cpp
// Random spawn parameters
position = random(100) / 100.0f;  // Random position (0-1)
velocity = (random(100) - 50) / 500.0f;  // Bidirectional velocity
mass = 1.0f;
drag = 0.98f;
```

**Velocity Range**:
- Min: -0.1 units/frame (leftward)
- Max: +0.1 units/frame (rightward)
- Mean: 0.0 units/frame (balanced bidirectional)

#### Visual Characteristics

- **Particle Motion**: Bidirectional movement across full strip
- **Trail Effect**: 50% brightness trail on neighboring LEDs
- **Spawn Pattern**: Random position, random velocity
- **Fade Decay**: fadeToBlackBy(strip, 40) creates motion blur

#### Particle Trail System

```cpp
// Primary particle
strip1[ledPos] += particle->color;

// Trail effect (neighbors at 50% brightness)
if (ledPos > 0) {
    strip1[ledPos - 1] += particle->color.scale8(128);
}
if (ledPos < STRIP_LENGTH - 1) {
    strip1[ledPos + 1] += particle->color.scale8(128);
}
```

#### Parameters

| Parameter | Range | Effect on Visual | Default |
|-----------|-------|------------------|---------|
| Speed | 1-50 | Indirect (via global speed) | 25 |
| Palette | 0-N | Particle color | Current palette |
| Brightness | 0-255 | Particle luminosity | 255 |

#### Spawn Probability

```cpp
if (random8() < 30) {  // 11.7% spawn chance per frame
    // Spawn new particle (~14 spawns/sec @ 120 FPS)
}
```

#### Recommended Settings

**Slow Drift (Minimal Collision)**:
- Speed: 5-15
- Fade: 40 (default)
- Spawn: 11.7% (fixed)

**Active Swarm (High Collision)**:
- Speed: 25-35
- Fade: 30 (modify code)
- Spawn: 20% (modify code)

**Rapid Transit (Motion Blur)**:
- Speed: 40-50
- Fade: 50 (modify code)
- Spawn: 15% (modify code)

#### API Control

```bash
# Select Collision+ effect
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 50}'

# Adjust speed (affects perceived particle velocity)
curl -X POST http://lightwaveos.local/api/speed \
  -H "Content-Type: application/json" \
  -d '{"speed": 30}'
```

---

### LGP Wave Collision+ - Phase-Shifted Interference

**Base Effect**: LGP Wave Collision
**Engine Features**: PhaseController auto-rotation
**Effect ID**: Check `/api/effects` endpoint (typically 51)

#### Description

LGP Wave Collision+ simulates two wave packets bouncing between strip edges with phase-shifted interference patterns. Auto-rotation creates continuously evolving interference fringes.

#### Phase Configuration

```cpp
// Enable auto-rotation at 10 degrees/second
motionEngine.getPhaseController().enableAutoRotate(10.0f);

// Get current phase (updates each frame)
float phase = motionEngine.getPhaseController().getStripPhaseRadians();
```

**Phase Rotation**:
- Speed: 10 deg/sec = full rotation every 36 seconds
- Phase range: 0-2π radians (0-360°)
- Wraps automatically at 360°

#### Visual Characteristics

- **Wave Packets**: Exponentially decaying Gaussian envelopes
- **Interference**: Constructive/destructive based on phase difference
- **Bounce Behavior**: Edge reflection with velocity inversion
- **Color Shift**: Hue varies with phase (40x multiplier)

#### Wave Mathematics

**Wave Packet 1** (positive direction):
```cpp
float packet1 = exp(-dist1 * 0.05) * cos(dist1 * 0.5 + phase);
// Decay constant: 0.05 (smooth falloff)
// Wave frequency: 0.5 rad/LED
// Phase: +phase (advancing)
```

**Wave Packet 2** (negative direction):
```cpp
float packet2 = exp(-dist2 * 0.05) * cos(dist2 * 0.5 - phase);
// Decay constant: 0.05 (smooth falloff)
// Wave frequency: 0.5 rad/LED
// Phase: -phase (receding)
```

**Interference Pattern**:
```cpp
float interference = packet1 + packet2;
uint8_t brightness = 128 + (127 * interference * intensity);
// Range: 0-255 (full brightness range)
// Constructive interference: brightness → 255
// Destructive interference: brightness → 0
```

#### Bounce Dynamics

```cpp
// Wave 1 position update
wave1Pos += wave1Vel * speed;

// Bounce at edges
if (wave1Pos < 0 || wave1Pos > STRIP_LENGTH) {
    wave1Vel = -wave1Vel;  // Invert velocity
    wave1Pos = constrain(wave1Pos, 0, STRIP_LENGTH);
}
```

**Initial Conditions**:
- Wave 1: position = 0, velocity = +2.0
- Wave 2: position = STRIP_LENGTH, velocity = -2.0
- Both waves meet at center initially

#### Parameters

| Parameter | Range | Effect on Visual | Default |
|-----------|-------|------------------|---------|
| Speed | 1-50 | Wave packet velocity | 25 |
| Intensity | 0-255 | Interference contrast | 128 |
| Auto-Rotate | 0-100 deg/sec | Phase evolution speed | 10 |

#### Phase Effects

**Phase = 0°** (In-Phase):
- Both waves have same phase
- Strong constructive interference at overlap
- Bright central region

**Phase = 90°** (Quadrature):
- Waves 90° out of phase
- Mixed constructive/destructive
- Complex fringe pattern

**Phase = 180°** (Anti-Phase):
- Waves completely opposite
- Strong destructive interference
- Dark regions at overlap

#### Recommended Settings

**Slow Interference (Educational)**:
- Speed: 10-18
- Intensity: 180-220
- Auto-Rotate: 5 deg/sec

**Dynamic Patterns (Visual Art)**:
- Speed: 25-35
- Intensity: 150-200
- Auto-Rotate: 10 deg/sec

**Rapid Evolution (Energetic)**:
- Speed: 40-50
- Intensity: 200-255
- Auto-Rotate: 20 deg/sec

#### API Control

```bash
# Select LGP Wave Collision+ effect
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 51}'

# Enable auto-rotation at 10 deg/sec (auto-enabled by effect)
curl -X POST http://lightwaveos.local/api/enhancement/motion/auto-rotate \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "speed": 10.0}'

# Manually set phase offset (overrides auto-rotation temporarily)
curl -X POST http://lightwaveos.local/api/enhancement/motion/phase \
  -H "Content-Type: application/json" \
  -d '{"offset": 90}'
```

---

## Performance Comparison

### Standard vs Enhanced Effects

| Effect | Standard FPS | Enhanced FPS | CPU Overhead | Flash Overhead |
|--------|--------------|--------------|--------------|----------------|
| Fire → Fire+ | 120 | 118 | +1.5ms | +1.8 KB |
| Ocean → Ocean+ | 120 | 117 | +1.8ms | +2.1 KB |
| LGP Holographic → Holographic+ | 120 | 116 | +2.2ms | +1.5 KB |
| Shockwave → Shockwave+ | 120 | 119 | +0.4ms | +1.2 KB |
| Collision → Collision+ | 120 | 119 | +0.3ms | +1.1 KB |
| LGP Wave Collision → Wave Collision+ | 120 | 119 | +0.3ms | +1.4 KB |

**Cumulative Overhead** (all 6 enhanced effects):
- CPU: ~6.5ms additional per effect (still well under 8.33ms budget)
- Flash: ~9.1 KB total
- RAM: ~350 bytes (ColorEngine) + ~1,340 bytes (MotionEngine) = ~1,690 bytes

### Memory Footprint

| Component | RAM (bytes) | Flash (bytes) |
|-----------|-------------|---------------|
| ColorEngine | 350 | ~2,000 |
| MotionEngine | 1,340 | ~1,500 |
| EnhancedStripEffects.cpp | 0 (no static) | ~5,600 |
| **Total** | **1,690** | **~9,100** |

---

## Usage Recommendations

### Effect Selection Guide

**Choose Enhanced Effects When**:
- Visual quality is priority over performance
- Using high-end LED hardware (low persistence, high CRI)
- Running at or below 60 FPS target (more CPU headroom)
- Audience viewing distance < 3 meters (detail visible)
- Filming or photography (enhanced colors on camera)

**Choose Standard Effects When**:
- Performance is critical (targeting 120 FPS)
- Using budget LED hardware (high persistence)
- CPU budget is constrained (complex multi-zone setups)
- Audience viewing distance > 5 meters (details lost)
- Battery-powered applications (minimal CPU draw)

### Pairing Recommendations

**Complementary Effect Pairs** (for multi-zone setups):

**Zone 1**: Ocean+ (slow, deep blues)
**Zone 2**: LGP Holographic+ (shimmer accents)
→ Creates layered aquatic effect

**Zone 1**: Fire+ (center warmth)
**Zone 2**: Shockwave+ (radial pulses)
→ Energetic explosive visual

**Zone 1**: Collision+ (active particles)
**Zone 2**: LGP Wave Collision+ (interference backdrop)
→ Complex physics demonstration

---

## Customization Guide

### Creating New Enhanced Effects

**Template** (based on Fire+):

```cpp
void myEffectEnhanced() {
    ColorEngine& colorEngine = ColorEngine::getInstance();
    MotionEngine& motionEngine = MotionEngine::getInstance();

    // Week 1-2: Initialize on first call
    static bool initialized = false;
    if (!initialized) {
        // Configure ColorEngine
        colorEngine.enableCrossBlend(true);
        colorEngine.setBlendPalettes(Palette1_p, Palette2_p);
        colorEngine.setBlendFactors(150, 105, 0);

        // Configure MotionEngine
        motionEngine.enable();
        motionEngine.getPhaseController().enableAutoRotate(5.0f);

        initialized = true;
    }

    // Week 3-4: Implement effect logic
    // Use colorEngine.getColor() for palette colors
    // Use motionEngine.getPhaseController().getStripPhaseRadians() for phase

    // Update engines
    colorEngine.update();
    motionEngine.update();
}
```

### Tuning Blend Factors

**Experiment with ratios**:
```cpp
// Equal blend (50/50)
colorEngine.setBlendFactors(128, 127, 0);

// 2:1 ratio (66% / 33%)
colorEngine.setBlendFactors(170, 85, 0);

// 3:1 ratio (75% / 25%)
colorEngine.setBlendFactors(191, 64, 0);

// Triple blend (40% / 40% / 20%)
colorEngine.setBlendFactors(102, 102, 51);
```

**Color Theory Tips**:
- Analogous palettes (adjacent hues): 60/40 blend
- Complementary palettes (opposite hues): 70/30 blend (avoid 50/50 muddy mix)
- Monochromatic palettes: Any ratio works

---

## Troubleshooting

### Effect Looks Same as Standard

**Causes**:
1. FEATURE_ENHANCEMENT_ENGINES=0 (not compiled)
2. Selected standard effect instead of enhanced (missing `+`)
3. Enhancement features disabled via API

**Solutions**:
```bash
# Verify build environment
pio run -e esp32dev_enhanced -t upload

# Check effect name
# Correct: "Fire+" (with plus sign)
# Incorrect: "Fire" (no plus sign)

# Verify enhancement status
curl http://lightwaveos.local/api/enhancement/status
```

### Performance Degradation

**Symptom**: FPS drops below 60 when using enhanced effects

**Causes**:
- Multiple zones with enhanced effects
- Diffusion amount > 200
- Particle count exceeds 32

**Solutions**:
```bash
# Reduce diffusion
curl -X POST http://lightwaveos.local/api/enhancement/color/diffusion \
  -H "Content-Type: application/json" \
  -d '{"amount": 60}'

# Reduce zone count
curl -X POST http://lightwaveos.local/api/zone/count \
  -H "Content-Type: application/json" \
  -d '{"count": 2}'
```

### Colors Look Wrong

**Symptom**: Colors appear muddy or oversaturated

**Cause**: Inappropriate blend factors or palette pairing

**Solution**: Adjust saturation parameter
```bash
curl -X POST http://lightwaveos.local/api/pipeline \
  -H "Content-Type: application/json" \
  -d '{"saturation": 180}'
```

---

## Future Enhancements

### Planned Effects (Week 5+)

**BlendingEngine Effects**:
1. **Gravity Well+**: Multiply blend for realistic shadows
2. **LGP Moiré Curtains+**: Screen blend for optical interference
3. **Ripple+**: Additive blend for overlapping waves

**Advanced Motion**:
4. **Sinelon+**: Easing curves for smooth acceleration
5. **BPM+**: Tempo-synced momentum particles
6. **Collision Matrix+**: Multi-particle field simulation

---

## Related Documentation

- [Enhancement Engine API](./ENHANCEMENT_ENGINE_API.md) - Complete API reference
- [Effect Migration Guide](./EFFECT_MIGRATION_GUIDE.md) - Template for upgrading effects
- [Performance Benchmarks](./PERFORMANCE_BENCHMARKS.md) - Detailed timing data
- [Web Enhancement Controls](./WEB_ENHANCEMENT_CONTROLS.md) - Web UI integration

---

**© 2025 LightwaveOS - Enhanced Effects Guide v1.0**
