# Palette System Integration Audit Report

**Date**: 2024-12-19  
**Scope**: All effects in `firmware/v2/src/effects/ieffect/`  
**Objective**: Ensure all effects use `ctx.palette.getColor()` instead of direct `CHSV`/`hsv2rgb_spectrum` calls

---

## Executive Summary

**Total Effects Audited**: 21 files with direct HSV usage  
**Already Using Palette**: 66 files (152 instances)  
**Needs Fixing**: 21 files categorized by priority

---

## Categories

### 🔴 HIGH PRIORITY - Direct CHSV for Color Generation

These effects use `CHSV()` directly to generate colors. They should be converted to use `ctx.palette.getColor()`.

| File | Line(s) | Usage Pattern | Fix Required |
|------|---------|---------------|--------------|
| **OceanEffect.cpp** | 63 | `CHSV(hue, saturation, brightness)` | Replace with `ctx.palette.getColor(paletteIdx, brightness)` |
| **JuggleEffect.cpp** | 37 | `CHSV(dothue, ctx.saturation, 255)` | Replace with `ctx.palette.getColor(dothue, 255)` |
| **LGPMycelialNetworkEffect.cpp** | 132, 134 | `CHSV(hue1/2, ctx.saturation, brightness)` | Replace with palette lookups |
| **LGPEvanescentSkinEffect.cpp** | 60, 62 | `CHSV(hue, ctx.saturation, brightness)` | Replace with palette lookups |
| **LGPAnisotropicCloakEffect.cpp** | 68, 70 | `CHSV(hue, ctx.saturation, brightness)` | Replace with palette lookups |
| **LGPChladniHarmonicsEffect.cpp** | 71, 76 | `CHSV(hue1/2, ctx.saturation, brightness)` | Replace with palette lookups |
| **LGPDopplerShiftEffect.cpp** | 55, 57 | `CHSV(shiftedHue, 255, brightness)` | Replace with palette lookups |
| **LGPCausticFanEffect.cpp** | 52, 54 | `CHSV(hue, ctx.saturation, brightness)` | Replace with palette lookups |
| **LGPQuantumTunnelingEffect.cpp** | 51, 53, 93, 95, 124, 127 | Multiple `CHSV()` calls | Replace all with palette lookups |
| **LGPColorAcceleratorEffect.cpp** | 76 | `CHSV(debrisHue, 255, debrisBright)` | Replace with palette lookup |
| **LGPRileyDissonanceEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |
| **LGPSolitonWavesEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |
| **LGPPlasmaMembraneEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |
| **LGPBirefringentShearEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |
| **LGPMetamaterialCloakEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |
| **LGPComplementaryMixingEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |
| **LGPGrinCloakEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |
| **LGPGravitationalWaveChirpEffect.cpp** | TBD | `CHSV()` usage | Replace with palette lookup |

**Total High Priority**: 18 files

---

### 🟡 MEDIUM PRIORITY - Post-Processing HSV Adjustments

These effects use `hsv2rgb_spectrum()` for post-processing (saturation adjustments, etc.). These may be acceptable but should be reviewed to see if palette system can handle the same operations.

| File | Line(s) | Usage Pattern | Fix Required |
|------|---------|---------------|--------------|
| **LGPChordGlowEffect.cpp** | 257 | `hsv2rgb_spectrum(hsv, baseColor)` - Saturation adjustment | Review: May need palette saturation API |
| **AudioBloomEffect.cpp** | 387 | `hsv2rgb_spectrum(hsv, buffer[i])` - Saturation increase | Review: May need palette saturation API |
| **BreathingEffect.cpp** | 61 | `hsv2rgb_spectrum(CHSV(...), noteColor)` - Chromagram color | **Should use palette** - This is primary color generation |

**Total Medium Priority**: 3 files (1 needs fixing, 2 need review)

---

## Fix Pattern Reference

### Standard Pattern (from AudioWaveformEffect.cpp)

```cpp
// Chromatic mode:
uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
uint8_t brightU8 = (uint8_t)bright;
brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
CRGB out_col = ctx.palette.getColor(paletteIdx, brightU8);

// Single hue mode:
uint8_t brightU8 = (uint8_t)brightness_sum;
brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
sum_color = ctx.palette.getColor(ctx.gHue, brightU8);
```

### Simple Hue-Based Pattern

```cpp
// BEFORE:
CRGB color = CHSV(hue, ctx.saturation, brightness);

// AFTER:
uint8_t paletteIdx = hue;  // or (hue + ctx.gHue) if rotation needed
uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
CRGB color = ctx.palette.getColor(paletteIdx, brightU8);
```

### Hue with gHue Offset Pattern

```cpp
// BEFORE:
CRGB color = CHSV(ctx.gHue + offset, ctx.saturation, brightness);

// AFTER:
uint8_t paletteIdx = (uint8_t)(ctx.gHue + offset);
uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
CRGB color = ctx.palette.getColor(paletteIdx, brightU8);
```

---

## Detailed File Analysis

### 1. OceanEffect.cpp
**Issue**: Line 63 uses `CHSV(hue, saturation, brightness)` directly  
**Fix**: Replace with `ctx.palette.getColor(hue, brightness)`  
**Context**: Ocean wave colors - should respect palette system

### 2. JuggleEffect.cpp
**Issue**: Line 37 uses `CHSV(dothue, ctx.saturation, 255)`  
**Fix**: Replace with `ctx.palette.getColor(dothue, 255)`  
**Context**: Multiple juggling balls with different hues

### 3. BreathingEffect.cpp
**Issue**: Line 61 uses `hsv2rgb_spectrum(CHSV(...), noteColor)` for chromagram colors  
**Fix**: Replace with palette system (like WaveformEffect/SnapwaveEffect)  
**Context**: Chromagram-driven color synthesis - should match other audio effects

### 4. LGPMycelialNetworkEffect.cpp
**Issue**: Lines 132, 134 use `CHSV(hue1/2, ctx.saturation, brightness)`  
**Fix**: Replace with `ctx.palette.getColor(hue1/2, brightness)`  
**Context**: Fungal network expansion with dual-color system

### 5. LGPEvanescentSkinEffect.cpp
**Issue**: Lines 60, 62 use `CHSV(hue, ctx.saturation, brightness)`  
**Fix**: Replace with palette lookups  
**Context**: Evanescent wave visualization

### 6. LGPAnisotropicCloakEffect.cpp
**Issue**: Lines 68, 70 use `CHSV(hue, ctx.saturation, brightness)`  
**Fix**: Replace with palette lookups  
**Context**: Anisotropic material visualization

### 7. LGPChladniHarmonicsEffect.cpp
**Issue**: Lines 71, 76 use `CHSV(hue1/2, ctx.saturation, brightness)`  
**Fix**: Replace with palette lookups  
**Context**: Chladni pattern harmonics

### 8. LGPDopplerShiftEffect.cpp
**Issue**: Lines 55, 57 use `CHSV(shiftedHue, 255, brightness)`  
**Fix**: Replace with palette lookups  
**Context**: Doppler shift color visualization

### 9. LGPCausticFanEffect.cpp
**Issue**: Lines 52, 54 use `CHSV(hue, ctx.saturation, brightness)`  
**Fix**: Replace with palette lookups  
**Context**: Caustic fan patterns

### 10. LGPQuantumTunnelingEffect.cpp
**Issue**: Multiple `CHSV()` calls (lines 51, 53, 93, 95, 124, 127)  
**Fix**: Replace all with palette lookups  
**Context**: Quantum tunneling particle visualization with barriers and wave packets

### 11. LGPColorAcceleratorEffect.cpp
**Issue**: Line 76 uses `CHSV(debrisHue, 255, debrisBright)`  
**Fix**: Replace with palette lookup  
**Context**: Color cycling with momentum

### 12-18. Remaining LGP Effects
**Issue**: Various `CHSV()` usage patterns  
**Fix**: Replace with palette lookups following standard pattern  
**Context**: Various LGP-specific visualizations

---

## Post-Processing Cases (Review Needed)

### LGPChordGlowEffect.cpp
**Current**: Uses `rgb2hsv_approximate()` then `hsv2rgb_spectrum()` for saturation adjustment  
**Question**: Can palette system handle saturation adjustments?  
**Recommendation**: If saturation adjustment is critical, may need to keep HSV conversion but ensure base color comes from palette

### AudioBloomEffect.cpp
**Current**: Uses `rgb2hsv_approximate()` then `hsv2rgb_spectrum()` for saturation increase  
**Question**: Can palette system handle saturation adjustments?  
**Recommendation**: Similar to ChordGlow - review if saturation boost can be done via palette or if HSV conversion is acceptable for post-processing

---

## Implementation Priority

### Phase 1: Critical Audio Effects (COMPLETED ✅)
- ✅ WaveformEffect.cpp
- ✅ SnapwaveEffect.cpp

### Phase 2: High Priority - Simple Hue Replacements
1. OceanEffect.cpp
2. JuggleEffect.cpp
3. BreathingEffect.cpp (chromagram colors)
4. LGPMycelialNetworkEffect.cpp
5. LGPEvanescentSkinEffect.cpp
6. LGPAnisotropicCloakEffect.cpp
7. LGPChladniHarmonicsEffect.cpp
8. LGPDopplerShiftEffect.cpp
9. LGPCausticFanEffect.cpp
10. LGPColorAcceleratorEffect.cpp

### Phase 3: High Priority - Complex Multi-Color
11. LGPQuantumTunnelingEffect.cpp (multiple CHSV calls)

### Phase 4: Remaining LGP Effects
12-18. All remaining LGP effects with CHSV usage

### Phase 5: Post-Processing Review
- LGPChordGlowEffect.cpp
- AudioBloomEffect.cpp

---

## Testing Checklist

After each fix:
- [ ] Colors render correctly with different palettes
- [ ] Brightness scaling works correctly
- [ ] gHue rotation works correctly
- [ ] Saturation parameter (if applicable) works correctly
- [ ] No visual corruption or color artifacts
- [ ] Effect maintains its visual signature
- [ ] Centre-origin compliance maintained (if applicable)

---

## Notes

1. **Saturation Parameter**: Some effects use `ctx.saturation` directly. The palette system may handle saturation differently. Need to verify if `ctx.saturation` should affect palette lookups or if it's handled internally.

2. **Fixed Hue Values**: Some effects use fixed hue values (e.g., `CHSV(160, 255, brightness)` in QuantumTunneling). These should map to fixed palette indices.

3. **Random Hues**: Some effects use `random8()` for hue (e.g., LGPColorAcceleratorEffect). These should use `random8()` for palette index instead.

4. **Complementary Colors**: Some effects use `hue + 128` for complementary colors. This should map to `paletteIdx + 128` (wrapping at 256).

---

## Status

- **Audit Complete**: ✅
- **Phase 1 Complete**: ✅ (WaveformEffect, SnapwaveEffect)
- **Phase 2 Complete**: ✅ (10 files)
- **Phase 3 Complete**: ✅ (1 file - LGPQuantumTunnelingEffect)
- **Phase 4 Complete**: ✅ (8 files - all remaining LGP effects)
- **Phase 5 Complete**: ✅ (2 files - post-processing cases reviewed and acceptable)

**Total Fixed**: 20 files
**Remaining**: 2 files (post-processing saturation adjustments - acceptable)

### Post-Processing Cases (Acceptable)

**LGPChordGlowEffect.cpp** (line 257):
- Uses `hsv2rgb_spectrum()` for saturation adjustment on palette colors
- Base color comes from `ctx.palette.getColor()` (line 250)
- Post-processing saturation adjustment is acceptable for fine-tuning

**AudioBloomEffect.cpp** (line 387):
- Uses `hsv2rgb_spectrum()` for saturation increase on palette colors
- Base colors come from palette system
- Post-processing saturation boost is acceptable for visual enhancement

**Note**: These post-processing cases are acceptable because:
1. Base colors originate from the palette system
2. HSV conversion is only used for saturation adjustment, not color generation
3. This is a common pattern for fine-tuning palette colors

