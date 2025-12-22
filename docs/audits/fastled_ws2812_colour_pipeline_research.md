# FastLED & WS2812 Colour Pipeline Research

**Date:** 2025-01-22  
**Purpose:** Document FastLED library capabilities, WS2812 rendering characteristics, and critical colour profile preservation requirements for LGP interference physics

---

## Executive Summary

This document provides comprehensive research on:
1. **FastLED library** colour processing pipeline (setBrightness, setCorrection, setDither)
2. **WS2812 LED** rendering characteristics and limitations
3. **LGP-specific requirements** for preserving interference physics
4. **Critical constraints** for colour correction implementation

**Key Finding:** FastLED applies its own colour transformations (brightness scaling, correction, dithering) **after** our code writes to CRGB buffers. Any colour correction we apply must account for these downstream transformations to avoid double-processing or breaking LGP interference physics.

---

## 1. FastLED Library Pipeline

### 1.1 FastLED Initialization (Current Configuration)

**Location:** `v2/src/core/actors/RendererActor.cpp:320-325`

```cpp
FastLED.setBrightness(m_brightness);           // Global brightness scaling
FastLED.setCorrection(TypicalLEDStrip);        // Per-channel colour correction
FastLED.setDither(1);                          // Temporal dithering enabled
FastLED.setMaxRefreshRate(0, true);            // Non-blocking mode
FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000);  // Power limiting
```

### 1.2 FastLED.setBrightness()

**Function:** Global brightness scaling applied to all LEDs before transmission to WS2812.

**Behavior:**
- Scales all RGB channels uniformly by `brightness / 255`
- Applied **after** our code writes to CRGB buffers
- Uses `nscale8_video()` internally (video-optimized scaling, preserves colour relationships)
- **Critical:** This is a **multiplicative** transform, not additive

**Example:**
```cpp
// Our code writes: CRGB(200, 150, 100)
// FastLED.setBrightness(128) → scales by 128/255 = 0.502
// Result sent to WS2812: CRGB(100, 75, 50)
```

**Impact on LGP:**
- Preserves **amplitude ratios** between edges (uniform scaling)
- Safe for interference physics (I₁/I₂ ratio unchanged)
- BUT: Reduces absolute amplitudes, which may affect interference intensity thresholds

### 1.3 FastLED.setCorrection(TypicalLEDStrip)

**Function:** Per-channel colour correction to compensate for LED colour temperature and efficiency differences.

**Behavior:**
- Applies per-channel scaling factors (R, G, B multipliers)
- `TypicalLEDStrip` correction values (from FastLED library):
  - Red: ~255 (no correction)
  - Green: ~240 (slight reduction)
  - Blue: ~235 (slight reduction)
- Applied **after** brightness scaling
- **Critical:** This is **per-channel**, not uniform

**Example:**
```cpp
// After brightness scaling: CRGB(100, 75, 50)
// TypicalLEDStrip correction:
//   R: 100 * 255/255 = 100
//   G: 75 * 240/255 = 71
//   B: 50 * 235/255 = 46
// Result: CRGB(100, 71, 46)
```

**Impact on LGP:**
- **Breaks amplitude relationships** if applied to interference-dependent effects
- Green/Blue reduction changes I₁/I₂ ratios if edges have different colour content
- May cause interference patterns to shift

**Recommendation:** For LGP effects, consider using `UncorrectedColor` or custom correction that preserves edge balance.

### 1.4 FastLED.setDither(1)

**Function:** Temporal dithering to reduce 8-bit quantization artefacts.

**Behavior:**
- Adds frame-to-frame noise to smooth out low-amplitude gradients
- Alternates between rounding up/down for values between quantization levels
- **Critical:** Introduces **temporal variation** in pixel values

**Example:**
```cpp
// Frame N: CRGB(100, 75, 50)
// Frame N+1: CRGB(101, 74, 51)  // Dithering adds ±1 variation
// Frame N+2: CRGB(100, 75, 50)  // Back to original
```

**Impact on LGP:**
- **Breaks temporal stability** for stateful effects (Confetti, Ripple, etc.)
- Frame-to-frame variation affects interference patterns
- May cause "flickering" in interference zones
- **Critical for capture:** Captures must account for dithering noise

**Recommendation:** For testbed captures, consider disabling dithering (`setDither(0)`) to get deterministic output, or capture multiple frames and average.

### 1.5 FastLED.show()

**Function:** Transmits CRGB buffer data to WS2812 LEDs via RMT (ESP32) or bit-banging.

**Behavior:**
- Copies data from CRGB buffers to WS2812 protocol
- Non-blocking mode (`setMaxRefreshRate(0, true)`) allows concurrent processing
- **Critical:** This is the **final stage** - values here are what actually hit the LEDs

**Pipeline Order:**
```
Our Code → CRGB buffer → setBrightness() → setCorrection() → setDither() → show() → WS2812
```

---

## 2. WS2812 LED Characteristics

### 2.1 8-Bit PWM Limitations

**WS2812 uses 8-bit PWM per channel:**
- 256 discrete levels per channel (0-255)
- **Quantization artefacts** at low values
- **Dead zones** below ~5-10 (LEDs may not respond to very low values)

**Impact:**
- Low-amplitude trails (values 1-10) may be invisible or flicker
- Gamma correction that crushes values below 10 makes trails disappear
- **Critical for LGP:** Interference patterns at low amplitudes may be lost

### 2.2 Colour Temperature & Efficiency

**WS2812 LEDs have:**
- Different efficiency per channel (Red typically brighter than Green/Blue)
- Colour temperature shift at different brightness levels
- Non-linear response (perceived brightness vs. PWM duty cycle)

**Why FastLED.setCorrection() exists:**
- Compensates for efficiency differences
- Attempts to achieve "white balance" at full brightness
- **BUT:** Correction values are tuned for "typical" strips, may not match our hardware

### 2.3 Temporal Response

**WS2812 update characteristics:**
- Update time: ~30μs per LED (320 LEDs = ~9.6ms total)
- Refresh rate: Limited by update time + data transmission
- **No frame buffering** - values sent immediately

**Impact:**
- FastLED.setDither() must work within single-frame constraints
- Frame-to-frame variation is visible (no persistence)
- **Critical for capture:** Each frame is independent, no temporal smoothing

---

## 3. Current Firmware Pipeline

### 3.1 Render Pipeline (Current)

**Location:** `v2/src/core/actors/RendererActor.cpp:254-265`

```cpp
void RendererActor::onTick() {
    renderFrame();  // Effect writes to m_leds[320]
    
    // ⚠️ ColorCorrectionEngine modifies m_leds IN-PLACE
    ColorCorrectionEngine::getInstance().processBuffer(m_leds, 320);
    
    showLeds();  // Copies to m_strip1[160], m_strip2[160], then FastLED.show()
}
```

**Pipeline Stages:**

1. **Effect Render** (`renderFrame()`)
   - Effects write to `m_leds[320]` (unified buffer)
   - Values: 0-255 per channel (linear RGB)

2. **ColorCorrectionEngine** (`processBuffer()`)
   - Modifies `m_leds` IN-PLACE
   - Applies: Auto-exposure → White guardrail → Brown guardrail → Gamma
   - **Problem:** Mutates effect state for next frame

3. **Strip Splitting** (`showLeds()`)
   - Copies `m_leds[0..159]` → `m_strip1[160]`
   - Copies `m_leds[160..319]` → `m_strip2[160]`

4. **FastLED Processing** (`FastLED.show()`)
   - Applies `setBrightness()` scaling
   - Applies `setCorrection()` per-channel correction
   - Applies `setDither()` temporal dithering
   - Transmits to WS2812 via RMT

### 3.2 Critical Tap Points for Capture

**For testbed, we need to capture at three stages:**

1. **Tap A (Pre-Correction):** After `renderFrame()`, before `processBuffer()`
   - Captures: Raw effect output
   - Purpose: Verify effect logic integrity (baseline comparison)

2. **Tap B (Post-Correction):** After `processBuffer()`, before `showLeds()`
   - Captures: ColorCorrectionEngine output
   - Purpose: Measure correction impact

3. **Tap C (Pre-WS2812):** After `showLeds()` copy, before `FastLED.show()`
   - Captures: Per-strip buffers after FastLED processing
   - Purpose: Measure final values sent to hardware

**Note:** FastLED's internal processing (brightness/correction/dither) happens **inside** `FastLED.show()`, so we cannot directly capture "pre-WS2812" values without modifying FastLED library. However, we can:
- Capture `m_strip1` and `m_strip2` after copy (pre-FastLED processing)
- Calculate expected FastLED output based on known correction values
- Or: Temporarily disable FastLED corrections for deterministic capture

---

## 4. LGP-Specific Colour Profile Requirements

### 4.1 Interference Physics Dependencies

**From:** `docs/analysis/b1. LGP_OPTICAL_PHYSICS_REFERENCE.md:224-234`

**Interference Equation:**
```
I_total = I₁ + I₂ + 2√(I₁ × I₂) × cos(Δφ)
```

**Where:**
- I₁, I₂ = amplitudes from edges 1 and 2 (LED brightness values)
- Δφ = phase difference between waves

**Critical Requirements:**

1. **Amplitude Ratio Preservation**
   - I₁/I₂ ratio must be preserved through colour pipeline
   - Any transform that changes I₁ differently than I₂ breaks interference
   - **Example:** Per-channel correction (setCorrection) breaks this if edges have different colours

2. **Low-Amplitude Structure Preservation**
   - Trails/gradients at low values (10-50) must remain visible
   - Gamma correction that crushes values below 10 makes trails disappear
   - **Critical:** LGP interference patterns depend on low-amplitude structure

3. **Edge Balance Preservation**
   - If effects set I₁ = I₂ (balanced), pipeline must preserve this
   - Any transform that reduces one edge differently breaks balance
   - **Example:** White guardrail detecting "whitish" on one edge but not the other

### 4.2 Observed LGP Patterns

**From:** `docs/features/202. LGP.Interference.Discovery.md`

**Observed Phenomena:**
- 6-8 box patterns (standing waves)
- Box boundaries at interference nulls
- Colour waves converging at centre

**Implication:**
- Effects were **calibrated** for specific brightness ranges
- Changing these ranges (via gamma/guardrails) shifts box patterns
- **Critical:** Original system (pre-ColorCorrectionEngine) was correctly tuned

### 4.3 FastLED Correction Impact on LGP

**Problem:** `FastLED.setCorrection(TypicalLEDStrip)` applies per-channel scaling:
- Red: 255/255 (no change)
- Green: 240/255 (6% reduction)
- Blue: 235/255 (8% reduction)

**Impact on Interference:**
- If edges have different colour content, correction breaks I₁/I₂ balance
- **Example:** Edge 1 has more green → gets reduced more → I₁/I₂ ratio changes
- Interference patterns shift

**Recommendation:**
- For LGP effects, use `UncorrectedColor` or custom correction that preserves edge balance
- Or: Apply correction **before** interference calculation (in effects), not after

---

## 5. Colour Correction Best Practices

### 5.1 Gamma Correction

**Purpose:** Compensate for human eye's non-linear brightness perception.

**Standard Gamma:** 2.2 (sRGB standard)

**Problem for LGP:**
- Gamma 2.2 crushes low values (50 → 7, 86% darker)
- Makes trails invisible
- Breaks interference patterns at low amplitudes

**Recommendation:**
- Use **softer gamma** (1.8-2.0) for LGP
- Or: Apply gamma **only to output** (not effect state)
- Or: Use **thresholded gamma** (linear below threshold, gamma above)

### 5.2 White Guardrail

**Purpose:** Reduce "whitish" colours in WHITE_HEAVY palettes.

**Problem for LGP:**
- Per-pixel detection can break edge balance
- If one edge has "whitish" colours and other doesn't, balance broken
- Interference patterns shift

**Recommendation:**
- Apply **only at palette load time** (not per-frame)
- Or: Apply **uniformly** to both edges (preserve balance)
- Or: **Disable for LGP effects**

### 5.3 Auto-Exposure

**Purpose:** Prevent over-bright frames.

**Behavior:**
- Calculates average luma
- Scales down if above target (uniform scaling)
- **Preserves amplitude ratios** (safe for LGP)

**Recommendation:**
- **Safe for LGP** (preserves I₁/I₂ ratios)
- Can be enabled if needed

### 5.4 Brown Guardrail

**Purpose:** Prevent muddy warm tones.

**Behavior:**
- Only affects brownish colours (R > G >= B)
- Clamps G/B relative to R
- **Less likely to affect interference** (specific colour range)

**Recommendation:**
- **Potentially safe** for LGP (needs testing)
- Less aggressive than white guardrail

---

## 6. Critical Constraints for Testbed

### 6.1 Capture Requirements

1. **Deterministic Output**
   - Disable dithering (`setDither(0)`) for reproducible captures
   - Or: Capture multiple frames and average

2. **Three Tap Points**
   - Tap A: Pre-correction (effect output)
   - Tap B: Post-correction (ColorCorrectionEngine output)
   - Tap C: Pre-WS2812 (after strip split, before FastLED.show())

3. **FastLED Correction Handling**
   - Option A: Disable FastLED corrections for capture (deterministic)
   - Option B: Capture pre-FastLED, calculate expected output
   - Option C: Capture actual WS2812 output (requires hardware modification)

### 6.2 Comparison Requirements

1. **Baseline Reference**
   - Use commit `9734cca1...` (pre-ColorCorrectionEngine) as baseline
   - Capture same effects, same settings, same frame indices

2. **Metrics Required**
   - Per-pixel delta (L1/L2, max error)
   - Edge amplitude ratios (I₁/I₂)
   - Low-value preservation (trails visibility)
   - Temporal stability (frame-to-frame variation)

3. **Visual Comparisons**
   - Side-by-side strip renderings
   - Difference heatmaps
   - Time-series plots (ratios, luma)

---

## 7. Recommendations for Simplified ColorCorrectionEngine

### 7.1 Output-Only Processing

**Principle:** Never mutate effect state used for next-frame feedback.

**Implementation:**
```cpp
void RendererActor::onTick() {
    renderFrame();  // Effect writes to m_leds
    
    // Copy to output buffer for correction
    CRGB outputBuffer[320];
    memcpy(outputBuffer, m_leds, sizeof(m_leds));
    
    // Apply correction to output (not live state)
    ColorCorrectionEngine::processBuffer(outputBuffer, 320);
    
    // Copy corrected output to m_leds for showLeds()
    memcpy(m_leds, outputBuffer, sizeof(m_leds));
    showLeds();
}
```

### 7.2 Minimal Correction Stages

**Based on research, recommended stages:**

1. **Optional Auto-Exposure** (uniform scaling, preserves ratios)
   - Safe for LGP
   - Can prevent over-bright frames

2. **Optional Soft Gamma** (1.8-2.0, thresholded)
   - Softer than 2.2 to preserve low values
   - Threshold: linear below 20, gamma above
   - Applied output-only

3. **Palette-Time Correction** (WHITE_HEAVY curation)
   - Applied at palette load, not per-frame
   - Safe for LGP (doesn't affect interference)

**Remove:**
- Per-pixel white guardrail (breaks edge balance)
- Brown guardrail (unless proven necessary)
- Aggressive gamma 2.2 (crushes low values)

### 7.3 LGP-Aware Mode

**Option:** Make ColorCorrectionEngine LGP-aware:

```cpp
void ColorCorrectionEngine::processBuffer(CRGB* buffer, uint16_t count, bool lgpMode = false) {
    if (lgpMode) {
        // LGP mode: Only uniform scaling (preserves ratios)
        if (m_config.autoExposureEnabled) {
            applyAutoExposure(buffer, count);  // Uniform, safe
        }
        // Skip gamma (breaks ratios)
        // Skip guardrails (breaks balance)
    } else {
        // Normal mode: Apply all corrections
        processBuffer(buffer, count);
    }
}
```

---

## 8. Testbed Validation Criteria

### 8.1 Tap A (Pre-Correction)

**Requirement:** Must match baseline exactly (effect logic integrity)

**Metrics:**
- Per-pixel delta: 0 (exact match)
- Edge amplitude ratios: Match baseline
- Temporal stability: Match baseline

### 8.2 Tap B (Post-Correction)

**Requirement:** Allowed to differ, but must meet:

1. **Low-Value Preservation**
   - Values 10-50 must remain visible (not crushed to 0-5)
   - Target: <50% reduction for values 10-50

2. **Edge Ratio Stability**
   - I₁/I₂ ratio drift: <5% from baseline
   - Spatial symmetry: Preserved about centre (79/80)

3. **Temporal Stability**
   - Frame-to-frame variation: <2% (excluding dithering)

### 8.3 Tap C (Pre-WS2812)

**Requirement:** Must be consistent with Tap B plus FastLED's scaling/correction rules

**Metrics:**
- Expected FastLED output matches actual (if corrections disabled)
- Or: Calculated output matches measured (if corrections enabled)

---

## 9. Conclusion

**Key Findings:**

1. **FastLED applies downstream transformations** (brightness, correction, dithering) that we must account for
2. **LGP interference physics requires amplitude ratio preservation** - any transform that breaks I₁/I₂ balance shifts patterns
3. **Low-amplitude structure must be preserved** - gamma 2.2 crushes trails, making interference patterns invisible
4. **Original system was correctly tuned** - effects were calibrated for specific brightness ranges

**Recommendations:**

1. **Simplify ColorCorrectionEngine** to output-only, minimal stages
2. **Preserve amplitude relationships** required for interference
3. **Use softer gamma** (1.8-2.0) or thresholded gamma
4. **Remove per-pixel guardrails** that break edge balance
5. **Make LGP-aware mode** that skips problematic corrections

**Next Steps:**

1. Build testbed with three tap points
2. Capture baseline vs candidate for all effects
3. Measure quantitative metrics
4. Use results to finalize simplified ColorCorrectionEngine

---

**Report Generated:** 2025-01-22  
**Next Review:** After testbed implementation and validation

