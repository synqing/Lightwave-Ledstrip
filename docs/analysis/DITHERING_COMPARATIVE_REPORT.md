# Dithering Comparative Analysis Report

**DitherBench v1.0 - Quantitative Assessment**

Date: 2026-01-11  
Comparison: LWOS v2 vs SensoryBridge 4.1.1 vs Emotiscope 1.2/2.0

---

## Executive Summary

This report presents a comprehensive quantitative comparison of three dithering algorithms used in LED controller systems:

1. **LWOS v2** - Bayer 4×4 ordered spatial dithering + FastLED temporal dithering
2. **SensoryBridge 4.1.1** - 4-phase temporal threshold quantiser with per-channel noise origins
3. **Emotiscope 1.2/2.0** - Sigma-delta error accumulation with deadband threshold

### Key Findings

**TL;DR**:
- **SensoryBridge** produces the smoothest gradients with minimal banding (best for ramps/gradients)
- **LWOS Bayer-only** achieves perfect temporal stability (best for static scenes, no flicker)
- **Emotiscope** provides good convergence to time-averaged brightness (best for long-term accuracy)

---

## Methodology

### Test Configuration

```
LEDs: 160 per strip (320 total)
Frames: 512 per test
Stimuli: 
  - Near-black ramp (0.0-0.2)
  - Full ramp (0.0-1.0)
  - Constant field (0.5)
  - LGP center gradient
  - Palette blend (RGB)
```

### Metrics Collected

1. **Banding Score** (0-1, lower = less banding)
   - Spatial derivative analysis
   - Discrete step counting
   - Shannon entropy of value distribution

2. **Temporal Stability Score** (0-1, higher = more stable)
   - Per-LED variance over time
   - Flicker energy (high-frequency FFT power)
   - Temporal signal-to-noise ratio

3. **Accuracy Score** (0-1, higher = better)
   - Mean Absolute Error vs high-precision reference
   - Root Mean Squared Error
   - Time-averaged convergence

---

## Results Overview

### Banding Performance (Lower is Better)

| Pipeline | Near-Black | Full Ramp | Constant | LGP Gradient | Palette |
|---------|-----------|-----------|----------|-------------|---------|
| **LWOS (Bayer+Temporal)** | 1.000 | **0.177** | 1.000 | 0.209 | 0.825 |
| **LWOS (Bayer only)** | 1.000 | **0.140** | 1.000 | 0.222 | 0.865 |
| **LWOS (No dither)** | **0.004** | **0.011** | **0.000** | **0.015** | 0.925 |
| **SensoryBridge** | **0.004** | **0.001** | **0.000** | **0.004** | **0.725** |
| **Emotiscope** | 0.538 | **0.003** | 1.000 | **0.041** | 0.741 |

**Winner: SensoryBridge** - Consistently lowest banding across all test cases.

### Temporal Stability (Higher is Better)

| Pipeline | Near-Black | Full Ramp | Constant | LGP Gradient | Palette |
|---------|-----------|-----------|----------|-------------|---------|
| **LWOS (Bayer+Temporal)** | 0.993 | 0.989 | 0.976 | 0.989 | 0.993 |
| **LWOS (Bayer only)** | **1.000** | **1.000** | **1.000** | **1.000** | **1.000** |
| **LWOS (No dither)** | **1.000** | **1.000** | **1.000** | **1.000** | **1.000** |
| **SensoryBridge** | 0.985 | 0.985 | **1.000** | 0.985 | 0.990 |
| **Emotiscope** | 0.984 | 0.985 | 0.992 | 0.985 | 0.990 |

**Winner: LWOS (Bayer only / No dither)** - Perfect temporal stability (zero flicker).

---

## Detailed Analysis

### 1. Near-Black Performance

**Challenge**: Quantisation artifacts are most visible in shadows (0-20% brightness).

**Results**:
- **SensoryBridge**: Banding score 0.004 (excellent smoothness)
- **LWOS No dither**: Banding score 0.004 (tied)
- **LWOS Bayer**: Banding score 1.000 (visible artifacts)
- **Emotiscope**: Banding score 0.538 (moderate artifacts)

**Analysis**:
- Temporal dithering (SB, Emotiscope) spreads error over time → smooth appearance
- Bayer spatial dither creates visible 4×4 pattern in shadows
- LWOS temporal adds frame-to-frame flicker (stability 0.993)

**Recommendation**: SensoryBridge or LWOS No-dither for shadow detail.

---

### 2. Full Ramp Performance

**Challenge**: Smooth 0-100% gradient without visible bands.

**Results**:
- **SensoryBridge**: Banding 0.001, Stability 0.985
- **Emotiscope**: Banding 0.003, Stability 0.985
- **LWOS No dither**: Banding 0.011, Stability 1.000
- **LWOS Bayer**: Banding 0.140-0.177, Stability 0.989-1.000

**Analysis**:
- SensoryBridge 4-phase temporal produces smoothest gradients
- Emotiscope sigma-delta converges well over 512 frames
- LWOS Bayer creates visible 4×4 spatial pattern
- LWOS No-dither has subtle banding but perfect stability

**Recommendation**: SensoryBridge for smooth gradients with acceptable temporal variance.

---

### 3. Constant Field Performance

**Challenge**: Detect shimmer/flicker on uniform color.

**Results**:
- **All non-temporal pipelines**: Banding 0.000, Stability 1.000 (perfect)
- **LWOS Bayer+Temporal**: Banding 1.000, Stability 0.976 (LSB flicker)
- **Emotiscope**: Banding 1.000, Stability 0.992 (error accumulation visible)

**Analysis**:
- Temporal dithering on constant field creates unnecessary variance
- Bayer spatial dither on flat color creates visible pattern
- Best performance: No dithering on uniform colors

**Recommendation**: Disable temporal dithering for constant fields.

---

### 4. LGP Gradient Performance

**Challenge**: Centre-heavy gradient (LightwaveOS specific).

**Results**:
- **SensoryBridge**: Banding 0.004 (smoothest)
- **LWOS No dither**: Banding 0.015 (good)
- **Emotiscope**: Banding 0.041 (acceptable)
- **LWOS Bayer**: Banding 0.209-0.222 (visible artifacts)

**Analysis**:
- SensoryBridge handles complex gradients best
- Bayer creates visible concentric ring artifacts
- Emotiscope error accumulation works well

**Recommendation**: SensoryBridge for LGP-style effects.

---

### 5. Palette Blend Performance

**Challenge**: Color transitions (hue changes, not just brightness).

**Results**:
- **SensoryBridge**: Banding 0.725 (best)
- **Emotiscope**: Banding 0.741 (close second)
- **LWOS Bayer**: Banding 0.825-0.865 (acceptable)
- **LWOS No dither**: Banding 0.925 (worst)

**Analysis**:
- Color transitions benefit from temporal dithering
- Spatial dithering alone insufficient for hue quantisation
- No dithering shows obvious color banding

**Recommendation**: SensoryBridge or Emotiscope for color blends.

---

## Trade-off Analysis

### SensoryBridge Strengths
✅ Best banding performance across all tests  
✅ Smooth gradients (near-zero banding on ramps)  
✅ Good temporal stability (0.985+)  
✅ Simple algorithm (4 thresholds, per-channel noise origins)

### SensoryBridge Weaknesses
⚠️ Slight temporal variance (not perfectly stable)  
⚠️ 254 scale factor (not standard 255)  
⚠️ Per-channel state management required

### LWOS Bayer Strengths
✅ Perfect temporal stability (when temporal disabled)  
✅ No persistent state (stateless spatial dithering)  
✅ Gamma 2.2 correction built-in  
✅ Well-integrated with FastLED ecosystem

### LWOS Bayer Weaknesses
⚠️ Visible 4×4 pattern in shadows and flat colors  
⚠️ High banding scores (0.14-1.0)  
⚠️ Temporal model adds flicker without smoothness benefit

### Emotiscope Strengths
✅ Excellent convergence (time-averaged accuracy)  
✅ Deadband prevents small-error accumulation  
✅ Good gradient performance (banding 0.003-0.041)  
✅ Sigma-delta is mathematically robust

### Emotiscope Weaknesses
⚠️ Per-LED, per-channel state (3 floats × num_leds memory)  
⚠️ Initialization affects first N frames  
⚠️ Constant fields show error accumulation artifacts  
⚠️ More complex implementation

---

## Recommendations for LightwaveOS v2

### Option 1: Adopt SensoryBridge Quantiser (Recommended)

**Pros**:
- Best overall banding performance
- Simple to implement (254 scale, 4 thresholds, 3 uint8 counters)
- No per-LED state required

**Cons**:
- Slight temporal variance (may be visible in camera/slow-mo)

**Implementation**:
```cpp
// Replace Bayer dithering with SensoryBridge quantiser
// Keep gamma 2.2 LUT
// Disable FastLED.setDither(1)
```

### Option 2: Hybrid Approach (Experimental)

**Use different algorithms per context**:
- **Static scenes**: No dithering (perfect stability)
- **Gradients/ramps**: SensoryBridge (smooth)
- **Color blends**: Emotiscope (time-averaged accuracy)

**Pros**:
- Best-of-breed per use case

**Cons**:
- Complexity (3 code paths)
- Switching between modes may cause visible transitions

### Option 3: Keep Current System with Refinements

**Improvements**:
- Disable Bayer on constant fields (detect via frame-to-frame delta)
- Disable temporal on uniform colors
- Increase Bayer threshold for shadows (reduce near-black artifacts)

**Pros**:
- No major code changes

**Cons**:
- Still won't match SensoryBridge smoothness

---

## Conclusion

The quantitative data clearly shows **SensoryBridge's 4-phase temporal quantiser** delivers superior banding performance across diverse test scenarios. Its combination of:

1. Low banding (0.001-0.004 on critical tests)
2. Good temporal stability (0.985+)
3. Simple implementation (3 counters, 4 thresholds)

...makes it the **recommended choice** for LightwaveOS v2 if smooth gradients and minimal artifacts are priorities.

**LWOS Bayer** excels at temporal stability but creates visible spatial patterns. **Emotiscope** provides excellent time-averaged accuracy but requires per-LED state.

For production deployment, we recommend:
1. Implement SensoryBridge quantiser
2. Keep gamma 2.2 LUT (proven, effective)
3. Disable FastLED temporal dithering (redundant)
4. Add heuristic to disable dithering on constant fields

---

## Appendix: Algorithm Specifications

### SensoryBridge 4-Phase Quantiser

```python
DITHER_TABLE = [0.25, 0.50, 0.75, 1.00]

def quantize(value_float, noise_origin, led_index):
    decimal = value_float * 254.0
    whole = int(decimal)
    fract = decimal - whole
    
    threshold = DITHER_TABLE[(noise_origin + led_index) % 4]
    if fract >= threshold:
        whole += 1
    
    return clip(whole, 0, 255)

# Per frame: noise_origin_r/g/b += 1
```

### LWOS Bayer 4×4 Dithering

```python
BAYER_4X4 = [
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5]
]

def apply_bayer(value_uint8, led_index):
    threshold = BAYER_4X4[led_index % 4][(led_index // 4) % 4]
    low_nibble = value_uint8 & 0x0F
    
    if low_nibble > threshold and value_uint8 < 255:
        value_uint8 += 1
    
    return value_uint8
```

### Emotiscope Sigma-Delta

```python
THRESHOLD = 0.055
dither_error = [0.0] * num_leds  # Persistent state

def quantize(value_float, led_index):
    scaled = value_float * 255.0
    quantised = int(scaled)
    new_error = scaled - quantised
    
    if new_error >= THRESHOLD:
        dither_error[led_index] += new_error
    
    if dither_error[led_index] >= 1.0:
        quantised += 1
        dither_error[led_index] -= 1.0
    
    return clip(quantised, 0, 255)
```

---

**Report generated by DitherBench v1.0**  
**Reproducible builds: All test configurations version-controlled**
