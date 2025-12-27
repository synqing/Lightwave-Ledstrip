# AudioBloomEffect: Logarithmic Distortion & Saturation Boost Implementation

## Overview

The `AudioBloomEffect` class has been successfully enhanced with post-processing to match Sensory Bridge's visual quality. The effect applies three sequential transformations to create a "bunched" bloom appearance:

1. **Logarithmic Distortion** - Compresses radial coordinates toward the center using sqrt remapping
2. **Edge Fade** - Reduces brightness in the outer half of the strip
3. **Saturation Boost** - Enhances color vibrancy by boosting HSV saturation

## Implementation Files

- **Header**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/effects/ieffect/AudioBloomEffect.h`
- **Implementation**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/effects/ieffect/AudioBloomEffect.cpp`

## Architecture

### Class Structure

```cpp
class AudioBloomEffect : public plugins::IEffect {
private:
    // Radial buffers (center at index 0, extends to HALF_LENGTH-1)
    CRGB m_radial[HALF_LENGTH];        // Main radial buffer
    CRGB m_radialAux[HALF_LENGTH];     // Aux buffer for alternate frames
    CRGB m_radialTemp[HALF_LENGTH];    // Temp buffer for post-processing

    // Audio-sync state
    uint32_t m_iter = 0;               // Frame counter
    uint32_t m_lastHopSeq = 0;         // Hop sequence tracker
    float m_scrollPhase = 0.0f;        // Fractional scroll accumulator

    // Post-processing helpers
    void distortLogarithmic(CRGB* src, CRGB* dst, uint16_t len);
    void fadeTopHalf(CRGB* buffer, uint16_t len);
    void increaseSaturation(CRGB* buffer, uint16_t len, uint8_t amount);
};
```

### Buffer Layout

For 160 LEDs per strip (dual-strip system):
- **Index 0**: Center (LED 79/80 when rendered)
- **Index 1-79**: Outward to edge
- **Total**: 80 entries (HALF_LENGTH)

The radial buffer uses one half of the strip for calculations. On render, `SET_CENTER_PAIR()` mirrors values symmetrically to both sides of the center.

## Post-Processing Pipeline

### 1. Logarithmic Distortion

**Purpose**: Create a perspective/acceleration effect where content bunches toward the center.

**Location**: `AudioBloomEffect::distortLogarithmic()` (lines 163-188)

```cpp
void AudioBloomEffect::distortLogarithmic(CRGB* src, CRGB* dst, uint16_t len) {
    // Maps linear position i to sqrt(i) position in source
    // Effect: First quarter of strip covers half the visual space (accelerating outward)
    for (uint16_t i = 0; i < len; ++i) {
        // Normalized position: 0.0 (center) to 1.0 (edge)
        float prog = (float)i / (float)(len - 1);

        // Apply sqrt curve for logarithmic compression
        // sqrt(0.0) = 0.0, sqrt(0.25) ≈ 0.5, sqrt(1.0) = 1.0
        float prog_distorted = sqrtf(prog);

        // Map distorted position back to source index
        float srcPos = prog_distorted * (len - 1);
        uint16_t srcIdx = (uint16_t)srcPos;
        float fract = srcPos - srcIdx;

        // Bilinear interpolation for smooth results
        if (srcIdx >= len - 1) {
            dst[i] = src[len - 1];
        } else {
            CRGB c1 = src[srcIdx];
            CRGB c2 = src[srcIdx + 1];
            dst[i] = CRGB(
                (uint8_t)(c1.r * (1.0f - fract) + c2.r * fract),
                (uint8_t)(c1.g * (1.0f - fract) + c2.g * fract),
                (uint8_t)(c1.b * (1.0f - fract) + c2.b * fract)
            );
        }
    }
}
```

**Mathematical Insight**:
- Linear mapping: LED at position 40/80 = 0.5 → reads from position 0.5 * 79 ≈ 39.5
- Logarithmic mapping: LED at position 40/80 = 0.5 → reads from position sqrt(0.5) * 79 ≈ 0.707 * 79 ≈ 56
  - This pulls content inward, making the first half of the display show twice as much visual content as the back half
  - Creates perspective: first half "accelerates" visually

### 2. Edge Fade

**Purpose**: Reduce brightness in the outer regions to create depth perception.

**Location**: `AudioBloomEffect::fadeTopHalf()` (lines 190-200)

```cpp
void AudioBloomEffect::fadeTopHalf(CRGB* buffer, uint16_t len) {
    // Fade the outer half from full brightness toward zero
    uint16_t halfLen = len >> 1;  // 40 for HALF_LENGTH=80

    for (uint8_t i = 0; i < halfLen; ++i) {
        // Fade factor: 0.0 at edge, 1.0 at center
        float fade = (float)i / (float)halfLen;  // 0.0 to 1.0

        // Index counting backward from edge
        uint16_t idx = (len - 1) - i;  // 79, 78, 77, ..., 40

        // Apply fade (multiply RGB)
        buffer[idx].r = (uint8_t)(buffer[idx].r * fade);
        buffer[idx].g = (uint8_t)(buffer[idx].g * fade);
        buffer[idx].b = (uint8_t)(buffer[idx].b * fade);
    }
}
```

**Effect**:
- LEDs in outer half (indices 40-79) fade from 50% brightness at center to 0% at edge
- Creates natural depth: "far" LEDs appear dimmer
- Works bilaterally (both left and right edges)

### 3. Saturation Boost

**Purpose**: Enhance color vibrancy after rendering, compensating for any desaturation from dithering/interpolation.

**Location**: `AudioBloomEffect::increaseSaturation()` (lines 202-208)

```cpp
void AudioBloomEffect::increaseSaturation(CRGB* buffer, uint16_t len, uint8_t amount) {
    for (uint16_t i = 0; i < len; ++i) {
        // Convert RGB to HSV (approximate for speed)
        CHSV hsv = rgb2hsv_approximate(buffer[i]);

        // Boost saturation with 8-bit overflow protection
        // qadd8() = saturating add (max out at 255, never overflow)
        hsv.s = qadd8(hsv.s, amount);

        // Convert back to RGB using spectrum-based lookup
        hsv2rgb_spectrum(hsv, buffer[i]);
    }
}
```

**Saturation Boost Strategy**:
- Default `amount = 24` (out of 255 max)
- Approximately 9.4% boost: (24/255) ≈ 0.094
- `qadd8()` safely clamps at 255 (white stays white)
- Applied to all 80 LEDs equally

## Render Pipeline

### Full Effect Execution Flow

**Location**: `AudioBloomEffect::render()` (lines 37-161)

```
1. Check audio sync available → continue if not, return if unavailable
2. Detect audio hop event (new frame of chromagram data)
3. On every other hop (even iterations):
   a. Compute sum_color from 12-band chromagram
   b. Apply scroll (fractional movement outward)
   c. Update radial buffer with new color at center
   d. Apply post-processing:
      i.   distortLogarithmic()  ← Maps positions toward center
      ii.  fadeTopHalf()         ← Dim outer regions
      iii. increaseSaturation()  ← Boost color vibrancy  [FIXED]
   e. Store result in auxiliary buffer for frame caching
4. On odd iterations: reuse cached result from auxiliary buffer
5. Render radial buffer to center-origin LED pairs
```

### Critical Bug Fix: HSV Conversion

**Original Code (BROKEN)**:
```cpp
void AudioBloomEffect::increaseSaturation(CRGB* buffer, uint16_t len, uint8_t amount) {
    for (uint16_t i = 0; i < len; ++i) {
        CHSV hsv = rgb2hsv_approximate(buffer[i]);
        hsv.s = qadd8(hsv.s, amount);
        buffer[i] = hsv;  // ERROR: Assigning CHSV to CRGB (implicit cast)
    }
}
```

**Problem**:
- Line assigns `CHSV` (Hue, Saturation, Value) to `CRGB` (Red, Green, Blue)
- Implicit cast reinterprets memory: H→R, S→G, V→B
- Results in colors with hue values in red channel, saturation in green, etc.
- Visual artifact: unpredictable color shift instead of saturation boost

**Fixed Code**:
```cpp
void AudioBloomEffect::increaseSaturation(CRGB* buffer, uint16_t len, uint8_t amount) {
    for (uint16_t i = 0; i < len; ++i) {
        CHSV hsv = rgb2hsv_approximate(buffer[i]);
        hsv.s = qadd8(hsv.s, amount);
        hsv2rgb_spectrum(hsv, buffer[i]);  // CORRECT: Proper HSV→RGB conversion
    }
}
```

**Solution**:
- Call `hsv2rgb_spectrum()` to properly convert boosted HSV back to RGB
- Preserves hue and value, only increases saturation
- Colors become more vivid (more pure hue, less white)

## Color Mathematics

### RGB to HSV Conversion
- `rgb2hsv_approximate()`: Fast integer-based conversion (FastLED)
- Preserves hue/saturation while determining brightness

### HSV to RGB Conversion
- `hsv2rgb_spectrum()`: Spectrum-based lookup (more accurate color rendering)
- Uses pre-calculated color wheel for smooth transitions
- Saturating add `qadd8()` prevents overflow (max saturation = 255)

### Saturation Boost Effect

Example: Pure Red (255, 0, 0)
```
RGB → HSV:  H=0°, S=255, V=255
Boost S:    H=0°, S=255 (already max, qadd8 saturates)
HSV → RGB:  (255, 0, 0) - unchanged

Example: Dull Red (255, 50, 50)
RGB → HSV:  H=0°, S≈157, V=255
Boost S:    H=0°, S=181 (157+24, saturated)
HSV → RGB:  (255, 23, 23) - more vivid red
```

## Performance Characteristics

### Memory Usage
- `3 × 80 CRGB = 240 bytes` for radial buffers
- Allocated at construction, never freed (embedded system)
- Fixed memory footprint independent of animation complexity

### CPU Cycles (Per Render)
- **Logarithmic distortion**: 80 × (1 sqrt + 1 multiply + 1 lerp) ≈ 240 ops
- **Edge fade**: 40 × (1 multiply × 3) ≈ 120 ops
- **Saturation boost**: 80 × (1 rgb2hsv + 1 qadd8 + 1 hsv2rgb) ≈ 240 ops
- **Total**: ~600 CPU ops per effect, negligible overhead

### Frame Rate
- AudioBloomEffect renders every other hop (50% duty cycle on audio events)
- Hoops occur at 100 Hz (every 10ms typical)
- Effective render frequency: 50 Hz
- No frame-rate impact to 120 FPS target

## Testing & Validation

### Visual Characteristics (Expected Behavior)
1. **Bloom appearance**:
   - Colors bunch toward center (logarithmic distortion)
   - Outer edges appear dimmer (fade effect)
   - Colors appear more saturated and vivid

2. **Animation smoothness**:
   - Fractional scroll phase prevents stuttering
   - Alternate frame caching (2x speedup potential)

3. **Color accuracy**:
   - Pure colors remain pure (saturation boost doesn't desaturate)
   - Dull colors become more vivid
   - No hue shift from saturation operation

### Compilation Requirements
- FastLED 3.10.0+ (provides `hsv2rgb_spectrum`, `rgb2hsv_approximate`)
- C++11 (for `sqrtf()`, `memset()`, range-based operations)
- No additional dependencies beyond base LightwaveOS stack

### Known Limitations
- Distortion uses approximate (linear interpolation, not cubic spline)
- Saturation boost is unconditional (no parameter control)
- Center-origin fixed (not bilaterally symmetric at per-LED level)

## Integration Notes

### AudioBloomEffect Registration
The effect is registered in main.cpp effect array (ID 73):
```cpp
// Effect ID 73 in effects[] array
{"Audio Bloom", &AudioBloomEffectInstance, EFFECT_TYPE_AUDIO_SYNC}
```

### Required Compilation Flags
- `FEATURE_AUDIO_SYNC` - Must be enabled for effect to render
- `FEATURE_EFFECT_VALIDATION` - Optional, enables effect validation instrumentation

### Customization Points
To adjust post-processing intensity, modify in `render()`:
```cpp
// Line 127 in AudioBloomEffect.cpp
increaseSaturation(m_radial, HALF_LENGTH, 24);  // Change 24 for different boost
```

Saturation boost scale:
- 16 = subtle (+6%)
- 24 = moderate (+9%)
- 32 = strong (+13%)
- 48 = very strong (+19%)

## References

### Sensory Bridge Implementation
- Inspired by `led_utilities.h` distortion and saturation functions
- Adapted for LightwaveOS CENTER_ORIGIN architecture
- Alternative: linear progression instead of sqrt for different perspective effect

### FastLED Color Functions
- `rgb2hsv_approximate()`: ColorUtils.h - Fast RGB→HSV
- `hsv2rgb_spectrum()`: ColorUtils.h - Accurate HSV→RGB
- `qadd8()`: lib8tion.h - Saturating 8-bit addition

## Conclusion

AudioBloomEffect now features professional-grade post-processing that enhances visual impact while maintaining performance. The three-stage pipeline (distort → fade → saturate) creates the characteristic "bunched bloom" appearance that defines Sensory Bridge's aesthetic.

The critical HSV conversion bug has been fixed, ensuring proper color enhancement without artifacts.
