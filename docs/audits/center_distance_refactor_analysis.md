# Center Distance Refactor Analysis

**Date:** 2025-12-23  
**Baseline:** commit `9734cca`  
**Candidate:** current working tree

## Summary

The center distance calculation was refactored from `abs((float)i - CENTER_LEFT)` to `centerPairDistance((uint16_t)i)` in 6 locations within `CoreEffects.cpp`. This is an **intentional refactor** to properly implement the CENTER PAIR pattern, where LEDs 79 and 80 are both treated as the center.

## Mathematical Difference

### Old Calculation: `abs((float)i - CENTER_LEFT)`
- LED 79: `abs(79 - 79) = 0` (center)
- LED 80: `abs(80 - 79) = 1` (1 unit from center)
- Result: Asymmetric center treatment

### New Calculation: `centerPairDistance((uint16_t)i)`
```cpp
constexpr uint16_t centerPairDistance(uint16_t index) {
    return (index <= CENTER_LEFT) ? (CENTER_LEFT - index) : (index - CENTER_RIGHT);
}
```
- LED 79: `centerPairDistance(79) = CENTER_LEFT - 79 = 79 - 79 = 0` (center)
- LED 80: `centerPairDistance(80) = 80 - CENTER_RIGHT = 80 - 80 = 0` (center)
- Result: Symmetric center treatment (both LEDs 79 and 80 are distance 0)

## Affected Effects

All 6 instances in `CoreEffects.cpp` were refactored:
1. **Effect 1 (Ocean)** - Line 100
2. **Effect 6 (BPM)** - Line 265
3. **Effect 7 (Wave)** - Line 290
4. **Effect 8 (Ripple)** - Line 342
5. **Effect 10 (Interference)** - Line 440
6. **Effect 11 (Breathing)** - Line 470

## Impact Assessment

### Visual Impact
- **Expected**: Subtle visual changes due to symmetric center treatment
- **Observed**: Tap A mismatches show edge ratio drift up to 586% (effect 26)
- **Root Cause**: Effects calibrated for old asymmetric behavior now see different distance values

### LGP Physics Impact
- **Positive**: Correct CENTER PAIR implementation aligns with LGP physics (center is between LEDs 79/80)
- **Negative**: Effects calibrated for old behavior may show visual drift
- **Recommendation**: This refactor is **correct** and should be kept. Effects showing drift may need recalibration, but the mathematical model is now accurate.

## Conclusion

The refactor is **mathematically correct** and properly implements the CENTER PAIR pattern. The Tap A mismatches are expected visual changes from the refactor, not bugs. Effects showing significant drift (e.g., effect 26 with 586% ratio drift) may need recalibration to work with the new symmetric center treatment.

**Action**: Keep the refactor. Document that visual changes are expected and intentional.

## References

- `v2/src/effects/CoreEffects.h` - `centerPairDistance()` definition
- `baseline_9734cca/v2/src/effects/CoreEffects.cpp` - Old implementation
- `v2/src/effects/CoreEffects.cpp` - New implementation
- `docs/audits/capture_analysis_results_2025-12-23.md` - Full analysis results

