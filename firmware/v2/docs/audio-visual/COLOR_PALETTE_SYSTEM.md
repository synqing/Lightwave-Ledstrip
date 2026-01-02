# Color Palette System

**Document Version:** 1.0.0
**Last Updated:** 2025-12-31
**Companion To:** AUDIO_OUTPUT_SPECIFICATIONS.md, VISUAL_PIPELINE_MECHANICS.md

This document provides a complete reference for the color palette system in LightwaveOS v2, including all 75 palettes, their metadata, access methods, and color correction integration.

---

## Table of Contents

1. [Palette Architecture Overview](#1-palette-architecture-overview)
2. [Palette Categories](#2-palette-categories)
3. [Complete Palette Reference](#3-complete-palette-reference)
4. [Palette Flags and Metadata](#4-palette-flags-and-metadata)
5. [Color Access Methods](#5-color-access-methods)
6. [Dual-Strip Color Strategy](#6-dual-strip-color-strategy)
7. [Color Operations Reference](#7-color-operations-reference)
8. [Color Correction Engine](#8-color-correction-engine)
9. [Implementation Patterns](#9-implementation-patterns)
10. [Performance Considerations](#10-performance-considerations)

---

## 1. Palette Architecture Overview

The palette system provides a unified interface for accessing 75 curated gradient palettes optimized for LED visualization on Light Guide Plate hardware.

### Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PALETTE SYSTEM ARCHITECTURE                         │
└─────────────────────────────────────────────────────────────────────────────┘

Palette Sources (3 Collections)
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐             │
│  │   CPT-CITY      │  │    CRAMERI      │  │  R COLORSPACE   │             │
│  │   (Artistic)    │  │  (Scientific)   │  │ (LGP-Optimized) │             │
│  │                 │  │                 │  │                 │             │
│  │  Indices: 0-32  │  │ Indices: 33-56  │  │ Indices: 57-74  │             │
│  │  Count: 33      │  │  Count: 24      │  │  Count: 18      │             │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘             │
│           │                    │                    │                       │
│           └────────────────────┼────────────────────┘                       │
│                                ↓                                            │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    gMasterPalettes[75]                              │   │
│  │         TProgmemRGBGradientPaletteRef (PROGMEM storage)             │   │
│  └────────────────────────────────┬────────────────────────────────────┘   │
│                                   │                                         │
└───────────────────────────────────┼─────────────────────────────────────────┘
                                    ↓
                 ┌──────────────────────────────────────┐
                 │         CRGBPalette16                │
                 │      (Runtime working copy)         │
                 └──────────────────┬───────────────────┘
                                    ↓
        ┌───────────────────────────┼───────────────────────────┐
        ↓                           ↓                           ↓
┌───────────────┐         ┌─────────────────────┐      ┌────────────────┐
│ PaletteRef    │         │ ColorCorrection     │      │ Metadata       │
│ (ctx.palette) │         │ Engine (optional)   │      │ Arrays         │
│               │         │                     │      │                │
│ getColor(     │         │ - WHITE_HEAVY fix   │      │ - flags[]      │
│   index,      │◄────────│ - HSV saturation    │      │ - avg_Y[]      │
│   brightness  │         │ - Gamma correction  │      │ - max_bright[] │
│ )             │         └─────────────────────┘      └────────────────┘
└───────────────┘
        │
        ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                              EFFECT RENDER                                  │
│                                                                             │
│  for (uint16_t i = 0; i < ctx.ledCount; i++) {                             │
│      ctx.leds[i] = ctx.palette.getColor(paletteIndex, brightness);         │
│  }                                                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Components

| Component | Location | Purpose |
|-----------|----------|---------|
| `gMasterPalettes[]` | `Palettes_MasterData.cpp` | 75 PROGMEM gradient definitions |
| `MasterPaletteNames[]` | `Palettes_MasterData.cpp` | Human-readable palette names |
| `master_palette_flags[]` | `Palettes_MasterData.cpp` | Bitfield metadata per palette |
| `PaletteRef` | `EffectContext.h:432` | Effect access wrapper |
| `ColorCorrectionEngine` | `ColorCorrectionEngine.h` | Post-processing singleton |

---

## 2. Palette Categories

The 75 palettes are organized into three distinct categories optimized for different use cases:

### Category Summary

| Range | Category | Count | Source | Primary Use |
|-------|----------|-------|--------|-------------|
| 0-32 | **Artistic** | 33 | CPT-City | Dramatic visual effects |
| 33-56 | **Scientific** | 24 | Crameri | Data visualization, CVD-safe |
| 57-74 | **LGP-Optimized** | 18 | R Colorspace | Light guide plate interference |

### Category Range Constants

```cpp
// From Palettes_Master.h
constexpr uint8_t CPT_CITY_START     = 0;
constexpr uint8_t CPT_CITY_END       = 32;
constexpr uint8_t CRAMERI_START      = 33;
constexpr uint8_t CRAMERI_END        = 56;
constexpr uint8_t COLORSPACE_START   = 57;
constexpr uint8_t COLORSPACE_END     = 74;

constexpr uint8_t MASTER_PALETTE_COUNT = 75;
```

### Category Helper Functions

```cpp
// Check category membership
inline bool isCptCityPalette(uint8_t paletteIndex) {
    return paletteIndex <= CPT_CITY_END;
}

inline bool isCrameriPalette(uint8_t paletteIndex) {
    return paletteIndex >= CRAMERI_START && paletteIndex <= CRAMERI_END;
}

inline bool isColorspacePalette(uint8_t paletteIndex) {
    return paletteIndex >= COLORSPACE_START && paletteIndex <= COLORSPACE_END;
}

// Get category name string
inline const char* getPaletteCategory(uint8_t paletteIndex) {
    if (isCptCityPalette(paletteIndex)) return "Artistic";
    if (isCrameriPalette(paletteIndex)) return "Scientific";
    if (isColorspacePalette(paletteIndex)) return "LGP-Optimized";
    return "Unknown";
}
```

---

## 3. Complete Palette Reference

### 3.1 CPT-City Artistic Palettes (0-32)

Sourced from the CPT-City gradient archive. Optimized for dramatic visual impact.

| ID | Name | Flags | Avg Y | Max Bright | Notes |
|----|------|-------|-------|------------|-------|
| 0 | Sunset Real | WARM \| VIVID | 120 | 255 | Warm sunset gradient |
| 1 | Rivendell | COOL \| CALM | 80 | 255 | Forest greens |
| 2 | Ocean Breeze 036 | COOL \| CALM | 90 | 255 | Deep blues |
| 3 | RGI 15 | COOL \| HIGH_SAT \| VIVID | 120 | 255 | Purple-red |
| 4 | Retro 2 | WARM \| VIVID | 110 | 255 | Orange-brown |
| 5 | Analogous 1 | COOL \| HIGH_SAT \| VIVID | 130 | 255 | Blue-red spectrum |
| 6 | Pink Splash 08 | WARM \| HIGH_SAT \| VIVID | 140 | 255 | Pink-magenta |
| 7 | Coral Reef | COOL \| CALM | 120 | 255 | Cyan-blue |
| 8 | Ocean Breeze 068 | COOL \| CALM | 90 | 255 | Teal gradient |
| 9 | Pink Splash 07 | WARM \| HIGH_SAT \| VIVID | 160 | 255 | Vibrant pink |
| 10 | Vintage 01 | WARM \| CALM | 90 | 255 | Sepia tones |
| 11 | Departure | WARM \| WHITE_HEAVY \| VIVID | 160 | 200 | Fire-to-green |
| 12 | Landscape 64 | COOL \| CALM | 110 | 255 | Sky-earth |
| 13 | Landscape 33 | WARM \| CALM | 90 | 255 | Desert |
| 14 | Rainbow Sherbet | WARM \| HIGH_SAT \| VIVID | 170 | 230 | Multi-color |
| 15 | GR65 Hult | COOL \| HIGH_SAT \| VIVID | 140 | 255 | Magenta-cyan |
| 16 | GR64 Hult | COOL \| CALM | 100 | 255 | Teal-brown |
| 17 | GMT Dry Wet | COOL \| CALM | 130 | 255 | Brown-blue |
| 18 | IB Jul01 | WARM \| HIGH_SAT \| VIVID | 140 | 255 | Red-green |
| 19 | Vintage 57 | WARM \| CALM | 110 | 255 | Amber |
| 20 | IB15 | WARM \| HIGH_SAT \| VIVID | 140 | 255 | Purple-orange |
| 21 | Fuschia 7 | COOL \| HIGH_SAT \| VIVID | 150 | 255 | Purple-magenta |
| 22 | Emerald Dragon | COOL \| HIGH_SAT \| VIVID | 130 | 255 | Green gradient |
| 23 | Lava | WARM \| WHITE_HEAVY \| VIVID | 180 | 180 | Black-to-white fire |
| 24 | Fire | WARM \| WHITE_HEAVY \| VIVID | 200 | 160 | Classic fire |
| 25 | Colorful | COOL \| HIGH_SAT \| VIVID | 170 | 230 | Multi-hue |
| 26 | Magenta Evening | WARM \| HIGH_SAT \| VIVID | 130 | 255 | Pink-purple |
| 27 | Pink Purple | COOL \| WHITE_HEAVY \| VIVID | 150 | 230 | Pastel to vivid |
| 28 | Autumn 19 | WARM \| CALM | 110 | 255 | Fall colors |
| 29 | Blue Magenta White | COOL \| WHITE_HEAVY \| HIGH_SAT \| VIVID | 200 | 180 | Dramatic sweep |
| 30 | Black Magenta Red | WARM \| HIGH_SAT \| VIVID | 140 | 255 | Deep colors |
| 31 | Red Magenta Yellow | WARM \| HIGH_SAT \| VIVID | 160 | 255 | Warm spectrum |
| 32 | Blue Cyan Yellow | COOL \| HIGH_SAT \| VIVID | 180 | 230 | Cool-to-warm |

### 3.2 Crameri Scientific Palettes (33-56)

Perceptually uniform palettes by Fabio Crameri. All are CVD-friendly (colorblind-safe).

| ID | Name | Flags | Avg Y | Max Bright | Notes |
|----|------|-------|-------|------------|-------|
| 33 | Vik | COOL \| WARM \| WHITE_HEAVY \| CALM \| CVD | 170 | 220 | Diverging blue-orange |
| 34 | Tokyo | COOL \| CALM \| CVD | 150 | 255 | Purple-green sequential |
| 35 | Roma | WARM \| COOL \| CALM \| CVD | 160 | 255 | Diverging warm-cool |
| 36 | Oleron | COOL \| WARM \| WHITE_HEAVY \| CALM \| CVD | 170 | 220 | Blue-earth |
| 37 | Lisbon | COOL \| CALM \| CVD | 120 | 230 | Blue-earth |
| 38 | La Jolla | WARM \| WHITE_HEAVY \| CALM \| CVD | 160 | 230 | Yellow-brown |
| 39 | Hawaii | COOL \| WARM \| CALM \| CVD | 170 | 230 | Magenta-cyan |
| 40 | Devon | COOL \| WHITE_HEAVY \| CALM \| CVD | 180 | 220 | Purple-white |
| 41 | Cork | COOL \| WHITE_HEAVY \| CALM \| CVD | 170 | 220 | Blue-green |
| 42 | Broc | COOL \| WHITE_HEAVY \| CALM \| CVD | 160 | 220 | Blue-tan |
| 43 | Berlin | COOL \| WARM \| CALM \| CVD | 140 | 230 | Blue-orange |
| 44 | Bamako | WARM \| CALM \| CVD | 160 | 255 | Teal-tan |
| 45 | Acton | COOL \| CALM \| CVD | 170 | 230 | Purple-pink |
| 46 | Batlow | COOL \| WARM \| CALM \| CVD | 170 | 230 | Blue-yellow-pink |
| 47 | Bilbao | WARM \| WHITE_HEAVY \| CALM \| CVD | 170 | 230 | White-red |
| 48 | Buda | WARM \| WHITE_HEAVY \| CALM \| CVD | 170 | 230 | Magenta-yellow |
| 49 | Davos | COOL \| WHITE_HEAVY \| CALM \| CVD | 170 | 220 | Blue-white |
| 50 | GrayC | WHITE_HEAVY \| CALM \| CVD \| **EXCLUDED** | 160 | 220 | Grayscale |
| 51 | Imola | COOL \| CALM \| CVD | 160 | 230 | Blue-green |
| 52 | La Paz | COOL \| WHITE_HEAVY \| CALM \| CVD | 160 | 230 | Blue-tan |
| 53 | Nuuk | COOL \| WHITE_HEAVY \| CALM \| CVD | 160 | 230 | Teal-yellow |
| 54 | Oslo | COOL \| WHITE_HEAVY \| CALM \| CVD | 170 | 230 | Dark blue-light |
| 55 | Tofino | COOL \| CALM \| CVD | 140 | 230 | Blue-green |
| 56 | Turku | WARM \| WHITE_HEAVY \| CALM \| CVD | 160 | 230 | Dark-tan-white |

### 3.3 R Colorspace Palettes (57-74)

LGP-optimized palettes from R's colorspace package. Designed for perceptual uniformity.

| ID | Name | Flags | Avg Y | Max Bright | Notes |
|----|------|-------|-------|------------|-------|
| 57 | **Viridis** | COOL \| CVD | 160 | 255 | Gold standard sequential |
| 58 | **Plasma** | WARM \| VIVID \| CVD | 150 | 255 | Fire-like perceptual |
| 59 | **Inferno** | WARM \| VIVID \| CVD | 140 | 240 | High contrast dramatic |
| 60 | **Magma** | WARM \| CALM \| CVD | 130 | 240 | Subtle, elegant |
| 61 | Cubhelix | CALM \| CVD | 150 | 255 | Monotonic luminance spiral |
| 62 | Abyss | COOL \| CALM \| CVD | 80 | 255 | Deep ocean blues |
| 63 | Bathy | COOL \| CALM \| CVD | 100 | 255 | Bathymetric ocean |
| 64 | Ocean | COOL \| CALM \| CVD | 110 | 255 | Blues throughout |
| 65 | Nighttime | COOL \| CALM \| CVD | 70 | 255 | Dark purples/blues |
| 66 | Seafloor | COOL \| CALM \| CVD | 100 | 255 | Marine blue-green |
| 67 | IBCSO | COOL \| CALM \| CVD | 90 | 255 | Antarctic ocean |
| 68 | Copper | WARM \| CALM \| CVD | 140 | 200 | Warm copper tones |
| 69 | Hot | WARM \| VIVID \| CVD | 180 | 200 | Classic thermal |
| 70 | Cool | COOL \| VIVID \| CVD | 180 | 220 | Cyan-magenta |
| 71 | Earth | WARM \| CALM \| CVD | 140 | 220 | Terrain/topographic |
| 72 | Sealand | COOL \| CALM \| CVD | 130 | 220 | Sea-to-land transition |
| 73 | Split | COOL \| WARM \| CALM \| CVD | 140 | 200 | Diverging blue-red |
| 74 | Red2Green | WARM \| VIVID \| CVD | 160 | 200 | Diverging red-green |

---

## 4. Palette Flags and Metadata

### 4.1 Flag Definitions

```cpp
// From Palettes_Master.h
namespace lightwaveos {
namespace palettes {

constexpr uint8_t PAL_WARM        = 0x01;  // Warm tones (reds, oranges, yellows)
constexpr uint8_t PAL_COOL        = 0x02;  // Cool tones (blues, greens, purples)
constexpr uint8_t PAL_HIGH_SAT    = 0x04;  // High saturation
constexpr uint8_t PAL_WHITE_HEAVY = 0x08;  // Contains significant white/bright regions
constexpr uint8_t PAL_CALM        = 0x10;  // Subtle, calm transitions
constexpr uint8_t PAL_VIVID       = 0x20;  // Vivid, high-contrast transitions
constexpr uint8_t PAL_CVD_FRIENDLY= 0x40;  // Colorblind-safe (Crameri/Colorspace)
constexpr uint8_t PAL_EXCLUDED    = 0x80;  // Exclude from random selection

} // namespace palettes
} // namespace lightwaveos
```

### 4.2 Flag Semantics

| Flag | Bit | Purpose | When to Use |
|------|-----|---------|-------------|
| `PAL_WARM` | 0x01 | Contains warm colors (red/orange/yellow) | Filter for cozy/energetic effects |
| `PAL_COOL` | 0x02 | Contains cool colors (blue/green/purple) | Filter for calm/professional effects |
| `PAL_HIGH_SAT` | 0x04 | Highly saturated colors | Attention-grabbing, vibrant effects |
| `PAL_WHITE_HEAVY` | 0x08 | Significant white/bright regions | Triggers ColorCorrectionEngine |
| `PAL_CALM` | 0x10 | Gentle transitions | Ambient lighting, meditation |
| `PAL_VIVID` | 0x20 | High-contrast transitions | Music visualization, alerts |
| `PAL_CVD_FRIENDLY` | 0x40 | Colorblind-safe | Accessibility-critical applications |
| `PAL_EXCLUDED` | 0x80 | Exclude from random selection | Grayscale, pure white palettes |

### 4.3 Metadata Arrays

```cpp
// Palette characteristic flags
extern const uint8_t master_palette_flags[75];

// Average perceptual brightness (BT.601 luminance)
extern const uint8_t master_palette_avg_Y[75];

// Maximum brightness cap (for power management)
extern const uint8_t master_palette_max_brightness[75];
```

### 4.4 Flag Query Functions

```cpp
// Check if palette has specific flag
inline bool paletteHasFlag(uint8_t paletteIndex, uint8_t flag) {
    if (paletteIndex >= MASTER_PALETTE_COUNT) return false;
    return (master_palette_flags[paletteIndex] & flag) != 0;
}

// Convenience wrappers
inline bool isPaletteWarm(uint8_t paletteIndex);
inline bool isPaletteCool(uint8_t paletteIndex);
inline bool isPaletteCalm(uint8_t paletteIndex);
inline bool isPaletteVivid(uint8_t paletteIndex);
inline bool isPaletteCVDFriendly(uint8_t paletteIndex);

// Get brightness-adjusted max for power management
inline uint8_t getPaletteMaxBrightness(uint8_t paletteIndex);

// Get average brightness (perceived luminance)
inline uint8_t getPaletteAvgBrightness(uint8_t paletteIndex);

// Get palette name (safe, returns "Unknown" if out of range)
inline const char* getPaletteName(uint8_t paletteIndex);
```

---

## 5. Color Access Methods

### 5.1 Primary Access: PaletteRef

Effects access colors through `ctx.palette`, a `PaletteRef` wrapper that provides safe, efficient palette lookup.

```cpp
// PaletteRef class (simplified from EffectContext.h:432)
class PaletteRef {
public:
    PaletteRef() : m_palette(nullptr) {}
    explicit PaletteRef(const CRGBPalette16* palette) : m_palette(palette) {}

    /**
     * @brief Get color from palette
     * @param index Position in palette (0-255)
     * @param brightness Optional brightness scaling (0-255)
     * @return Color from palette with LINEARBLEND interpolation
     */
    CRGB getColor(uint8_t index, uint8_t brightness = 255) const {
        if (!m_palette) return CRGB::Black;
        return ColorFromPalette(*m_palette, index, brightness, LINEARBLEND);
    }

    bool isValid() const { return m_palette != nullptr; }
    const CRGBPalette16* getRaw() const { return m_palette; }

private:
    const CRGBPalette16* m_palette;
};
```

### 5.2 Usage Patterns

```cpp
// Basic color lookup
CRGB color = ctx.palette.getColor(ctx.gHue);

// With brightness control
CRGB color = ctx.palette.getColor(index, 192);  // 75% brightness

// Position-based coloring
for (uint16_t i = 0; i < ctx.ledCount; i++) {
    uint8_t palettePos = map(i, 0, ctx.ledCount - 1, 0, 255);
    ctx.leds[i] = ctx.palette.getColor(palettePos);
}

// Distance-based coloring (CENTER ORIGIN)
for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    uint16_t dist = centerPairDistance(i);
    uint8_t palettePos = ctx.gHue + (dist * 3);  // Spiral outward
    ctx.leds[i] = ctx.palette.getColor(palettePos, ctx.brightness);
}
```

### 5.3 Palette Index Interpretation

The `index` parameter (0-255) maps across the 16-color palette with linear interpolation:

```
Index:     0    16    32    48    64   ...   240   255
           │     │     │     │     │          │     │
Palette:   ●─────●─────●─────●─────●──...──●─────●
         Color  C1    C2    C3    C4       C14   C15
           0
```

FastLED's `ColorFromPalette()` with `LINEARBLEND` smoothly interpolates between the 16 control points.

---

## 6. Dual-Strip Color Strategy

LightwaveOS uses two 160-LED strips (320 total). The palette system supports several strategies for creating complementary patterns.

### 6.1 Complementary Offset (+128)

The most common pattern: Strip 2 uses colors 180 degrees opposite on the palette.

```cpp
// 180-degree offset (complementary colors)
for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
    uint8_t basePos = ctx.gHue + distFromCenter;

    // Strip 1: Base palette position
    ctx.leds[i] = ctx.palette.getColor(basePos, ctx.brightness);

    // Strip 2: Offset by 128 (half the palette = 180°)
    ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
        (uint8_t)(basePos + 128),
        ctx.brightness
    );
}
```

### 6.2 Triadic Offset (+64)

90-degree offset creates a triadic color relationship.

```cpp
// 90-degree offset (triadic colors)
ctx.leds[i] = ctx.palette.getColor(palettePos, brightness);
ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
    (uint8_t)(palettePos + 64),  // 90-degree shift
    brightness
);
```

### 6.3 Visual Diagram

```
Palette Index:  0       64      128      192     255
                │       │       │        │       │
                ▼       ▼       ▼        ▼       ▼
             ┌──────────────────────────────────────┐
Palette:     │████████████████████████████████████│
             └──────────────────────────────────────┘
                ↑                ↑
                │                │
           Strip 1 (base)   Strip 2 (+128)

Example with ctx.gHue = 0:
- Strip 1 samples palette at index 0 (e.g., blue)
- Strip 2 samples palette at index 128 (e.g., orange)
```

### 6.4 Common Dual-Strip Patterns

| Pattern | Offset | Effect |
|---------|--------|--------|
| Complementary | +128 | Opposite colors, high contrast |
| Triadic | +64 | Third-harmonics, balanced |
| Analogous | +32 | Similar colors, smooth |
| Split-Complementary | +96 | Offset complementary |
| Mirrored | Same index | Identical colors on both strips |

---

## 7. Color Operations Reference

FastLED provides optimized color operations. These are critical for performance in render loops.

### 7.1 Brightness Scaling

```cpp
// scale8: 8-bit multiply (0-255 scale factor)
// Result = (value * scale) >> 8
uint8_t dim = scale8(brightness, 128);  // 50% brightness

// scale8_video: Video-safe scaling (preserves visibility at low values)
// Slightly brighter at low values than scale8
uint8_t videoDim = scale8_video(brightness, 128);

// nscale8: In-place scaling
CRGB color = CRGB::Red;
nscale8(&color, 128);  // Modifies color in place

// nscale8x3_video: Scale all 3 channels, video-safe
nscale8x3_video(color.r, color.g, color.b, 128);
```

### 7.2 Saturating Arithmetic

```cpp
// qadd8: Saturating add (clamps at 255, no wraparound)
uint8_t sum = qadd8(200, 100);  // Returns 255, not 44

// qsub8: Saturating subtract (clamps at 0)
uint8_t diff = qsub8(50, 100);  // Returns 0, not 206

// Example: Adding flash effect without overflow
brightness = qadd8(brightness, (uint8_t)(flash * 60.0f));
```

### 7.3 HSV Construction

```cpp
// Direct HSV construction
CHSV hsv(hue, saturation, value);  // All 0-255

// Convert to RGB
CRGB rgb;
hsv2rgb_rainbow(hsv, rgb);  // Fast rainbow conversion
hsv2rgb_spectrum(hsv, rgb); // Slower, more accurate

// In-place conversion
CRGB color = CHSV(128, 255, 200);  // Implicit conversion
```

### 7.4 Palette Blending

```cpp
// nblendPaletteTowardPalette: Smooth palette transitions
// Updates first palette toward second by 'maxChanges' steps per call
nblendPaletteTowardPalette(currentPalette, targetPalette, 48);

// Full transition typically takes ~6 frames at 48 steps
```

### 7.5 Color Math Reference

| Function | Operation | Notes |
|----------|-----------|-------|
| `scale8(val, scale)` | `(val * scale) >> 8` | Fast multiply |
| `scale8_video(val, scale)` | Video-safe scale | Never goes to 0 unless scale=0 |
| `qadd8(a, b)` | `min(a + b, 255)` | Saturating add |
| `qsub8(a, b)` | `max(a - b, 0)` | Saturating subtract |
| `blend(a, b, ratio)` | Linear interpolation | ratio 0=a, 255=b |
| `lerp8by8(a, b, frac)` | Linear interpolation | frac 0=a, 255=b |
| `dim8_raw(val)` | Approximate `val²` | Good for gamma |
| `brighten8_raw(val)` | Approximate `√val` | Inverse gamma |

---

## 8. Color Correction Engine

The ColorCorrectionEngine provides post-processing to fix common palette issues on LED hardware.

### 8.1 Architecture

```
Effect Render Complete
         │
         ↓
┌─────────────────────────────────────────────────────────────────┐
│                    ColorCorrectionEngine                        │
│                                                                 │
│  ┌───────────────┐    ┌──────────────────┐    ┌─────────────┐  │
│  │ V-Clamping    │ →  │ White Guardrail   │ →  │ Auto-       │  │
│  │ (max=200)     │    │ (HSV/RGB mode)    │    │ Exposure    │  │
│  └───────────────┘    └──────────────────┘    └─────────────┘  │
│         │                      │                     │          │
│         ↓                      ↓                     ↓          │
│  ┌───────────────┐    ┌──────────────────┐    ┌─────────────┐  │
│  │ Saturation    │    │ Brown Guardrail   │    │ Gamma       │  │
│  │ Boost         │    │ (LC-style)        │    │ Correction  │  │
│  └───────────────┘    └──────────────────┘    └─────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
         │
         ↓
    LED Output
```

### 8.2 Correction Modes

```cpp
enum class CorrectionMode : uint8_t {
    OFF = 0,    // No correction applied
    HSV = 1,    // HSV saturation boost (enforce minimum saturation)
    RGB = 2,    // RGB white reduction (LC-style, reduce white component)
    BOTH = 3    // Both HSV and RGB layered together
};
```

### 8.3 Configuration

```cpp
struct ColorCorrectionConfig {
    // Mode Selection
    CorrectionMode mode = CorrectionMode::BOTH;

    // HSV Mode Parameters
    uint8_t hsvMinSaturation = 120;  // 0-255, colors below this get boosted

    // RGB Mode Parameters
    uint8_t rgbWhiteThreshold = 150;  // minRGB value to consider "whitish"
    uint8_t rgbTargetMin = 100;       // Target minimum RGB after correction

    // Auto-Exposure Parameters
    bool autoExposureEnabled = false;
    uint8_t autoExposureTarget = 110;  // Target average luma (BT.601)

    // Gamma Correction
    bool gammaEnabled = true;
    float gammaValue = 2.2f;  // Standard gamma (1.0-3.0)

    // Brown Guardrail (LC_SelfContained pattern)
    bool brownGuardrailEnabled = false;
    uint8_t maxGreenPercentOfRed = 28;  // Max G as % of R for browns
    uint8_t maxBluePercentOfRed = 8;    // Max B as % of R for browns

    // V-Clamping (White Accumulation Prevention)
    bool vClampEnabled = true;
    uint8_t maxBrightness = 200;        // Max brightness (conservative)
    uint8_t saturationBoostAmount = 25; // Saturation boost after V-clamp
};
```

### 8.4 Usage

```cpp
// Access singleton
auto& engine = ColorCorrectionEngine::getInstance();

// Set mode
engine.setMode(CorrectionMode::BOTH);

// At palette load time (for WHITE_HEAVY palettes)
engine.correctPalette(palette, paletteFlags);

// Post-render (in render loop) - applies full pipeline
engine.processBuffer(leds, ledCount);
```

### 8.5 When Correction is Applied

The engine automatically processes `PAL_WHITE_HEAVY` palettes. Effects in certain families skip correction (see `PatternRegistry::shouldSkipColorCorrection()`):

| Skip Correction | Reason |
|-----------------|--------|
| INTERFERENCE family | Precise amplitude for wave interference |
| ADVANCED_OPTICAL family | LGP-sensitive patterns |
| Stateful effects (Confetti, Ripple) | Read previous frame buffer |
| PHYSICS_BASED family | Precise values for simulations |
| MATHEMATICAL family | Exact RGB for mathematical mappings |

---

## 9. Implementation Patterns

### 9.1 Basic Palette Effect

```cpp
void LGPBasicEffect::render(plugins::EffectContext& ctx) {
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // CENTER ORIGIN: distance from center (79/80)
        uint16_t dist = centerPairDistance(i);

        // Palette position: rotating + distance-based gradient
        uint8_t palettePos = ctx.gHue + (dist << 1);

        // Apply to both strips with complementary offset
        ctx.leds[i] = ctx.palette.getColor(palettePos, ctx.brightness);
        ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
            (uint8_t)(palettePos + 128),
            ctx.brightness
        );
    }
}
```

### 9.2 Audio-Reactive Color Selection

```cpp
void render(plugins::EffectContext& ctx) {
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Use chroma dominant bin for color offset
        float maxChroma = 0.0f;
        uint8_t dominantBin = 0;
        for (uint8_t bin = 0; bin < 12; ++bin) {
            if (ctx.audio.controlBus.chroma[bin] > maxChroma) {
                maxChroma = ctx.audio.controlBus.chroma[bin];
                dominantBin = bin;
            }
        }

        // Map 12 chroma bins to palette range
        uint8_t chromaOffset = dominantBin * (255 / 12);

        // Apply with smoothing
        float alphaBin = 1.0f - expf(-ctx.getSafeDeltaSeconds() / 0.25f);
        m_chromaSmooth += ((float)dominantBin - m_chromaSmooth) * alphaBin;
        chromaOffset = (uint8_t)(m_chromaSmooth * (255.0f / 12.0f));

        // Use in color generation
        uint8_t hue = ctx.gHue + chromaOffset;
        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
    }
#endif
}
```

### 9.3 Brightness Modulation with Energy

```cpp
// Brightness modulation based on audio energy
float brightnessGain = 0.4f + 0.5f * energyAvgSmooth + 0.4f * energyDeltaSmooth;
if (brightnessGain > 1.5f) brightnessGain = 1.5f;
if (brightnessGain < 0.3f) brightnessGain = 0.3f;

// Apply to brightness parameter
uint8_t scaledBrightness = scale8(ctx.brightness, (uint8_t)(brightnessGain * 255.0f));
ctx.leds[i] = ctx.palette.getColor(palettePos, scaledBrightness);
```

### 9.4 Bandgap Color Differentiation

From LGPPhotonicCrystalEffect - using palette for allowed vs forbidden zones:

```cpp
// Bandgap structure determines hue
bool inBandgap = cellPosition < (latticeSize >> 1);

// Allowed zones: base hue
// Forbidden zones: complementary hue (+128)
uint8_t baseHue = inBandgap ? ctx.gHue : (uint8_t)(ctx.gHue + 128);

// Add audio-based chroma offset
baseHue += chromaOffset;

// Distance-based gradient within zone
uint8_t palettePos = baseHue + distFromCenter / 4;

ctx.leds[i] = ctx.palette.getColor(palettePos, brightness);
```

---

## 10. Performance Considerations

### 10.1 Memory Layout

| Array | Size | Storage | Access Time |
|-------|------|---------|-------------|
| `gMasterPalettes[]` | 75 × 4 bytes = 300B | PROGMEM | ~100ns (flash read) |
| `CRGBPalette16` | 16 × 3 bytes = 48B | RAM | ~10ns (RAM read) |
| `master_palette_flags[]` | 75 bytes | PROGMEM | ~100ns |
| `MasterPaletteNames[]` | 75 pointers | PROGMEM | ~100ns |

### 10.2 Optimization Tips

1. **Cache the palette pointer**: `ctx.palette` is already a reference, but avoid repeated `getRaw()` calls
2. **Use scale8 instead of division**: `scale8(val, 128)` is faster than `val / 2`
3. **Batch color operations**: Process multiple LEDs before writing to strip
4. **Avoid ColorFromPalette in tight loops if possible**: Pre-compute palette samples for repeating patterns

### 10.3 ColorCorrectionEngine Cost

| Stage | Approximate Cost |
|-------|------------------|
| V-Clamping | ~500µs per 320 LEDs |
| HSV Saturation Boost | ~800µs per 320 LEDs |
| RGB White Curation | ~400µs per 320 LEDs |
| Gamma LUT | ~200µs per 320 LEDs |
| **Total (BOTH mode)** | **~1,500µs per frame** |

**Recommendation**: Skip color correction for LGP-sensitive effects using `PatternRegistry::shouldSkipColorCorrection()`.

---

## Related Documentation

- **[AUDIO_OUTPUT_SPECIFICATIONS.md](AUDIO_OUTPUT_SPECIFICATIONS.md)** - Audio processing pipeline
- **[VISUAL_PIPELINE_MECHANICS.md](VISUAL_PIPELINE_MECHANICS.md)** - Visual rendering system
- **[IMPLEMENTATION_PATTERNS.md](IMPLEMENTATION_PATTERNS.md)** - Complete code examples
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues and fixes

---

## Appendix: Palette Selection Guide

### By Use Case

| Use Case | Recommended Palettes | Flags to Filter |
|----------|----------------------|-----------------|
| Ambient/Relaxation | 57-67 (Colorspace deep) | `PAL_CALM` |
| Music Visualization | 23-24, 58-59 | `PAL_VIVID` |
| Professional/Corporate | 33-56 (Crameri) | `PAL_CVD_FRIENDLY` |
| Party/Energy | 5-6, 9, 29-31 | `PAL_HIGH_SAT \| PAL_VIVID` |
| Nature/Organic | 1, 12-13, 62-67 | `PAL_COOL \| PAL_CALM` |
| Fire/Warmth | 23-24, 68-69 | `PAL_WARM \| PAL_VIVID` |

### By LGP Optimization

| Optimization | Palette Range | Notes |
|--------------|---------------|-------|
| Best for interference | 57-67 | Perceptually uniform, dark backgrounds |
| High contrast waves | 58-59 (Plasma, Inferno) | Strong gradients |
| Subtle effects | 60-61, 65 | Low average brightness |
| Accessibility | 33-56, 57-74 | All CVD-friendly |
