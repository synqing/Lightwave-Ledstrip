# Next-Stage Color Pipeline Simplification Plan

**Date:** 2025-12-23  
**Status:** Pending Analysis Results (Baseline Data Required)  
**Based on:** Capture analysis findings and LGP physics requirements

## Overview

This document outlines the recommended approach for simplifying and making LGP-safe the `ColorCorrectionEngine` based on regression analysis results. The plan will be finalized once baseline vs candidate comparison data is available.

## Current System Analysis

### ColorCorrectionEngine Architecture

**Location**: `v2/src/effects/enhancement/ColorCorrectionEngine.cpp`

**Current Pipeline** (in `processBuffer()`):
1. Auto-exposure (BT.601 luminance-based scaling)
2. White guardrail (HSV saturation boost / RGB white reduction)
3. Brown guardrail (HSV saturation boost)
4. Gamma correction (2.2, via LUT)

**Application Point**: `RendererActor::onTick()` line 262
- **CRITICAL**: Mutates `m_leds` buffer in-place
- **CRITICAL**: Applied after `renderFrame()` but before stateful effects read previous frame

### Known Issues (from Previous Analysis)

1. **Double-darkening**: White guardrail + gamma combine to crush bright colors by 75-80%
2. **Gamma crushing low values**: Values < 100 reduced by 60-90%, making subtle trails invisible
3. **Amplitude relationship destruction**: Gamma and guardrails break I₁/I₂ ratios critical for LGP interference
4. **State corruption**: In-place mutation breaks buffer-feedback effects (Confetti, Ripple)

## Recommended Strategies

### Strategy 1: Effect-Specific Opt-Out (Recommended)

**Approach**: Disable buffer correction for LGP-sensitive and stateful effects.

**Implementation**:
```cpp
// In RendererActor::onTick()
bool shouldApplyCorrection = true;

// Check if current effect is LGP-sensitive or stateful
uint8_t currentEffect = m_currentEffectId;
if (PatternRegistry::isLGPSensitive(currentEffect) || 
    PatternRegistry::isStateful(currentEffect)) {
    shouldApplyCorrection = false;
}

if (shouldApplyCorrection) {
    ColorCorrectionEngine::getInstance().processBuffer(m_leds, LedConfig::TOTAL_LEDS);
}
```

**PatternRegistry Extensions Needed**:
- `bool isLGPSensitive(uint8_t effectId)` - Check family/tags
- `bool isStateful(uint8_t effectId)` - Check for buffer-feedback

**Pros**:
- Preserves LGP physics for interference patterns
- Prevents state corruption for buffer-feedback effects
- Minimal code changes
- Easy to verify (effect-specific)

**Cons**:
- Some effects get correction, others don't (inconsistent)
- Requires maintaining opt-out list

### Strategy 2: LGP-Safe Uniform Correction

**Approach**: Simplify correction to only uniform transforms that preserve amplitude relationships.

**Implementation**:
```cpp
// Simplified processBuffer() - uniform scaling only
void ColorCorrectionEngine::processBufferLGPSafe(CRGB* leds, uint16_t numLeds) {
    if (m_config.mode == CorrectionMode::DISABLED) return;
    
    // Only auto-exposure (uniform scaling)
    if (m_config.autoExposureEnabled) {
        applyAutoExposure(leds, numLeds);
    }
    
    // Very soft gamma (or disabled for low values)
    if (m_config.gammaEnabled) {
        applySoftGamma(leds, numLeds, gamma=1.5, lowValueThreshold=50);
    }
    
    // NO guardrails (they break amplitude ratios)
}
```

**Pros**:
- Consistent across all effects
- Preserves amplitude relationships (uniform scaling)
- Still provides some correction benefits

**Cons**:
- Less aggressive correction (may not address white/brown issues)
- Requires careful gamma tuning

### Strategy 3: Output-Only Buffer (Hybrid)

**Approach**: Apply correction to a copy, not the live state buffer.

**Implementation**:
```cpp
// In RendererActor::onTick()
CRGB correctedLeds[LedConfig::TOTAL_LEDS];
memcpy(correctedLeds, m_leds, sizeof(CRGB) * LedConfig::TOTAL_LEDS);

// Apply correction to copy
ColorCorrectionEngine::getInstance().processBuffer(correctedLeds, LedConfig::TOTAL_LEDS);

// Use corrected buffer for output only
showLeds(correctedLeds);  // Instead of showLeds(m_leds)
```

**Pros**:
- Prevents state corruption completely
- Allows full correction pipeline
- Clean separation of concerns

**Cons**:
- Memory overhead (320 × 3 bytes = 960 bytes)
- Still doesn't solve LGP amplitude relationship issues
- Requires refactoring `showLeds()`

## Decision Matrix

| Strategy | LGP Fidelity | State Safety | Code Complexity | Performance | Recommendation |
|----------|--------------|--------------|------------------|-------------|---------------|
| Opt-Out | ✅✅✅ | ✅✅✅ | Low | ✅✅✅ | **Best for LGP** |
| Uniform Safe | ✅✅ | ✅✅ | Medium | ✅✅✅ | Good compromise |
| Output Buffer | ✅ | ✅✅✅ | Medium | ✅✅ | Good for state |

## Implementation Plan (Once Analysis Complete)

### Phase 1: Analysis Review
- [ ] Review regression scores from capture analysis
- [ ] Identify which effects are most affected
- [ ] Determine if Tap A (pre-correction) matches baseline (effect logic regression)
- [ ] Quantify Tap B (post-correction) divergence

### Phase 2: Strategy Selection
- [ ] Choose primary strategy based on analysis results
- [ ] Document decision rationale
- [ ] Define success criteria

### Phase 3: Implementation
- [ ] Implement chosen strategy
- [ ] Add PatternRegistry helper methods (if opt-out)
- [ ] Update RendererActor::onTick()
- [ ] Add configuration options (if needed)

### Phase 4: Verification
- [ ] Re-capture candidate data with new implementation
- [ ] Compare against baseline
- [ ] Verify LGP risk scores improve
- [ ] Visual inspection of affected effects

### Phase 5: Documentation
- [ ] Update ColorCorrectionEngine documentation
- [ ] Document opt-out list (if applicable)
- [ ] Add comments explaining LGP-safe design decisions

## Files to Modify

1. **`v2/src/core/actors/RendererActor.cpp`**
   - Line 262: Conditional correction application
   - Or: Copy buffer before correction

2. **`v2/src/effects/enhancement/ColorCorrectionEngine.cpp`**
   - Add `processBufferLGPSafe()` method (if Strategy 2)
   - Or: Modify `processBuffer()` to respect opt-out flag

3. **`v2/src/effects/PatternRegistry.cpp`** (if Strategy 1)
   - Add `isLGPSensitive(uint8_t effectId)`
   - Add `isStateful(uint8_t effectId)`

4. **`v2/src/effects/PatternRegistry.h`** (if Strategy 1)
   - Add method declarations

## Success Criteria

1. **LGP Risk Score**: < 0.1 for all LGP-sensitive effects
2. **Edge Ratio Drift**: < 5% for interference patterns
3. **Low-Value Crush**: < 10% for trail/gradient effects
4. **State Preservation**: Buffer-feedback effects work correctly
5. **Visual Inspection**: No obvious darkening or color distortion

## References

- `docs/audits/color_processing_algorithm_analysis_2025-01-22.md` - Detailed algorithm analysis
- `docs/audits/lgp_physics_aware_color_processing_analysis_2025-01-22.md` - LGP physics requirements
- `docs/audits/visual_regression_audit_2025-01-22.md` - Initial regression findings

---

**Status**: Awaiting baseline capture data and regression analysis results to finalize strategy selection.

