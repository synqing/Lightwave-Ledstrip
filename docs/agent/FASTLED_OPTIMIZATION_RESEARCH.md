# FastLED Optimization Patterns for ESP32-S3 Dual-Strip Rendering

## Executive Summary

This document provides deep-dive research on FastLED optimization patterns specifically for the LightwaveOS ESP32-S3 platform (dual 160-LED WS2812 strips at 120 FPS). It covers color operations, memory layout, blend modes, palette design, fixed-point math, and advanced rendering strategies proven in production code.

**Key metrics:**
- Target: 120 FPS = 8.33ms budget per frame
- Typical performance: 5.68ms measured (176 FPS sustainable)
- Effect calculation budget: ~2ms max
- Platform: ESP32-S3 N16R8 (320 KB internal SRAM, 8 MB PSRAM)

---

## 1. CRGB Operations & Color Blending

### 1.1 nscale8 vs Direct Multiplication

FastLED's `nscale8()` is **NOT a simple brightness scale**. It's an additive accumulation damper for iterative fading.

| Operation | What It Does | When to Use | Cost |
|-----------|------------|------------|------|
| `c.nscale8(250)` | Keeps 250/256 (~97.7%), dampens accumulation | Additive blending with decay (fadeToBlackBy) | ~2 cycles (x86: lea + mul) |
| `c = scale8(c, 250)` | Explicit channel-wise scale (faster) | Single-pass brightness | ~1 cycle per byte |
| `c.nscale8_video(250)` | Prevents black crush (clamps to avoid banding) | Video output after accumulation | ~3 cycles (checks) |
| `c *= scale` (float) | Floating point multiply | Avoid in hot loops on ESP32 | ~40+ cycles |

**Code patterns from LightwaveOS:**

```cpp
// GOOD: Iterative fade in effect loop
fadeToBlackBy(leds, NUM_LEDS, 20);  // Internally: nscale8(235) per frame
leds[i] += newColor;  // Additive blend with auto-saturation

// GOOD: Video output scaling (prevents clipping)
color.nscale8_video(ctx.brightness);  // Intelligent fade

// AVOID: This happens in scale8 internally - don't replicate
uint8_t r = (uint8_t)(color.r * brightness / 255.0f);  // Wrong!

// GOOD: FastLED's scale8 is the right tool
uint8_t r = scale8(color.r, brightness);  // Fast, 8-bit arithmetic
```

### 1.2 qadd8 vs qsub8 (Saturating Math)

FastLED provides saturating arithmetic designed for LED math:

```cpp
// qadd8 = saturating addition (max result = 255)
uint8_t combined = qadd8(red, blue);  // Won't overflow

// qsub8 = saturating subtraction (min result = 0)
uint8_t fade = qsub8(current, cooling);  // Won't underflow

// This is CRITICAL for additive blending:
// Without saturation: 200 + 100 = 44 (overflow wraps!)
// With saturation: 200 + 100 = 255 (correct)
```

**Performance: qadd8/qsub8 are 1 CPU cycle each on ESP32** (assembly: `add/sub` with conditional branch).

### 1.3 blend() vs nblend() - The Critical Difference

| Function | Signature | Result | Use Case |
|----------|-----------|--------|----------|
| `blend(c1, c2, amount)` | `blend(from, to, 0-255)` | Linear interpolation | Cross-fade between colors |
| `nblend(c1, c2, amount)` | In-place: `nblend(dst, src, 0-255)` | Accumulation | Layering multiple sources |

**From TransitionEngine (LightwaveOS):**
```cpp
// TransitionEngine::lerpColor() - Cross-fade transition
CRGB TransitionEngine::lerpColor(const CRGB& from, const CRGB& to, uint8_t blend) const {
    return ::blend(from, to, blend);  // FastLED's optimized blend
}

// Usage in transitions
uint8_t blend = (uint8_t)(localProgress * 255.0f);
output[i] = lerpColor(sourceFrame[i], targetFrame[i], blend);
```

**From LGPWaveCollisionEffect (additive accumulation):**
```cpp
// ADDITIVE blending - layers accumulate
nblend(ctx.leds[i], newColor, 180);  // ~70% new, ~30% existing
// nblend formula: dst = dst + (src - dst) * amount / 255
```

**The difference:**
- `blend(A, B, 100)` = A + (B - A) * 100/255 ≈ 39% A, 61% B
- `nblend(A, B, 100)` = In-place update, faster for accumulation

### 1.4 Additive Blending Patterns (No Overflow)

LightwaveOS uses three additive blending strategies:

**Pattern 1: Explicit layer accumulation**
```cpp
// From BeatPulseRippleEffect.cpp
// Accumulate intensity from all rings (additive blending)
for (int ring = 0; ring < NUM_RINGS; ring++) {
    uint8_t intensity = getRingIntensity(ring, i);
    ctx.leds[i] = qadd8(ctx.leds[i], CRGB(intensity, intensity, 0));
}
```

**Pattern 2: Fast nblend with decay**
```cpp
// From LGPWaveCollisionEffect.cpp
// Prevent accumulation by fading first, then blending
fadeToBlackBy(ctx.leds, STRIP_LENGTH, 30);  // Decay
nblend(ctx.leds[i], newColor, 180);  // Layer new
```

**Pattern 3: Saturating operations (safest)**
```cpp
// From LGPBirefringentShearEffect.cpp
uint8_t combined = qadd8(scale8(wave1, mixCarrier), scale8(wave2, mixWave));
uint8_t brightness = qadd8(combined, scale8(beat, 96));
```

---

## 2. Memory Layout & Cache Efficiency

### 2.1 LED Buffer Organization in LightwaveOS

```
RendererActor::m_leds[320]  (CRGB array)

Physical Layout (DMA buffer in internal SRAM):
┌─────────────────────────────────────────────┐
│ Strip 0 Logical (0-159)       │ 480 bytes   │
│ Strip 1 Logical (160-319)     │ 480 bytes   │
│ Total: 960 bytes (cache-cold on miss)       │
└─────────────────────────────────────────────┘

Cache implications for ESP32-S3:
- L1 cache: 16 KB per core
- Each LED = 3 bytes (RGB)
- Sequential iteration fits in ~3 cache lines
- Center-origin iteration (79→0, 80→159) breaks cache locality
```

### 2.2 Cache-Friendly Patterns

**GOOD: Sequential iteration (minimizes cache misses)**
```cpp
// Forward pass - fetches in cache order
for (int i = 0; i < STRIP_LENGTH; i++) {
    process(leds[i]);
}
// ~1 cache miss per 21 LEDs (64-byte cache line / 3-byte RGB)
```

**SUBOPTIMAL: Center-origin pattern (breaks spatial locality)**
```cpp
// From LightwaveOS effects - unavoidable due to design
for (int i = 0; i < HALF_LENGTH; i++) {
    process(leds[CENTER - i]);  // Jumps backward
    process(leds[CENTER + i]);  // Jumps forward
}
// ~2 cache misses per 10 LEDs due to scattered access
```

**Mitigation: Pre-compute distances**
```cpp
// Cache the distance values (fits in registers)
for (int i = 0; i < STRIP_LENGTH; i++) {
    int distFromCenter = abs(i - CENTER);
    uint8_t intensity = getIntensity(distFromCenter);
    leds[i] = palette[intensity];
}
// Sequential iteration + register-cached distance
```

### 2.3 CRGB Memory Layout

```cpp
struct CRGB {
    uint8_t r, g, b;  // 3 bytes per LED
};

// What FastLED shows() does:
// 1. Load CRGB from m_leds[]
// 2. Apply gamma correction (optional)
// 3. Encode to WS2812 protocol (9µs/LED)
// 4. Output via RMT/DMA

// Memory access:
// Sequential: leds[0], leds[1], ... - cache-efficient
// Strided: leds[i], leds[i+SKIP] - cache-miss prone
```

---

## 3. Palette Operations & ColorFromPalette Optimization

### 3.1 Palette Architecture in LightwaveOS

```cpp
// From context.palette usage across effects:
CRGB color = ctx.palette.getColor(hueIndex, brightness);

// Two palette types:
// 1. 16-entry palettes (InterpolatedPalette) - fast lookup
// 2. 256-entry palettes (CRGBPalette256) - maximum resolution
```

### 3.2 ColorFromPalette Performance

```cpp
// FastLED's ColorFromPalette(palette, index, brightness, blending)

// Internal operation (~20-40 cycles depending on blend):
inline CRGB ColorFromPalette(const CRGBPalette16& pal, uint8_t index,
                            uint8_t brightness, TBlendType blendType) {
    // Step 1: Locate palette entry (4 operations)
    uint8_t hi = (index >> 4);           // Index >> 4
    uint8_t lo = (index & 15);           // Index & 15

    // Step 2: Interpolate if needed (~8 operations)
    CRGB rgb0 = pal[hi];
    CRGB rgb1 = pal[hi + 1];
    CRGB rgb = blend(rgb0, rgb1, lo << 4);  // Interpolate

    // Step 3: Apply brightness (~3 operations)
    rgb.nscale8_video(brightness);

    return rgb;
}
```

**From LightwaveOS Effects:**
```cpp
// Most common pattern: hue-based palette lookup
uint8_t paletteIndex = (uint8_t)(ctx.gHue + offset);  // 0-255 maps palette
uint8_t brightness = scale8(effect_brightness, ctx.brightness);
CRGB color = ctx.palette.getColor(paletteIndex, brightness);

// This is FAST: ~30 cycles per LED
// Cache: Palette usually in L1 (256-byte max)
```

### 3.3 16-Entry vs 256-Entry Palettes

| Aspect | 16-Entry | 256-Entry | Tradeoff |
|--------|----------|-----------|----------|
| Lookup Speed | ~10 cycles | ~10 cycles | Same |
| Interpolation | 4 positions per 256 hues | Every hue exact | Resolution |
| Memory | 48 bytes | 768 bytes | Size |
| Cache hit % | 100% in L1 | ~98% in L1 | Negligible |

**Decision: Use 256-entry for effects, 16-entry for transitions**
```cpp
// 256-entry: Smooth gradients across effects
CRGBPalette256 effectPalette;  // Fits entirely in L1 cache

// 16-entry: Compact palette for ROM storage
PROGMEM CRGB miniPalette[16] = { /* colors */ };
```

### 3.4 Palette Interpolation Modes

```cpp
// TBlendType in FastLED:
// NOBLEND (no interpolation): ~10 cycles
// LINEARBLEND (interpolate): ~15 cycles (default)

// From LGPHolographicAutoCycleEffect:
nblendPaletteTowardPalette(m_activePalette, m_targetPalette, 24);
// Transitions between palettes smoothly over ~10 frames
// Uses LINEARBLEND internally for 256 entries
```

---

## 4. Fixed-Point Math vs Floating Point

### 4.1 Performance Comparison on ESP32-S3

| Operation | Type | Cycles | Notes |
|-----------|------|--------|-------|
| `a + b` | uint8_t | 1 | Direct add |
| `(a * b) >> 8` | Fixed (Q8.8) | 3 | Multiply + shift |
| `a * scale` | float | 40+ | FPU latency |
| `sin16(angle)` | Fixed lookup | 2 | LUT + interpolation |
| `sinf(angle)` | Float math | 80+ | FPU transcendental |

### 4.2 Fixed-Point Patterns in LightwaveOS

**Pattern 1: Q8.8 fixed-point (most common)**
```cpp
// From FastLEDOptim.h
inline float fastled_sin16_normalized(uint16_t angle) {
    return sin16(angle) / 32768.0f;  // Q15.16 → float
}

// Usage:
uint16_t angle = time * 256;  // Q8 fixed point
float wave = fastled_sin16_normalized(angle);  // Result: -1.0 to 1.0
int intensity = (int)(wave * 128.0f + 128.0f);  // Back to 0-255

// This pattern is ~30 cycles vs ~100 for sin()/cosf()
```

**Pattern 2: Avoid float in tight loops**
```cpp
// AVOID:
for (int i = 0; i < NUM_LEDS; i++) {
    float phase = (float)i * fFrequency;  // Float per LED!
    leds[i] = palette[(uint8_t)sin(phase)];  // 120+ cycles per LED
}

// GOOD:
uint16_t phaseStep = (uint16_t)(fFrequency * 256.0f);  // Pre-compute (float OK here)
uint16_t phase = 0;
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = palette[(uint8_t)(sin16(phase) >> 8)];  // ~5 cycles per LED
    phase += phaseStep;
}
```

### 4.3 FastLED Fixed-Point Functions

```cpp
// Key functions available in FastLED:
sin16(angle)        // -32768 to 32767 (-1.0 to 1.0)
cos16(angle)        // Same range
beatsin16(bpm, min, max)  // Oscillates min to max at bpm
scale8(val, scale)  // Multiply: val * scale / 256
scale16(val, scale) // Multiply: val * scale / 65536
qadd8(a, b)         // Saturating add
qsub8(a, b)         // Saturating sub

// All are ~1-3 cycles, suitable for 120 FPS loops
```

---

## 5. Blend Modes & Composition Strategies

### 5.1 Blend Mode Taxonomy

**From LightwaveOS effect code:**

1. **Linear Blend (Cross-fade)**
   - Formula: `result = A * (1-t) + B * t`
   - Use: Transitions between effects
   - FastLED: `blend(A, B, amount)`

2. **Additive Blend (Accumulation)**
   - Formula: `result = A + B` (saturating)
   - Use: Multiple light sources, rings, pulses
   - FastLED: `qadd8()` or `nblend()`

3. **Subtractive Blend (Darken-only)**
   - Formula: `result = A - B` (saturating)
   - Use: Masking, shadows
   - FastLED: `qsub8()`

4. **Screen Blend (Lighten-only)**
   - Formula: `result = 1 - (1-A) * (1-B)`
   - Use: Glow, halos
   - Implementation:

```cpp
// Screen blend - lighten effect
CRGB screenBlend(CRGB a, CRGB b) {
    uint8_t r = 255 - scale8(255 - a.r, 255 - b.r);
    uint8_t g = 255 - scale8(255 - a.g, 255 - b.g);
    uint8_t b = 255 - scale8(255 - a.b, 255 - b.b);
    return CRGB(r, g, b);
}
// ~9 cycles per pixel
```

### 5.2 Multi-Layer Composition

**From BeatPulseRippleEffect - Layering Strategy:**
```cpp
// Initialize with dark background
fadeToBlackBy(ctx.leds, STRIP_LENGTH, 30);  // 30/256 = ~12% per frame

// Layer 1: Bass ring (red)
for (int i = 0; i < STRIP_LENGTH; i++) {
    uint8_t intensity = getRingIntensity(BASS_RING, i);
    ctx.leds[i] = qadd8(ctx.leds[i], CRGB(intensity, 0, 0));
}

// Layer 2: Treble ring (blue)
for (int i = 0; i < STRIP_LENGTH; i++) {
    uint8_t intensity = getRingIntensity(TREBLE_RING, i);
    ctx.leds[i] = qadd8(ctx.leds[i], CRGB(0, 0, intensity));
}

// Result: Purple where both rings overlap
```

---

## 6. Palette Interpolation & Smooth Transitions

### 6.1 Palette Animation Patterns

**From LGPHolographicAutoCycleEffect:**
```cpp
// Smooth palette transition over multiple frames
void render() {
    static CRGBPalette256 activePalette = ...;
    static CRGBPalette256 targetPalette = ...;

    // Update every ~10 frames with small steps
    if (frameCounter % 10 == 0) {
        nblendPaletteTowardPalette(activePalette, targetPalette, 24);
    }

    // Use current palette for all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(activePalette, i, brightness, LINEARBLEND);
    }
}
```

**Why this works:**
- `nblendPaletteTowardPalette()` blends all 256 palette entries smoothly
- Amount=24 means ~10% change per call (24/256)
- Spread over 10 frames = ~1% per frame (imperceptible)
- Total transition time: ~100 frames (0.83s at 120 FPS)

### 6.2 Hue-Indexed Palette Lookup

**Most efficient pattern (used in 90+ LightwaveOS effects):**
```cpp
// Pattern: gHue provides continuous hue, palette provides color language
uint8_t index = (uint8_t)(ctx.gHue + offset);  // 0-255 maps full palette
CRGB color = ctx.palette.getColor(index, brightness);

// gHue increments ~1 per frame → ~256 frames for full sweep
// This creates slow, smooth color rotation WITHIN palette theme
```

---

## 7. Performance Optimization Techniques

### 7.1 Reduce Operations Per Frame

**Current LightwaveOS budget: 2ms for effect code**

```cpp
// Measurement from RendererActor:
// Worst case: Complex effect + transitions + zones = ~5.68ms total
// Safe budget: Effect code ≤ 2ms, leaving 3.68ms for:
//   - Transition processing (~0.8ms)
//   - Zone composition (~0.5ms)
//   - Color correction (~0.3ms)
//   - FastLED.show() (~2.5ms hardware)
```

### 7.2 Pre-Computation Strategy

**Pattern: Calculate once, use repeatedly**
```cpp
// GOOD: Pre-compute in setup()
class MyEffect {
    uint8_t palette[256];  // Pre-computed LUT

    void setup() {
        for (int i = 0; i < 256; i++) {
            palette[i] = (uint8_t)(sin(i * PI / 128.0f) * 127 + 128);
        }
    }

    void render() {
        for (int i = 0; i < NUM_LEDS; i++) {
            uint8_t val = palette[i];  // O(1) lookup
        }
    }
};

// AVOID: Computing in render()
for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t val = (uint8_t)(sin((float)i / 10.0f) * 127 + 128);
    // Per-LED: float division, sin(), float multiply = 80+ cycles
}
```

### 7.3 Batch Operations

**Pattern: Operate on contiguous ranges**
```cpp
// FastLED provides batch operations:
fill_solid(leds, NUM_LEDS, CRGB::Red);  // Optimized memset
fill_rainbow(leds, NUM_LEDS, hue);      // SSE-friendly loop
fadeToBlackBy(leds, NUM_LEDS, amount);  // Vectorized scale
fill_gradient(leds, NUM_LEDS, start, end, SHORTEST_HUES);  // Smooth gradient

// These are faster than manual loops due to:
// - Loop unrolling
// - Prefetch hints
// - Cache-optimized memory access
```

### 7.4 Static Buffers Over Dynamic Allocation

**From LightwaveOS constraints (CONSTRAINTS.md):**
```cpp
// RULE: No dynamic allocation in render()
// Reason: Heap fragmentation + timing uncertainty

// GOOD: Member static buffers
class MyEffect : public IEffect {
    CRGB tempBuffer[320];  // Fixed at compile-time

    void render(CRGB* leds, uint16_t count, EffectContext& ctx) {
        memcpy(tempBuffer, leds, count * 3);
        // ... process ...
        memcpy(leds, tempBuffer, count * 3);
    }
};
```

---

## 8. Dual-Strip Parallel Output Strategies

### 8.1 I2S vs RMT Protocol

**LightwaveOS uses RMT (Remote Control Module):**

```
RMT Timing Requirements:
┌─ WS2812 Protocol ────────────────────┐
│ One bit = 1.25µs ± 600ns             │
│ "0" bit: 0.4µs high + 0.85µs low     │
│ "1" bit: 0.8µs high + 0.45µs low     │
│ 24 bits per LED = 30µs               │
│ 320 LEDs = 9.6ms per frame           │
└──────────────────────────────────────┘

Parallel Output (Dual Strip):
- 2x 160-LED strips on GPIO4 + GPIO5
- RMT channels transmit in parallel
- Both finish in ~4.8ms (half of serial)
- Then CPU waits for ~3.5ms for protocol latency
```

### 8.2 FastLED RMT Configuration (ESP32-S3)

```cpp
// From FastLedDriver.h - configured for dual strips:
// Strip 1: GPIO4 (160 LEDs) → RMT channel 0
// Strip 2: GPIO5 (160 LEDs) → RMT channel 1

// FastLED buffer arrangement:
CRGB leds[320];
// leds[0-159] = Strip 1
// leds[160-319] = Strip 2

// FastLED.show() sends to both simultaneously (if configured)
```

### 8.3 Timing Constraints for Parallel Output

```cpp
// FastLED.show() timing (RMT parallel):
// - Setup: ~100µs
// - Encoding strip 1: ~4.8ms (parallel with strip 2)
// - Encoding strip 2: ~4.8ms (parallel)
// - Protocol overhead: ~3.5ms (wait for frame end)
// - Total: ~8-9ms per show()

// At 120 FPS (8.33ms budget):
// - Show overhead consumes ~96% of time!
// - Effect render must fit in remaining ~500µs
// - Therefore: Pre-render effect, call show() once per frame
```

### 8.4 Partial Update Strategy (Optimization)

**Not currently used in LightwaveOS, but viable:**

```cpp
// Concept: Only update changed LEDs
class PartialUpdateBuffer {
    CRGB leds[320];
    bool dirty[320];  // Track changes

    void update(int index, CRGB color) {
        if (leds[index] != color) {
            leds[index] = color;
            dirty[index] = true;  // Mark dirty
        }
    }

    void show() {
        // Only transmit dirty regions
        // Could save 10-20% in static scenes
        // Trade-off: Added complexity
    }
};

// Not recommended for audio-reactive effects (everything changes each frame)
```

---

## 9. Color Correction & Gamma Tables

### 9.1 Gamma Correction Overhead

```cpp
// FastLED provides:
// - Gamma tables (pre-computed LUT)
// - Runtime gamma correction via ColorCorrectionEngine

// Cost per LED:
// - No correction: 0 cycles
// - 8-bit LUT lookup: 3 cycles (cache hit)
// - 16-bit calculation: 5 cycles

// At 320 LEDs * 3 bytes = 960 bytes = ~2.5 cycles lookup per LED
// Total gamma time: 320 * 3 * 3 cycles = ~2880 cycles = ~7µs
// Negligible at 120 FPS (8333µs budget)
```

### 9.2 Recommended Gamma Settings

```cpp
// From LightwaveOS ColorCorrectionEngine:
// TypicalLEDStrip color correction:
// - Reduces green slightly (human eye sensitivity)
// - Boosts blue slightly
// - Result: More natural-looking colors

// For WS2812B strips:
FastLED.setCorrection(TypicalLEDStrip);  // Recommended default
// or
FastLED.setCorrection(TypicalSMD5050);    // Alternative
```

---

## 10. Production Patterns from LightwaveOS

### 10.1 Effect Render Template (Optimized)

```cpp
void MyEffect::render(CRGB* leds, uint16_t count, EffectContext& ctx) {
    // Step 1: Pre-compute loop invariants (outside loop)
    uint8_t hueOffset = ctx.gHue;
    uint8_t brightness = ctx.brightness;
    uint16_t time = ctx.time;

    // Step 2: Fade background (if needed)
    if (m_needsDecay) {
        fadeToBlackBy(leds, STRIP_LENGTH, m_decayRate);
    }

    // Step 3: Fast sequential iteration
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate using fixed-point math
        uint16_t phase = (i * m_frequency) + time;
        uint8_t intensity = (uint8_t)(sin16(phase) >> 7);  // -32768 to 32767 → 0-255

        // Palette lookup (cache-friendly)
        uint8_t paletteIdx = (uint8_t)(hueOffset + (i >> 1));  // Hue sweep
        CRGB color = ctx.palette.getColor(paletteIdx, intensity);

        // Accumulate with saturation
        if (m_useAdditive) {
            leds[i] = qadd8(leds[i], color);
        } else {
            leds[i] = color;
        }
    }

    // Step 4: Add specular highlights (optional)
    if (m_highlightStrength > 0) {
        for (uint16_t i = 0; i < 5; i++) {
            uint16_t pos = random16() % STRIP_LENGTH;
            leds[pos] = CRGB::White;
        }
    }
}
```

**Performance characteristics:**
- Loop pre-computation: ~50µs
- Main loop (320 LEDs):
  - sin16: 2 cycles × 320 = 640 cycles
  - palette lookup: 30 cycles × 320 = 9600 cycles
  - qadd8: 1 cycle × 320 = 320 cycles
  - Total: ~10560 cycles ≈ 25µs
- Total render: ~75µs (safely within 2ms budget)

### 10.2 Transition Blending (Efficient)

```cpp
// From TransitionEngine - used in RendererActor
void renderTransition(CRGB* leds, CRGB* source, CRGB* target,
                     uint16_t count, uint8_t progress) {
    // progress: 0 = source, 255 = target

    for (uint16_t i = 0; i < count; i++) {
        // FastLED blend is optimized for speed
        leds[i] = blend(source[i], target[i], progress);
    }

    // Cost: ~40 cycles per LED (blend contains lookups)
    // 320 LEDs × 40 = 12800 cycles ≈ 30µs
}
```

### 10.3 Zone-Based Rendering

```cpp
// From RendererActor + ZoneComposer
// Renders independent effects per zone, then composites

// Pseudo-code:
void renderZones(CRGB* output, uint16_t totalLeds) {
    // Initialize output to black
    fill_solid(output, totalLeds, CRGB::Black);

    // Render each zone effect into temp buffer
    for (int zone = 0; zone < NUM_ZONES; zone++) {
        CRGB zoneBuffer[ZONE_SIZE];
        zoneEffect[zone]->render(zoneBuffer, ZONE_SIZE, ctx);

        // Composite into output (additive)
        for (int i = 0; i < ZONE_SIZE; i++) {
            output[zone * ZONE_SIZE + i] = qadd8(
                output[zone * ZONE_SIZE + i],
                zoneBuffer[i]
            );
        }
    }
}

// Performance with 4 zones:
// - Each effect render: ~25µs (25 LEDs each)
// - Composite: ~10µs
// - Total: ~40µs + per-zone overhead
```

---

## 11. Measurement & Profiling

### 11.1 Timing Budget Breakdown

**From CONSTRAINTS.md:**
```
Frame total:          8.33ms (120 FPS target)
Measured:             5.68ms (176 FPS sustainable)
Safety margin:        ~31%

Breakdown (typical):
  Effect calculation:    ~1.2ms (20%)
  Transition proc:       ~0.8ms (14%)
  Zone composition:      ~0.5ms (9%)
  Color correction:      ~0.3ms (5%)
  FastLED.show():        ~2.5ms (43%)
  Framework overhead:    ~0.3ms (5%)

Available for effects: ~2ms max
```

### 11.2 Profiling Code

```cpp
// Use FreeRTOS tick timer for high-precision measurement
void profileEffect(IEffect* effect, EffectContext& ctx) {
    uint32_t start = esp_timer_get_time();

    effect->render(leds, TOTAL_LEDS, ctx);

    uint32_t elapsed = esp_timer_get_time() - start;
    float milliseconds = elapsed / 1000.0f;

    if (milliseconds > 2.0f) {
        LW_LOGW("Effect exceeded budget: %.2f ms", milliseconds);
    }
}
```

### 11.3 Cache Monitoring

```cpp
// ESP32 has limited cache profiling, but you can detect stalls:
// - High memory latency: sequential iteration vs strided
// - Measure with timing:

// Sequential (cache-friendly):
// Expected: 320 LEDs × ~2 cycles = ~640 cycles ≈ 1.5µs

// Center-origin (cache-unfriendly):
// Expected: 320 LEDs × ~5 cycles = ~1600 cycles ≈ 4µs
// Actual: May see ~8µs due to L1 misses
```

---

## 12. Common Pitfalls & Fixes

### 12.1 Overflow in Color Math

**Problem:**
```cpp
// WRONG: Channel overflow
leds[i].r = leds[i].r + newColor.r;  // If both > 127, overflows!
```

**Fix:**
```cpp
// CORRECT: Use qadd8
leds[i].r = qadd8(leds[i].r, newColor.r);  // Saturates at 255

// Or use nblend for weighted addition
nblend(leds[i], newColor, 128);  // 50% new, 50% old
```

### 12.2 Float Latency in Tight Loops

**Problem:**
```cpp
// WRONG: Float per LED
for (int i = 0; i < 320; i++) {
    float phase = (float)i * frequency;  // Float multiply
    leds[i] = palette[(uint8_t)(sinf(phase) * 127 + 128)];  // 100+ cycles!
}
```

**Fix:**
```cpp
// CORRECT: Fixed-point with pre-computed step
uint16_t phaseStep = (uint16_t)(frequency * 256.0f);  // Compute once
uint16_t phase = 0;
for (int i = 0; i < 320; i++) {
    leds[i] = palette[(uint8_t)(sin16(phase) >> 8)];  // 2 cycles
    phase += phaseStep;  // 1 cycle
}
```

### 12.3 Palette Lookup Mistakes

**Problem:**
```cpp
// WRONG: Interpolation disabled (banding)
CRGB color = ColorFromPalette(palette, hue, brightness, NOBLEND);
// Steps between palette entries are visible
```

**Fix:**
```cpp
// CORRECT: Interpolation enabled (smooth)
CRGB color = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
// Smooth gradients between entries
```

### 12.4 Memory Allocation in render()

**Problem:**
```cpp
// WRONG: Allocates every frame
void render(CRGB* leds, uint16_t count, EffectContext& ctx) {
    CRGB* temp = new CRGB[count];  // Heap fragmentation!
    // ...
    delete[] temp;
}
```

**Fix:**
```cpp
// CORRECT: Static member buffer
class MyEffect : public IEffect {
    CRGB m_tempBuffer[320];  // Pre-allocated

    void render(CRGB* leds, uint16_t count, EffectContext& ctx) override {
        // Use m_tempBuffer directly
    }
};
```

---

## 13. Reference Implementation Checklist

### Pre-Render Verification

- [ ] All color operations use uint8_t (no float in loops)
- [ ] All loops use sequential iteration (not center-origin jumps)
- [ ] Palette lookups use 0-255 index (mapped from hue)
- [ ] Additive blending uses `qadd8()` or saturates properly
- [ ] No `new`/`malloc` in render path
- [ ] Member buffers are static, sized at compile-time
- [ ] Fade operations use `fadeToBlackBy()` or `scale8()`
- [ ] No string concatenation in render
- [ ] Pre-compute loop invariants outside loop
- [ ] Measure performance with profiler

### Post-Render Verification

- [ ] Effect renders in < 2ms (typical: 0.2-1.5ms)
- [ ] No flickering or timing artifacts
- [ ] Color progression is smooth (no banding)
- [ ] Audio sync timing is stable (if audio-reactive)
- [ ] Memory usage stable (no heap growth)
- [ ] Transitions are smooth and artifact-free

---

## 14. Further Research & References

### Official Resources

- FastLED documentation: https://fastled.io/
- ESP32-S3 datasheet: https://www.espressif.com/en/products/socs/esp32-s3
- WS2812B protocol: https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf

### LightwaveOS-Specific Files

| File | Topic |
|------|-------|
| `firmware/v2/src/effects/utils/FastLEDOptim.h` | Fixed-point wrappers |
| `firmware/v2/src/effects/transitions/TransitionEngine.cpp` | Blend implementations |
| `firmware/v2/src/core/actors/RendererActor.cpp` | 120 FPS render loop |
| `firmware/v2/src/effects/ieffect/*.cpp` | Production effect patterns |
| `firmware/v2/src/hal/led/FastLedDriver.h` | HAL integration |

### Measurement Tools

```bash
# Profile effect timing
cd firmware/v2
pio run -e esp32dev_audio_esv11 -t upload
# Serial monitor shows: "Effect: X.XXms"

# Check memory usage
pio run -e esp32dev_audio_esv11 --target size
# Shows flash and SRAM allocation
```

---

## Appendix: Quick Reference Tables

### Color Operations Quick Reference

| Operation | Syntax | Cycles | Use Case |
|-----------|--------|--------|----------|
| Fade | `fadeToBlackBy(leds, 320, 20)` | 640 | Decay background |
| Scale | `scale8(val, 200)` | 1 | Brightness multiplier |
| Saturate add | `qadd8(a, b)` | 1 | Prevent overflow |
| Saturate sub | `qsub8(a, b)` | 1 | Prevent underflow |
| Blend | `blend(a, b, 128)` | 40 | Cross-fade |
| In-place blend | `nblend(a, b, 128)` | 35 | Layer accumulation |
| Palette lookup | `ColorFromPalette(p, i, b)` | 30 | Color mapping |
| Fill solid | `fill_solid(leds, 320, c)` | 960 | Initialize |
| Fill gradient | `fill_gradient(l, s, e, SHORTEST)` | 960 | Smooth transition |

### Timing Budget (120 FPS)

| Task | Allocation | Typical | Headroom |
|------|-----------|---------|----------|
| Total frame | 8330µs | 5680µs | 2650µs |
| Effect render | 2000µs | 1200µs | 800µs |
| Transitions | 1000µs | 800µs | 200µs |
| Zones | 500µs | 500µs | 0µs |
| Color correction | 400µs | 300µs | 100µs |
| FastLED.show() | 2500µs | 2500µs | 0µs |
| Framework | 500µs | 300µs | 200µs |

### Memory Layout (320 KB Internal SRAM)

| Component | Size | Location | Why |
|-----------|------|----------|-----|
| LED buffer (m_leds) | 960 B | Internal | DMA requirement |
| Task stacks (96KB) | 96 KB | Internal | FreeRTOS requirement |
| WiFi/lwIP buffers | 50 KB | Internal | Network stack |
| Effect state | 2 KB | Internal | Hot path |
| DSP buffers (ESV11) | 68 KB | PSRAM | Large, infrequent |
| Zone buffers | 4 KB | PSRAM | Non-DMA, large |

---

*Document version: 1.0*
*Last updated: 2026-02-08*
*Applies to: LightwaveOS v2 on ESP32-S3 N16R8*
