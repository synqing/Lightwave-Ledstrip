# Capture Data Analysis Results - Critical Findings

**Date:** 2025-12-23  
**Baseline:** commit `9734cca` (pre-ColorCorrectionEngine)  
**Candidate:** current working tree (with ColorCorrectionEngine)  
**Analysis Method:** LGP-first metrics with tap-based root cause attribution

---

## Executive Summary

**CRITICAL FINDING**: The analysis reveals **severe visual regressions** affecting **43 out of 18 tested effects** (some effects tested at multiple taps). The most alarming discovery is that **all top regressions show "Tap A mismatch"**, indicating that **effect logic itself has changed** between baseline and candidate, not just color correction pipeline issues.

### Severity Breakdown

- **43 effect/tap combinations** have LGP risk score > 0.3 (severe)
- **17 effects** show significant regression at Tap B (post-correction)
- **Edge ratio drift** up to **586%** (effect 26, pre_correction)
- **Low-value crush** up to **100%** (multiple effects)
- **PSNR degradation** as low as **9.2 dB** (effect 58, pre_ws2812)

---

## Top 10 Regressions (by LGP Risk Score)

| Rank | Effect | Tap | LGP Risk | Ratio Drift | Crush Score | PSNR | Root Cause |
|------|--------|-----|----------|-------------|-------------|------|------------|
| 1 | 32 | pre_ws2812 | **0.872** | 114.0% | 89.8% | 23.7 dB | **Tap A mismatch** |
| 2 | 65 | pre_ws2812 | **0.869** | 145.1% | 67.4% | 10.8 dB | **Tap A mismatch** |
| 3 | 32 | post_correction | **0.868** | 142.5% | 88.0% | 24.5 dB | **Tap A mismatch** |
| 4 | 66 | pre_ws2812 | **0.802** | 158.3% | 75.9% | 15.7 dB | **Tap A mismatch** |
| 5 | 67 | pre_ws2812 | **0.801** | 83.6% | 83.7% | 15.2 dB | **Tap A mismatch** |
| 6 | 26 | pre_ws2812 | **0.772** | 85.4% | 80.9% | 22.1 dB | **Tap A mismatch** |
| 7 | 13 | pre_correction | **0.736** | 163.7% | 43.6% | 13.1 dB | **Tap A mismatch** |
| 8 | 13 | pre_ws2812 | **0.705** | 76.0% | 61.9% | 14.4 dB | **Tap A mismatch** |
| 9 | 26 | post_correction | **0.656** | 51.7% | 90.9% | 21.5 dB | **Tap A mismatch** |
| 10 | 16 | pre_correction | **0.645** | 87.3% | 37.1% | 13.2 dB | **Tap A mismatch** |

**Critical Observation**: **ALL top 10 regressions are attributed to "Tap A mismatch"**, meaning the effect render logic itself has diverged from baseline, not just the color correction pipeline.

---

## Root Cause Analysis

### Tap A (Pre-Correction) Mismatches

**Finding**: Many effects show significant differences at Tap A, which captures the raw effect output **before** any color correction is applied.

**Implications**:
1. **Effect code has changed** between baseline and candidate
2. Possible causes:
   - Effect implementation modifications
   - Center distance calculation refactor (`abs(i-CENTER_LEFT)` → `centerPairDistance(i)`)
   - Palette handling changes
   - FastLED optimization utilities integration
   - Narrative engine integration

**Affected Effects** (Tap A LGP risk > 0.5):
- Effect 13 (LGP-sensitive): 0.736 risk, 163.7% ratio drift
- Effect 16 (LGP-sensitive): 0.645 risk, 87.3% ratio drift
- Effect 26 (LGP-sensitive): 0.600 risk, **586.2% ratio drift**
- Effect 65 (LGP-sensitive): 0.575 risk, 358.1% ratio drift
- Effect 67 (LGP-sensitive): 0.572 risk, 264.6% ratio drift

### Tap B (Post-Correction) Impact

**Finding**: 17 effects show significant regression at Tap B, where `ColorCorrectionEngine::processBuffer()` is applied.

**Key Metrics**:
- Effect 32: 0.868 risk, 142.5% ratio drift, 88.0% crush
- Effect 26: 0.656 risk, 51.7% ratio drift, **90.9% crush**
- Effect 66: 0.584 risk, 27.4% ratio drift, **94.9% crush**
- Effect 67: 0.581 risk, 4.6% ratio drift, **92.1% crush**

**ColorCorrectionEngine Impact**:
- **Low-value crush**: Up to 100% (all low values < 50 crushed to near-zero)
- **Edge ratio drift**: Up to 142.5% (amplitude relationships destroyed)
- **Double-darkening**: White guardrail + gamma correction combine to crush bright colors

### Tap C (Pre-WS2812) Issues

**Finding**: Many effects show additional degradation at Tap C, after strip splitting but before hardware output.

**Possible Causes**:
- FastLED strip handling changes
- Buffer interleaving issues
- Timing/synchronization problems

---

## LGP-Sensitive Effects - Critical Regressions

### Effect 13 (LGP-sensitive)
- **Pre-correction**: 0.736 risk, **163.7% ratio drift** (I₁/I₂ relationship destroyed)
- **Post-correction**: 0.181 risk, 12.75% ratio drift (correction actually *improves* this one)
- **Pre-WS2812**: 0.705 risk, 75.95% ratio drift

### Effect 16 (LGP-sensitive)
- **Pre-correction**: 0.645 risk, 87.3% ratio drift
- **Post-correction**: 0.602 risk, 39.68% ratio drift, **1.062 symmetry deviation**
- **Pre-WS2812**: 0.520 risk, 4.95% ratio drift

### Effect 26 (LGP-sensitive)
- **Pre-correction**: 0.600 risk, **586.2% ratio drift** (MASSIVE)
- **Post-correction**: 0.656 risk, 51.74% ratio drift, **90.9% crush**
- **Pre-WS2812**: 0.772 risk, 85.38% ratio drift

### Effect 32 (LGP-sensitive)
- **Pre-correction**: 0.231 risk, 13.40% ratio drift
- **Post-correction**: 0.868 risk, **142.5% ratio drift**, 88.0% crush
- **Pre-WS2812**: **0.872 risk** (highest overall), 114.05% ratio drift, 89.8% crush

### Effect 65 (LGP-sensitive) - Chromatic Lens
- **Pre-correction**: 0.575 risk, **358.1% ratio drift**
- **Post-correction**: 0.124 risk, 50.95% ratio drift, 67.2% crush
- **Pre-WS2812**: **0.869 risk**, 145.12% ratio drift, 67.4% crush

### Effect 66 (LGP-sensitive) - Chromatic Pulse
- **Pre-correction**: 0.751 risk, 57.26% ratio drift
- **Post-correction**: 0.584 risk, 27.41% ratio drift, **94.9% crush**
- **Pre-WS2812**: 0.802 risk, **158.34% ratio drift**, 75.9% crush

### Effect 67 (LGP-sensitive) - Chromatic Interference
- **Pre-correction**: 0.572 risk, **264.6% ratio drift**
- **Post-correction**: 0.581 risk, 4.59% ratio drift, **92.1% crush**
- **Pre-WS2812**: 0.801 risk, 83.60% ratio drift, 83.7% crush

---

## Stateful Effects - Buffer Feedback Corruption

### Effect 3 (Confetti) - Stateful
- **Pre-correction**: 0.425 risk, **100.0% crush** (all low values destroyed)
- **Post-correction**: 0.157 risk, 99.1% crush
- **Pre-WS2812**: 0.127 risk, 83.6% crush

**Issue**: Confetti relies on reading previous frame state. ColorCorrectionEngine mutates `m_leds` in-place, corrupting the feedback loop.

### Effect 8 (Ripple) - Stateful
- **Pre-correction**: 0.514 risk, 92.9% crush
- **Post-correction**: 0.119 risk, 95.5% crush
- **Pre-WS2812**: 0.558 risk, **100.0% crush**

**Issue**: Same buffer-feedback corruption as Confetti.

---

## Key Metrics Summary

### Edge Ratio Drift (I₁/I₂ Preservation)
- **Worst**: Effect 26 pre_correction: **586.2%** drift
- **Critical threshold**: > 5% indicates LGP interference pattern destruction
- **Affected**: 12+ effects with > 50% drift

### Low-Value Crush Score
- **Worst**: Multiple effects at **100%** (all low values crushed)
- **Critical threshold**: > 10% indicates trail/gradient destruction
- **Affected**: 20+ effects with > 50% crush

### PSNR (Image Quality)
- **Worst**: Effect 58 pre_ws2812: **9.2 dB** (extremely poor)
- **Acceptable**: > 30 dB
- **Affected**: 15+ effects with < 20 dB

---

## Recommendations (Prioritized)

### 1. **IMMEDIATE: Investigate Tap A Mismatches** (CRITICAL)

**All top regressions show "Tap A mismatch"**, meaning effect code has changed. This is **more severe** than color correction issues because it affects the fundamental pattern generation.

**Action Items**:
- Compare effect implementations between baseline and candidate
- Check if center distance refactor (`centerPairDistance`) changed behavior
- Verify palette handling hasn't changed
- Review FastLED optimization utilities integration
- Check narrative engine integration impact

**Files to Review**:
- `v2/src/effects/CoreEffects.cpp` (center distance refactor)
- `v2/src/effects/LGPChromaticEffects.cpp` (new chromatic effects)
- `v2/src/effects/utils/FastLEDOptim.h` (optimization utilities)

### 2. **HIGH: Disable ColorCorrectionEngine for LGP-Sensitive Effects**

**Finding**: ColorCorrectionEngine destroys amplitude relationships critical for LGP interference patterns.

**Implementation**:
```cpp
// In RendererActor::onTick()
bool shouldApplyCorrection = true;

// Check if current effect is LGP-sensitive
uint8_t currentEffect = m_currentEffect;
if (PatternRegistry::isLGPSensitive(currentEffect)) {
    shouldApplyCorrection = false;
}

if (shouldApplyCorrection) {
    ColorCorrectionEngine::getInstance().processBuffer(m_leds, LedConfig::TOTAL_LEDS);
}
```

**Effects to Exclude**: 10, 13, 16, 26, 32, 65, 66, 67 (Interference/Chromatic families)

### 3. **HIGH: Fix Buffer-Feedback State Corruption**

**Finding**: In-place mutation of `m_leds` breaks stateful effects (Confetti, Ripple).

**Implementation Options**:
- **Option A**: Copy buffer before correction (memory overhead: 960 bytes)
- **Option B**: Disable correction for stateful effects (simpler)
- **Option C**: Make effects read from a separate "previous frame" buffer

**Effects to Protect**: 3 (Confetti), 8 (Ripple)

### 4. **MEDIUM: Simplify ColorCorrectionEngine**

**Current Issues**:
- Double-darkening (white guardrail + gamma)
- Gamma crushing low values (60-90% reduction)
- Per-pixel guardrails break amplitude ratios

**Proposed Changes**:
- Remove per-pixel guardrails (white/brown)
- Use only uniform scaling (auto-exposure style)
- Apply very soft gamma (1.5) or disable for values < 50
- Make correction LGP-aware (preserve I₁/I₂ ratios)

### 5. **LOW: Investigate Tap C Degradation**

**Finding**: Additional degradation at Tap C suggests FastLED/strip handling issues.

**Investigation**:
- Check strip buffer interleaving
- Verify timing/synchronization
- Review FastLED.show() integration

---

## Visual Artifacts

Visual comparisons have been generated for all effects with LGP risk > 0.1:
- **Location**: `reports/colour_testbed/2025-12-23/effect_<id>/`
- **Contents**: Side-by-side renderings, difference heatmaps
- **Note**: Transfer curve plots had minor generation issues (non-critical)

---

## Next Steps

1. **Review visual artifacts** in `reports/colour_testbed/2025-12-23/` to confirm findings
2. **Investigate Tap A mismatches** - compare effect code between baseline and candidate
3. **Implement opt-out mechanism** for LGP-sensitive and stateful effects
4. **Re-capture and verify** after fixes are applied
5. **Document effect code changes** that caused Tap A mismatches

---

## References

- **Full Report**: `reports/colour_testbed/2025-12-23/analysis_report.md`
- **Visual Artifacts**: `reports/colour_testbed/2025-12-23/effect_<id>/`
- **Baseline Commit**: `9734cca1ee7dbbe4e463dc912e3c9ac2b9d1152e`
- **Previous Analysis**: `docs/audits/color_processing_algorithm_analysis_2025-01-22.md`
- **LGP Physics Analysis**: `docs/audits/lgp_physics_aware_color_processing_analysis_2025-01-22.md`

---

**Status**: Analysis complete. Critical regressions identified. Immediate action required on Tap A mismatches and ColorCorrectionEngine impact.

