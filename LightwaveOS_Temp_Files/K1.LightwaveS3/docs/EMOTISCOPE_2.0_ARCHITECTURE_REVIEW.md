# Emotiscope 2.0 Architecture Review - Critical Differences

**Date**: 2025-01-XX  
**Status**: Critical Issues Identified

## Executive Summary

After comprehensive review of Emotiscope 2.0 reference implementation, **3 critical architectural differences** have been identified that may affect beat tracking accuracy:

1. **MISSING: Silent Bin Suppression** - Emotiscope 2.0 only processes bins with magnitude > 0.005
2. **WRONG: Confidence Calculation Formula** - Different formula than Emotiscope 2.0
3. **MISSING: Phase Sync for All Active Bins** - Emotiscope 2.0 syncs all active bins, v2 only syncs winner

---

## Critical Issue #1: Silent Bin Suppression Missing

### Emotiscope 2.0 Implementation (lines 384-393)

```c
for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI; tempo_bin++) {
    float tempi_magnitude = tempi[tempo_bin].magnitude;

    if(tempi_magnitude > 0.005){
        // Smooth it
        tempi_smooth[tempo_bin] = tempi_smooth[tempo_bin] * 0.975 + (tempi_magnitude) * 0.025;
        tempi_power_sum += tempi_smooth[tempo_bin];

        sync_beat_phase(tempo_bin, delta);
    }
    else{
        tempi_smooth[tempo_bin] *= 0.995;  // Decay, don't add to power_sum
    }
}
```

**Key Behavior**:
- Bins with magnitude > 0.005: Smooth with 0.975/0.025 EMA, add to `tempi_power_sum`, sync phase
- Bins with magnitude <= 0.005: Decay at 0.995, **DO NOT** add to `tempi_power_sum`

### v2 Current Implementation (lines 345-360)

```cpp
for (uint16_t i = 0; i < NUM_TEMPI; i++) {
    float scaled = tempi_[i].magnitude_raw * autoranger;
    scaled = scaled * scaled;
    scaled = scaled * scaled;
    tempi_[i].magnitude = scaled;

    tempi_smooth_[i] = tempi_smooth_[i] * 0.975f +
                       tempi_[i].magnitude * 0.025f;
    power_sum_ += tempi_smooth_[i];  // ❌ ALWAYS adds, even if magnitude < 0.005

    tempi_[i].beat = sinf(tempi_[i].phase);
}
```

**Problem**: All bins are added to `power_sum_`, even silent ones. This dilutes the power sum and affects confidence calculation.

**Impact**: 
- Silent bins contribute noise to power_sum
- Confidence calculation becomes less accurate
- Winner selection may be affected by noise

---

## Critical Issue #2: Wrong Confidence Calculation Formula

### Emotiscope 2.0 Implementation (lines 398-405)

```c
// Measure contribution factor of each tempi, calculate confidence level
float max_contribution = 0.0000001;
for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI; tempo_bin++) {
    max_contribution = fmaxf(
        tempi_smooth[tempo_bin],
        max_contribution
    );
}
tempo_confidence = max_contribution / tempi_power_sum;
```

**Formula**: `confidence = max(tempi_smooth[i]) / tempi_power_sum`

**Meaning**: Ratio of strongest smoothed magnitude to total power sum.

### v2 Current Implementation (lines 362-369)

```cpp
float max_contrib = 0.0f;
if (power_sum_ > 0.01f) {
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        float contrib = (tempi_smooth_[i] / power_sum_) * tempi_[i].magnitude;
        if (contrib > max_contrib) max_contrib = contrib;
    }
}
confidence_ = max_contrib;
```

**Formula**: `confidence = max((tempi_smooth[i] / power_sum) * tempi[i].magnitude)`

**Meaning**: Maximum of (normalized smoothed magnitude × raw magnitude).

**Problem**: This is a **completely different formula** that weights by both smoothed AND raw magnitude, which double-counts the magnitude.

**Impact**:
- Confidence values will be different from Emotiscope 2.0
- Lock threshold (0.3) may not work as intended
- Beat tracking behavior will differ

---

## Critical Issue #3: Phase Sync Only for Winner Bin

### Emotiscope 2.0 Implementation (lines 384-390)

```c
if(tempi_magnitude > 0.005){
    // Smooth it
    tempi_smooth[tempo_bin] = tempi_smooth[tempo_bin] * 0.975 + (tempi_magnitude) * 0.025;
    tempi_power_sum += tempi_smooth[tempo_bin];

    sync_beat_phase(tempo_bin, delta);  // ✅ Syncs ALL active bins
}
```

**Behavior**: All bins with magnitude > 0.005 have their phase synced every frame.

### v2 Current Implementation

Phase sync only happens in `advancePhase()` for the winner bin:

```cpp
void EmotiscopeTempo::advancePhase(float delta_sec) {
    // ...
    // Advance phase at winner's rate
    current_phase_ += tempi_[winner_bin_].phase_radians_per_frame *
                      delta_sec * REFERENCE_FPS;
    // ...
}
```

**Problem**: Only the winner bin's phase is advanced. Other active bins' phases are not synced.

**Impact**:
- Phase information for non-winner bins becomes stale
- If winner changes, phase may jump
- Phase-based beat detection may be less accurate

---

## Additional Differences (Non-Critical)

### 1. Decay Order

**Emotiscope 2.0** (line 264):
```c
shift_array_left(novelty_curve, NOVELTY_HISTORY_LENGTH, 1);
dsps_mulc_f32_ae32(novelty_curve, novelty_curve, NOVELTY_HISTORY_LENGTH, 0.999, 1, 1);
novelty_curve[NOVELTY_HISTORY_LENGTH - 1] = input;
```
Decay applied **BEFORE** writing new value.

**v2**:
```cpp
spectral_curve_[spectral_index_] = novelty;
spectral_index_ = (spectral_index_ + 1) % SPECTRAL_HISTORY_LENGTH;
// Apply decay AFTER writing
for (uint16_t i = 0; i < SPECTRAL_HISTORY_LENGTH; i++) {
    spectral_curve_[i] *= NOVELTY_DECAY;
}
```
Decay applied **AFTER** writing new value.

**Assessment**: Both approaches work, but Emotiscope 2.0's order ensures the new value is not decayed on first frame.

### 2. Smoothing EMA Constants

Both use 0.975/0.025 - **MATCHES** ✅

### 3. Silent Bin Decay Rate

**Emotiscope 2.0**: 0.995 decay for silent bins  
**v2**: Not implemented (all bins smoothed the same)

---

## Recommended Fixes

### Priority 1: Fix Silent Bin Suppression

```cpp
for (uint16_t i = 0; i < NUM_TEMPI; i++) {
    float scaled = tempi_[i].magnitude_raw * autoranger;
    scaled = scaled * scaled;
    scaled = scaled * scaled;
    tempi_[i].magnitude = scaled;

    if (tempi_[i].magnitude > 0.005f) {
        // Active bin: smooth and add to power sum
        tempi_smooth_[i] = tempi_smooth_[i] * 0.975f +
                           tempi_[i].magnitude * 0.025f;
        power_sum_ += tempi_smooth_[i];
    } else {
        // Silent bin: decay only, don't add to power sum
        tempi_smooth_[i] *= 0.995f;
    }

    tempi_[i].beat = sinf(tempi_[i].phase);
}
```

### Priority 2: Fix Confidence Calculation

```cpp
// Find maximum smoothed magnitude (Emotiscope 2.0 formula)
float max_contribution = 0.0000001f;
for (uint16_t i = 0; i < NUM_TEMPI; i++) {
    max_contribution = std::max(max_contribution, tempi_smooth_[i]);
}

// Confidence = max smoothed / power sum
if (power_sum_ > 0.01f) {
    confidence_ = max_contribution / power_sum_;
} else {
    confidence_ = 0.0f;
}
```

### Priority 3: Sync Phase for All Active Bins

Add phase sync in `updateTempo()` after smoothing:

```cpp
for (uint16_t i = 0; i < NUM_TEMPI; i++) {
    // ... smoothing code ...
    
    if (tempi_[i].magnitude > 0.005f) {
        // Sync phase for active bins (Emotiscope 2.0 parity)
        float phase_push = tempi_[i].phase_radians_per_frame * delta_sec * REFERENCE_FPS;
        tempi_[i].phase += phase_push;
        
        // Wrap phase
        while (tempi_[i].phase > static_cast<float>(M_PI)) {
            tempi_[i].phase -= 2.0f * static_cast<float>(M_PI);
            tempi_[i].phase_inverted = !tempi_[i].phase_inverted;
        }
        while (tempi_[i].phase < -static_cast<float>(M_PI)) {
            tempi_[i].phase += 2.0f * static_cast<float>(M_PI);
            tempi_[i].phase_inverted = !tempi_[i].phase_inverted;
        }
    }
}
```

---

## Impact Assessment

| Issue | Severity | Impact on Beat Tracking |
|-------|----------|------------------------|
| Silent Bin Suppression Missing | **HIGH** | Noise in power_sum, inaccurate confidence |
| Wrong Confidence Formula | **HIGH** | Different confidence values, lock threshold may not work |
| Phase Sync Only for Winner | **MEDIUM** | Phase jumps on winner change, less accurate phase tracking |

---

## Testing Recommendations

After fixes:
1. Test with 138 BPM track - should match Emotiscope 2.0 behavior
2. Verify confidence values are in expected range (0.0-1.0)
3. Test winner switching - phase should transition smoothly
4. Test silence - silent bins should not affect power_sum

---

## Conclusion

The v2 implementation is **missing critical Emotiscope 2.0 features** that affect beat tracking accuracy. The three issues identified should be fixed to achieve true parity with Emotiscope 2.0 behavior.

