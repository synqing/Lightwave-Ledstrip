# FastLED Color Blending & Palette Design Guide

Practical color theory and blending strategies for LightwaveOS visual effects.

---

## 1. Fundamental Blend Modes

### Linear Blend (Standard Cross-fade)

**Formula:** `result = A × (1 - t) + B × t` where t ∈ [0, 1]

```cpp
// FastLED implementation
CRGB result = blend(colorA, colorB, amount);
// amount: 0 = pure A, 128 = 50/50 mix, 255 = pure B

// Example: Fade from red to blue
CRGB red = CRGB(255, 0, 0);
CRGB blue = CRGB(0, 0, 255);
for (uint8_t t = 0; t <= 255; t++) {
    CRGB mixed = blend(red, blue, t);
    // t=0: pure red, t=128: purple, t=255: pure blue
}
```

**Use cases:**
- Effect transitions
- Palette animations
- Cross-fading between colors
- Smooth gradients

**Performance:** ~40 cycles per blend call (can do 7,500/ms on ESP32)

---

### Additive Blend (Light Accumulation)

**Formula:** `result = A + B` (saturating at 255)

```cpp
// Method 1: qadd8 (explicit saturation)
leds[i] = qadd8(baseColor, addedLight);
// If baseColor=200 + addedLight=100 → result=255 (saturated)

// Method 2: nblend (in-place accumulation)
nblend(leds[i], newSource, 180);  // ~70% new, ~30% existing

// Method 3: Direct accumulation (when you know it won't overflow)
leds[i] += newColor;  // WARNING: Can wrap on overflow!
```

**Physical interpretation:**
- Two light sources shining through translucent material
- Brightness accumulates, max 255 (full brightness)
- Colors mix additively (red + green = yellow, not brown)

**Use cases:**
- Multiple rings/pulses overlapping
- Glow/halo effects
- Light layer composition
- Audio-reactive stacking

**Performance:** qadd8 = 1 cycle per operation, nblend = ~35 cycles

---

### Subtractive Blend (Shadow/Mask)

**Formula:** `result = A - B` (saturating at 0)

```cpp
// Explicit saturating subtraction
uint8_t darker = qsub8(brightness, fadeAmount);

// Usage: Create darkening effect
for (int i = 0; i < 320; i++) {
    uint8_t shadow = (i % 20 < 10) ? 80 : 0;  // Half shadow
    leds[i] = CRGB(qsub8(leds[i].r, shadow),
                    qsub8(leds[i].g, shadow),
                    qsub8(leds[i].b, shadow));
}
```

**Use cases:**
- Shadows/darkening
- Masking effects
- Chiaroscuro (light/dark contrast)

---

### Screen Blend (Lighten-only, Photographic)

**Formula:** `result = 1 - (1 - A) × (1 - B)`

```cpp
// Photographic "screen" blending
CRGB screenBlend(CRGB a, CRGB b) {
    return CRGB(
        255 - scale8(255 - a.r, 255 - b.r),
        255 - scale8(255 - a.g, 255 - b.g),
        255 - scale8(255 - a.b, 255 - b.b)
    );
}

// Usage: Glow over existing colors (lightens, never darkens)
CRGB glowColor = CRGB(255, 200, 100);  // Warm glow
for (int i = 0; i < 320; i++) {
    leds[i] = screenBlend(leds[i], glowColor);  // Always brighter
}
```

**Properties:**
- Never darkens (opposite of multiply)
- Overlapping areas get brighter
- Used for light halos, glows, highlights

**Performance:** ~9 cycles per pixel

---

### Multiply Blend (Darken-only)

**Formula:** `result = (A × B) / 255`

```cpp
// Multiply blending (inverse of screen)
CRGB multiplyBlend(CRGB a, CRGB b) {
    return CRGB(
        scale8(a.r, b.r),
        scale8(a.g, b.g),
        scale8(a.b, b.b)
    );
}

// Usage: Shadow/tint layer (always darkens)
CRGB shadowColor = CRGB(0, 0, 100);  // Blue shadow
for (int i = 0; i < 320; i++) {
    leds[i] = multiplyBlend(leds[i], shadowColor);  // Always darker
}
```

**Properties:**
- Never lightens
- Black input always produces black
- White input preserves original
- Used for color tinting, shadows

---

### Overlay Blend (Combine Multiply + Screen)

```cpp
// Overlay: multiply if base < 128, screen if base > 128
CRGB overlayBlend(CRGB base, CRGB blend) {
    CRGB result;
    for (int c = 0; c < 3; c++) {
        uint8_t* basePtr = (c == 0) ? &base.r : (c == 1) ? &base.g : &base.b;
        uint8_t* blendPtr = (c == 0) ? &blend.r : (c == 1) ? &blend.g : &blend.b;
        uint8_t* resultPtr = (c == 0) ? &result.r : (c == 1) ? &result.g : &result.b;

        if (*basePtr < 128) {
            *resultPtr = scale8(*basePtr * 2, *blendPtr);
        } else {
            *resultPtr = 255 - scale8((255 - *basePtr) * 2, 255 - *blendPtr);
        }
    }
    return result;
}

// Usage: Enhanced contrast blending
// Bright areas get brighter, dark areas darker
```

**Performance:** ~25 cycles per pixel (expensive)

---

## 2. Palette Design Fundamentals

### Color Harmonies (Theory)

| Harmony | Hue Angles | Effect | Use Case |
|---------|-----------|--------|----------|
| **Monochromatic** | Same hue, vary saturation/value | Cohesive, safe | Professional, minimal |
| **Analogous** | 30° apart | Harmonious, flowing | Organic, soothing |
| **Complementary** | 180° apart | High contrast, vibrant | Energetic, pop |
| **Triadic** | 120° apart | Balanced, lively | Dynamic, balanced |
| **Tetradic** | 90° apart | Complex, bold | Advanced, intricate |
| **Split-complementary** | 150° apart each side | Vibrant but safe | Modern, contemporary |

### Converting Hue to FastLED

```cpp
// FastLED uses 0-255 for hue (maps to 0-360 degrees)
// 0 = Red, 85 = Green, 170 = Blue, 255 = Red

uint8_t hue_0_to_360_degrees = 180;  // 180 degrees
uint8_t fastled_hue = (uint8_t)((hue_0_to_360_degrees * 255) / 360);  // ~127

// Or use predefined constants
#define RED        0
#define ORANGE    21
#define YELLOW    43
#define GREEN     85
#define CYAN     127
#define BLUE     170
#define MAGENTA  212
```

### Basic Palette Template

```cpp
PROGMEM CRGB palette_analogous[16] = {
    // Analogous harmony: greens → cyans → blues
    CRGB(0, 255, 0),     // Pure green
    CRGB(0, 240, 20),    // Green → cyan
    CRGB(0, 200, 80),
    CRGB(0, 160, 140),
    CRGB(0, 120, 200),
    CRGB(20, 80, 240),   // Cyan → blue
    CRGB(50, 40, 255),
    CRGB(100, 0, 200),
    // Repeat in reverse for full cycle
    CRGB(100, 0, 200),
    CRGB(50, 40, 255),
    CRGB(20, 80, 240),
    CRGB(0, 120, 200),
    CRGB(0, 160, 140),
    CRGB(0, 200, 80),
    CRGB(0, 240, 20),
    CRGB(0, 255, 0),
};

// Usage in effects:
CRGB color = ColorFromPalette(palette_analogous, hueIndex, brightness, LINEARBLEND);
```

---

## 3. Advanced Blending Strategies

### Layer Composition (Multi-Pass Rendering)

```cpp
void renderMultiLayer(CRGB* output, const EffectContext& ctx) {
    // Layer 1: Background pattern
    fill_solid(output, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t bg = sin8(i + ctx.time);
        output[i] = ctx.palette.getColor(ctx.gHue, bg);
    }

    // Layer 2: Rhythmic pulses (additive)
    uint8_t pulse = beatsin8(90, 20, 100);
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t dist = abs(i - 160);
        uint8_t intensity = scale8(255 - dist, pulse);
        CRGB pulse_color = CRGB(intensity >> 1, intensity, 255);
        output[i] = qadd8(output[i], pulse_color);  // Additive
    }

    // Layer 3: Specular highlights (brightest, replace mode)
    static uint8_t hl_pos = 0;
    output[hl_pos] = CRGB::White;
    if (hl_pos > 0) output[hl_pos - 1] = CRGB(200, 200, 200);
    hl_pos = (hl_pos + 1) % NUM_LEDS;
}
```

**Order matters:**
1. Background (blend or replace)
2. Mid-layers (additive accumulation)
3. Highlights (replaces or saturates)

### Temporal Dithering for Smooth Fades

```cpp
// Problem: 8-bit channels can show banding in gradients
// Solution: Temporal dithering (randomize across frames)

void render_with_dither(CRGB* leds, const EffectContext& ctx) {
    static uint8_t dither_frame = 0;
    dither_frame++;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t base_brightness = sin8(i + ctx.time);

        // Add time-varying dither (different per frame)
        uint8_t dither = (dither_frame ^ i) & 0x1F;  // 5-bit dither
        uint8_t brightness = qadd8(base_brightness, dither);

        // Render with dithered brightness
        uint8_t hue = (uint8_t)(ctx.gHue + (i >> 1));
        leds[i] = ctx.palette.getColor(hue, brightness);
    }
}

// Result: Smoother gradients, imperceptible flicker
```

### HSV-Based Color Blending (Perceptual)

```cpp
// More natural than RGB blending for colors
CRGB hsvBlend(CRGB rgbA, CRGB rgbB, uint8_t amount) {
    // Convert to HSV
    CHSV hsvA = rgb2hsv_approximate(rgbA);
    CHSV hsvB = rgb2hsv_approximate(rgbB);

    // Blend in HSV space (hue, saturation, value)
    CHSV hsvResult;
    hsvResult.h = hsvA.h + (int16_t)(hsvB.h - hsvA.h) * amount / 255;
    hsvResult.s = hsvA.s + (int16_t)(hsvB.s - hsvA.s) * amount / 255;
    hsvResult.v = hsvA.v + (int16_t)(hsvB.v - hsvA.v) * amount / 255;

    // Convert back to RGB
    return hsv2rgb_spectrum(hsvResult);
}

// Usage: More natural color transitions
for (int i = 0; i < 320; i++) {
    CRGB blended = hsvBlend(color1, color2, transition_progress);
    leds[i] = blended;
}
```

---

## 4. Practical Palette Recipes

### Recipe 1: Warm Sunset Palette

```cpp
PROGMEM CRGB sunset_palette[16] = {
    CRGB(255, 0, 0),      // Deep red
    CRGB(255, 20, 0),
    CRGB(255, 60, 0),
    CRGB(255, 100, 0),    // Orange
    CRGB(255, 140, 0),
    CRGB(255, 180, 0),
    CRGB(255, 200, 50),   // Golden
    CRGB(255, 220, 100),
    CRGB(255, 200, 100),
    CRGB(200, 150, 50),   // Amber
    CRGB(150, 100, 30),
    CRGB(100, 50, 20),
    CRGB(50, 30, 10),     // Dark brown
    CRGB(30, 20, 5),
    CRGB(20, 10, 0),
    CRGB(10, 5, 0),
};

// Usage: Warm, calming effect
// Palette provides color language, gHue provides animation
```

### Recipe 2: Cool Ocean Palette

```cpp
PROGMEM CRGB ocean_palette[16] = {
    CRGB(0, 20, 60),      // Deep navy
    CRGB(0, 40, 100),
    CRGB(0, 80, 150),
    CRGB(0, 120, 200),    // Ocean blue
    CRGB(20, 150, 220),
    CRGB(60, 180, 240),   // Light blue
    CRGB(100, 200, 255),
    CRGB(150, 220, 255),
    CRGB(200, 240, 255),  // Cyan/turquoise
    CRGB(100, 200, 200),
    CRGB(50, 150, 150),
    CRGB(20, 100, 100),
    CRGB(0, 80, 80),      // Teal
    CRGB(0, 60, 60),
    CRGB(0, 40, 40),
    CRGB(0, 20, 20),
};
```

### Recipe 3: Vibrant Rainbow (Saturated)

```cpp
// Using HSV-based palette for pure colors
PROGMEM CRGB rainbow_palette[16] = {
    CRGB::Red,
    CRGB(255, 100, 0),    // Orange
    CRGB::Yellow,
    CRGB(100, 255, 0),    // Yellow-green
    CRGB::Green,
    CRGB(0, 200, 100),    // Teal
    CRGB::Blue,
    CRGB(150, 0, 200),    // Purple
    CRGB::Magenta,
    CRGB(200, 0, 100),    // Deep pink
    CRGB::Red,
    // Can repeat for smoothness
    CRGB(200, 0, 100),
    CRGB::Magenta,
    CRGB(150, 0, 200),
    CRGB::Blue,
    CRGB(0, 200, 100),
};
```

### Recipe 4: Monochromatic (Professional)

```cpp
// Single hue, varying saturation & value
// Great for corporate/professional displays

PROGMEM CRGB monochromatic_blue[16] = {
    CRGB(0, 0, 255),      // Pure blue (max saturation)
    CRGB(20, 40, 200),
    CRGB(40, 80, 180),
    CRGB(80, 120, 160),
    CRGB(120, 150, 140),
    CRGB(150, 170, 130),  // Reduced saturation
    CRGB(170, 185, 120),
    CRGB(180, 195, 115),
    CRGB(190, 205, 110),
    CRGB(200, 215, 105),
    CRGB(210, 225, 100),
    CRGB(220, 235, 95),
    CRGB(230, 240, 90),
    CRGB(235, 245, 80),
    CRGB(240, 250, 70),
    CRGB(245, 255, 60),   // Nearly white
};
```

---

## 5. Palette Animation Techniques

### Hue-Shifting (Rotating Through Palette)

```cpp
// Simplest animation: let gHue increment
// Already handled by RendererActor context

// In effect:
uint8_t color_index = (uint8_t)(ctx.gHue + (position >> 1));
CRGB color = ctx.palette.getColor(color_index, brightness);

// gHue increments ~1 per frame
// At 120 FPS: 256 frames for full sweep = 2.1 seconds
// Smooth, hypnotic color rotation
```

### Palette Morphing (Blending Palettes)

```cpp
class PaletteMorphEffect : public IEffect {
private:
    CRGBPalette256 current_palette;
    CRGBPalette256 target_palette;
    uint16_t morph_timer = 0;

public:
    void render(CRGB* leds, uint16_t count, EffectContext& ctx) override {
        // Blend palettes every 5 frames
        if (morph_timer++ % 5 == 0) {
            nblendPaletteTowardPalette(current_palette, target_palette, 8);
            // Amount=8 means 8/256 = 3% per call
            // Full transition: ~256/8 = 32 calls = 160 frames = 1.3s
        }

        // Render using morphing palette
        for (int i = 0; i < count; i++) {
            uint8_t idx = (uint8_t)(ctx.gHue + (i >> 1));
            uint8_t brightness = sin8((i + ctx.time) >> 8);
            leds[i] = ColorFromPalette(current_palette, idx, brightness, LINEARBLEND);
        }
    }
};
```

### Brightness-Based Coloring

```cpp
// Use brightness/intensity to select from palette
// Creates effect of illumination revealing color

void render(CRGB* leds, uint16_t count, EffectContext& ctx) {
    for (int i = 0; i < count; i++) {
        // Calculate intensity from effect
        uint8_t intensity = sin8((i + ctx.time) >> 8);

        // Use intensity as palette index (maps 0-255 to full palette)
        // Bright areas → bright colors, dark areas → dark colors
        CRGB color = ctx.palette.getColor(intensity, 255);
        leds[i] = color;
    }
}

// Result: Effect appears to "reveal" color through brightness
```

---

## 6. Color Correction & Perception

### Gamma Correction

```cpp
// FastLED provides gamma tables
// Linear vs perceived brightness is non-linear

// Recommended for WS2812B:
FastLED.setCorrection(TypicalLEDStrip);
// Adjusts: Reduces green (~85%), keeps red/blue

// Why: Human eye is more sensitive to green
// Result: More natural-looking colors
```

### Perceived Brightness Scaling

```cpp
// Important: Brightness = perceived brightness
// Not linear! 128 is NOT "half as bright" as 255

// Use FastLED's brightness scaling:
CRGB color = ...;
color.nscale8_video(brightness);  // Proper perceptual scaling

// NOT:
color.r = color.r * brightness / 255;  // Too linear, loses detail

// nscale8_video uses gamma correction internally
```

### Color Psychology

```cpp
// Color choice affects emotional response:

BLUE    // Calm, professional, cool, deep
CYAN    // Fresh, modern, energetic but calm
GREEN   // Natural, growth, healing, relaxing
YELLOW  // Warm, optimistic, energetic
ORANGE  // Fun, warm, creative, playful
RED     // Energy, passion, urgency, intensity
MAGENTA // Mysterious, creative, sophisticated
PURPLE  // Regal, mysterious, contemplative

// Palette selection should match effect goal:
// - Calming effect: Use blues, greens, cool palette
// - Energetic effect: Use reds, oranges, warm palette
// - Professional: Use monochromatic or complementary
```

---

## 7. Troubleshooting Color Issues

### Problem: Colors Look Washed Out

**Causes:**
1. Brightness too low (< 50)
2. Saturation too low (palette uses pastels)
3. Gamma correction disabled

**Fixes:**
```cpp
// Ensure brightness is adequate
uint8_t brightness = scale8(intensity, ctx.brightness);
if (brightness < 50) brightness = qadd8(brightness, 50);  // Boost

// Use saturated palette
CRGB color = ctx.palette.getColor(hue, brightness);  // Not gray

// Enable gamma correction
FastLED.setCorrection(TypicalLEDStrip);
```

### Problem: Banding (Visible Color Steps)

**Causes:**
1. Palette too small (< 256 entries)
2. No interpolation (NOBLEND instead of LINEARBLEND)
3. No dithering in 8-bit gradients

**Fixes:**
```cpp
// Use 256-entry palette (or 16-entry with LINEARBLEND)
CRGBPalette256 smooth_palette = ...;

// Enable interpolation
CRGB color = ColorFromPalette(palette, index, brightness, LINEARBLEND);

// Add temporal dithering for extra smoothness
```

### Problem: Colors Shift When Brightened/Darkened

**Causes:**
1. Using scale8 on RGB channels (changes hue)
2. Brightness applied incorrectly

**Fix:**
```cpp
// Work in HSV space for brightness changes
CHSV hsv = rgb2hsv_approximate(rgb_color);
hsv.v = scale8(hsv.v, brightness);  // Scale value
CRGB result = hsv2rgb_spectrum(hsv);

// Or use nscale8_video which handles this internally
color.nscale8_video(brightness);
```

### Problem: Audio Reaction Colors Clash

**Causes:**
1. Fixed palette doesn't adapt to music
2. Palette too colorful (clashes with random effects)

**Fix:**
```cpp
// Use narrower palette for audio effects
// Stick to analogous colors (30° apart) instead of rainbow

// Or: Adaptive hue offset based on audio frequency
uint8_t freq_hue_offset = audioContext.dominantFreqHue;
uint8_t palette_index = (uint8_t)(baseHue + freq_hue_offset);
CRGB color = ctx.palette.getColor(palette_index, intensity);
```

---

## 8. Advanced: Custom Color Spaces

### HSV (Hue-Saturation-Value)

```cpp
// Convert RGB to HSV for color manipulation
CHSV hsv = rgb2hsv_approximate(rgb_color);
// hsv.h: 0-255 (hue, 0=red, 85=green, 170=blue)
// hsv.s: 0-255 (saturation, 0=gray, 255=pure color)
// hsv.v: 0-255 (value/brightness, 0=black, 255=bright)

// Manipulate in HSV (easier than RGB)
hsv.h = (hsv.h + 128) % 256;  // Shift hue by 180°
hsv.s = qadd8(hsv.s, 50);     // More saturated
hsv.v = scale8(hsv.v, 200);   // Brighter

// Convert back to RGB for output
CRGB result = hsv2rgb_spectrum(hsv);
```

### HLS (Hue-Lightness-Saturation)

```cpp
// Alternative to HSV, sometimes more intuitive
// Lightness: 0=black, 128=pure color, 255=white

// To create lighter version of color:
uint8_t lighter_value = 200;  // Closer to white
// Adjust in HSV: increase V towards 255
```

---

## 9. Quick Reference: FastLED Color Functions

| Function | Use | Performance |
|----------|-----|-------------|
| `ColorFromPalette()` | Get color from palette | ~30µs |
| `rgb2hsv_approximate()` | RGB → HSV | ~3µs |
| `hsv2rgb_spectrum()` | HSV → RGB | ~5µs |
| `blend()` | Linear cross-fade | ~40µs |
| `nblend()` | In-place blend | ~35µs |
| `fill_rainbow()` | Fill with rainbow | ~2ms for 320 |
| `fill_gradient()` | Fill with gradient | ~2ms for 320 |
| `nblendPaletteTowardPalette()` | Blend palettes | ~1.2ms |

---

## 10. Palette Design Worksheet

```
Effect name: ________________
Theme: (energetic/calm/professional/festive) __________

Color harmony:
  ☐ Monochromatic (single hue, vary saturation/value)
  ☐ Analogous (neighbor hues, harmonious)
  ☐ Complementary (opposite hues, high contrast)
  ☐ Triadic (3 equally-spaced hues, balanced)
  ☐ Tetradic (4 hues, complex but balanced)

Primary colors:
  1. _____________ (hue: ___°, saturation: _%, value: _%)
  2. _____________ (hue: ___°, saturation: _%, value: _%)
  3. _____________ (hue: ___°, saturation: _%, value: _%)

Animation technique:
  ☐ Static (no color change)
  ☐ Hue rotation (gHue driven)
  ☐ Palette morphing (blend target palettes)
  ☐ Audio reactive (frequency-based hue)

Blending mode:
  ☐ Replace (leds[i] = new_color)
  ☐ Additive (leds[i] = qadd8(leds[i], layer))
  ☐ Linear blend (leds[i] = blend(old, new, progress))
  ☐ Screen blend (leds[i] = screenBlend(leds[i], glow))

Palette size:
  ☐ 16-entry (compact, for transitions)
  ☐ 256-entry (smooth, for effects)

Gamma correction:
  ☐ TypicalLEDStrip (recommended default)
  ☐ TypicalSMD5050 (alternative)
  ☐ None (if colors look correct)
```

---

*Last updated: 2026-02-08*
*For use with LightwaveOS v2 FastLED color system*
