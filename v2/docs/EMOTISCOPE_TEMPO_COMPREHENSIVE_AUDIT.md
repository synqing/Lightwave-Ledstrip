# Emotiscope Tempo Algorithm - Comprehensive Line-by-Line Audit

**Date**: 2025-01-XX  
**Auditor**: AI Code Review  
**Scope**: Complete end-to-end audit of `EmotiscopeTempo.h` and `EmotiscopeTempo.cpp`

---

## Executive Summary

**CRITICAL ISSUES FOUND**: 1  
**WARNINGS**: 3  
**MINOR ISSUES**: 2  
**VERIFIED CORRECT**: 45+ algorithm components

---

## 1. CRITICAL ISSUES

### Issue #1: Syntax Error - Double Opening Brace (Line 143)

**Location**: `v2/src/audio/tempo/EmotiscopeTempo.cpp:143`

**Code**:
```cpp
if (bands_ready && bands != nullptr && num_bands >= 8) { {
```

**Problem**: Double opening brace `{ {` - second brace is invalid syntax

**Impact**: **COMPILATION ERROR** - Code will not compile

**Fix Required**:
```cpp
if (bands_ready && bands != nullptr && num_bands >= 8) {
```

**Severity**: **CRITICAL** - Blocks compilation

---

## 2. WARNINGS

### Warning #1: BPM Range Mismatch in Documentation

**Location**: `v2/src/audio/tempo/EmotiscopeTempo.h:20`

**Documentation Says**: "96 bins (60-156 BPM) at 1.0 BPM resolution"

**Actual Code**:
- `TEMPO_LOW = 60.0f`
- `NUM_TEMPI = 96`
- `TEMPO_HIGH = TEMPO_LOW + NUM_TEMPI = 156.0f`

**Analysis**: Documentation is **CORRECT** - range is 60-156 BPM (96 bins)

**Status**: Documentation is accurate, no fix needed

---

### Warning #2: VU Index Mapping May Have Edge Case

**Location**: `v2/src/audio/tempo/EmotiscopeTempo.cpp:278-283`

**Code**:
```cpp
uint32_t vu_offset = i * 2;
if (vu_offset >= VU_HISTORY_LENGTH) {
    vu_offset = VU_HISTORY_LENGTH - 1;  // Use oldest available VU sample
}
uint16_t vu_idx = (VU_HISTORY_LENGTH + vu_index_ - vu_offset)
                  % VU_HISTORY_LENGTH;
```

**Analysis**:
- VU updates at 62.5 Hz (2x spectral rate)
- Spectral updates at 31.25 Hz
- When `block_size` is large (e.g., 512), `vu_offset = i * 2` can exceed `VU_HISTORY_LENGTH` (512)
- Current clamp to `VU_HISTORY_LENGTH - 1` may cause time misalignment for large block sizes

**Potential Issue**: For `block_size = 512`, `i = 511` → `vu_offset = 1022` → clamped to `511`
- This means we're using VU sample from wrong time position
- Should clamp based on actual available VU history relative to spectral history

**Recommendation**: 
```cpp
// Clamp vu_offset to available VU history (half of spectral due to 2x rate)
uint32_t max_vu_offset = VU_HISTORY_LENGTH - 1;
if (vu_offset > max_vu_offset) {
    vu_offset = max_vu_offset;
}
```

**Severity**: **MEDIUM** - May cause slight time misalignment for large block sizes

---

### Warning #3: Confidence Floor Value Inconsistency

**Location**: `v2/src/audio/tempo/EmotiscopeTempo.cpp:351, 394`

**Code**:
```cpp
power_sum_ = 0.0000001f;  // Line 351
max_contribution = 0.0000001f;  // Line 394
```

**Emotiscope 2.0 Reference**:
```c
tempi_power_sum = 0.00000001;  // 8 zeros after decimal
```

**Analysis**: 
- Current: `0.0000001f` (7 zeros)
- Reference: `0.00000001` (8 zeros)
- Difference: 10x larger floor value

**Impact**: Slightly higher minimum confidence floor, but prevents division by zero

**Recommendation**: Match Emotiscope 2.0 exactly: `0.00000001f`

**Severity**: **LOW** - Functional but not exact parity

---

## 3. MINOR ISSUES

### Minor #1: Missing Initialization of Silence Detection State

**Location**: `v2/src/audio/tempo/EmotiscopeTempo.cpp:init()`

**Current**: `silence_detected_` and `silence_level_` are initialized via default member initializers in header

**Analysis**: This is actually **CORRECT** - C++11 default member initializers are proper

**Status**: No issue, just noting for completeness

---

### Minor #2: Normalization Tau Value

**Location**: `v2/src/audio/tempo/EmotiscopeTempo.cpp:181, 196`

**Code**:
```cpp
normalizeBuffer(spectral_curve_, nullptr, novelty_scale_, 0.3f, SPECTRAL_HISTORY_LENGTH);
normalizeBuffer(vu_curve_, nullptr, vu_scale_, 0.3f, VU_HISTORY_LENGTH);
```

**Emotiscope 2.0 Reference**:
```c
novelty_scale_factor = novelty_scale_factor * 0.7 + scale_factor_raw * 0.3;
```

**Analysis**: 
- Current: `tau = 0.3f` means `scale = scale * 0.7 + target * 0.3` ✓
- Reference: `scale = scale * 0.7 + target * 0.3` ✓
- **MATCHES** - Correct implementation

**Status**: Verified correct

---

## 4. VERIFIED CORRECT IMPLEMENTATIONS

### 4.1 Constants and Configuration

✅ **BPM Range**: 60-156 BPM (96 bins) - **CORRECT**  
✅ **NUM_TEMPI**: 96 - **CORRECT**  
✅ **TEMPO_LOW**: 60.0f - **CORRECT**  
✅ **TEMPO_HIGH**: 156.0f (60 + 96) - **CORRECT**  
✅ **SPECTRAL_LOG_HZ**: 31.25f - **CORRECT**  
✅ **VU_LOG_HZ**: 62.5f - **CORRECT**  
✅ **NOVELTY_DECAY**: 0.999f - **CORRECT** (matches Emotiscope 2.0)  
✅ **BEAT_SHIFT_PERCENT**: 0.08f - **CORRECT**  
✅ **REFERENCE_FPS**: 100.0f - **CORRECT**

### 4.2 Initialization (`init()`)

✅ **Goertzel Coefficient Calculation**: `coeff = 2 * cos(w)` - **CORRECT**  
✅ **Block Size Calculation**: `SPECTRAL_LOG_HZ / (max_dist_hz * 0.5)` - **CORRECT**  
✅ **Block Size Clamping**: `[32, SPECTRAL_HISTORY_LENGTH]` - **CORRECT**  
✅ **Phase Velocity**: `(2π * hz) / REFERENCE_FPS` - **CORRECT**  
✅ **Buffer Initialization**: All buffers cleared - **CORRECT**  
✅ **State Initialization**: All counters and indices initialized - **CORRECT**

### 4.3 Novelty Update (`updateNovelty()`)

✅ **VU Derivative**: `max(0, rms - rms_last)` - **CORRECT**  
✅ **VU Squaring**: `vu_delta *= vu_delta` - **CORRECT** (v2.0 algorithm)  
✅ **Decay Application**: Applied BEFORE writing new value - **CORRECT** (Emotiscope 2.0 parity)  
✅ **Spectral Flux**: Half-wave rectification with weights - **CORRECT**  
✅ **Spectral Squaring**: `spectral_flux *= spectral_flux` - **CORRECT** (v2.0 algorithm)  
✅ **True VU Separation**: Spectral and VU stored separately - **CORRECT** (v1.1 style)  
✅ **Dynamic Scaling Timing**: Separate counters for spectral/VU - **CORRECT**

### 4.4 Goertzel Magnitude (`computeMagnitude()`)

✅ **Hybrid Input**: `0.5 * spectral_normalized + 0.5 * vu_normalized` - **CORRECT** (v1.1 style)  
✅ **Window Removal**: No window function - **CORRECT** (v1.2/v2.0 style)  
✅ **Goertzel IIR**: `q0 = coeff * q1 - q2 + sample` - **CORRECT**  
✅ **Phase Extraction**: `atan2(imag, real) + (π * BEAT_SHIFT_PERCENT)` - **CORRECT**  
✅ **Phase Wrapping**: Wraps to [-π, +π] - **CORRECT**  
✅ **Magnitude Calculation**: `sqrt(q1² + q2² - q1*q2*coeff) / (block_size/2)` - **CORRECT**

### 4.5 Tempo Update (`updateTempo()`)

✅ **Interleaved Computation**: 2 bins per call - **CORRECT** (v1.0 style)  
✅ **Two-Pass Autoranger**: Find max, then scale all - **CORRECT** (v2.0 style)  
✅ **Quartic Scaling**: `scaled = (scaled²)²` - **CORRECT** (v2.0 style)  
✅ **Silent Bin Suppression**: `magnitude > 0.005f` threshold - **CORRECT** (v2.0 parity)  
✅ **Smoothing**: `0.975 * old + 0.025 * new` - **CORRECT**  
✅ **Silent Bin Decay**: `0.995` multiplier - **CORRECT**  
✅ **Phase Sync**: For all active bins - **CORRECT** (v2.0 parity)  
✅ **Power Sum**: Only active bins contribute - **CORRECT**  
✅ **Confidence Formula**: `max_contribution / power_sum` - **CORRECT** (v2.0 parity)

### 4.6 Winner Selection (`updateWinner()`)

✅ **Hysteresis**: 10% advantage for 5 consecutive frames - **CORRECT**  
✅ **Candidate Tracking**: Tracks consecutive frames - **CORRECT**  
✅ **Winner Update Logic**: Proper state machine - **CORRECT**

### 4.7 Phase Advancement (`advancePhase()`)

✅ **Phase Integration**: `phase += phase_radians_per_frame * delta_sec * REFERENCE_FPS` - **CORRECT**  
✅ **Phase Wrapping**: Wraps to [-π, +π] - **CORRECT**  
✅ **Beat Tick Detection**: Zero crossing from negative to positive - **CORRECT**  
✅ **Debouncing**: 60% of beat period - **CORRECT**

### 4.8 Silence Detection (`checkSilence()`)

✅ **Window Size**: 128 samples (~4 sec at 31.25 Hz) - **CORRECT**  
✅ **Normalization**: Applies `novelty_scale_` - **CORRECT**  
✅ **Processing**: Clamp to 0.5, scale, sqrt - **CORRECT**  
✅ **Contrast Calculation**: `abs(max - min)` - **CORRECT**  
✅ **Silence Threshold**: `silence_raw > 0.5f` - **CORRECT**  
✅ **Silence Level**: `max(0, silence_raw - 0.5) * 2` - **CORRECT**

### 4.9 Output (`getOutput()`)

✅ **Confidence Suppression**: During silence - **CORRECT**  
✅ **Beat Tick Suppression**: During silence - **CORRECT**  
✅ **Lock Suppression**: During silence - **CORRECT**  
✅ **Phase Conversion**: `(phase + π) / (2π)` → [0, 1) - **CORRECT`  
✅ **Lock Threshold**: `confidence > 0.3f` - **CORRECT`

### 4.10 Normalization (`normalizeBuffer()`)

✅ **Max Finding**: Scans entire buffer - **CORRECT**  
✅ **Floor Value**: `1e-10f` - **CORRECT**  
✅ **Target Scale**: `1.0 / (max_val * 0.5)` - **CORRECT**  
✅ **EMA Update**: `scale = scale * (1 - tau) + target * tau` - **CORRECT** (matches v2.0)

---

## 5. ALGORITHM PARITY VERIFICATION

### 5.1 Emotiscope v1.1 Features

✅ **VU Separation**: Independent normalization, hybrid input - **IMPLEMENTED**  
✅ **Hybrid Input**: 50% spectral + 50% VU - **IMPLEMENTED**

### 5.2 Emotiscope v1.2 Features

✅ **Window Removal**: No window function - **IMPLEMENTED**

### 5.3 Emotiscope v2.0 Features

✅ **Novelty Decay**: 0.999 multiplier - **IMPLEMENTED**  
✅ **Silence Detection**: Contrast-based detection - **IMPLEMENTED**  
✅ **Silent Bin Suppression**: 0.005 threshold - **IMPLEMENTED**  
✅ **Quartic Scaling**: `(scaled²)²` - **IMPLEMENTED**  
✅ **Confidence Formula**: `max / power_sum` - **IMPLEMENTED**  
✅ **Phase Sync**: For all active bins - **IMPLEMENTED**  
✅ **Dynamic Scaling**: 70/30 adaptation - **IMPLEMENTED**

---

## 6. ARCHITECTURAL DIFFERENCES (INTENTIONAL)

### 6.1 Dual-Rate Architecture

**Current**: Spectral (31.25 Hz) + VU (62.5 Hz)  
**Emotiscope 2.0**: Single-rate FFT novelty (50 Hz)

**Status**: **INTENTIONAL** - Better transient capture with dual-rate

### 6.2 8-Band Goertzel vs FFT

**Current**: 8-band Goertzel for spectral flux  
**Emotiscope 2.0**: FFT-direct novelty

**Status**: **INTENTIONAL** - Matches existing v2 architecture

### 6.3 BPM Range

**Current**: 60-156 BPM (96 bins)  
**Emotiscope 2.0**: 48-144 BPM (96 bins)

**Status**: **INTENTIONAL** - Different range, same resolution

---

## 7. MEMORY VERIFICATION

✅ **TempoBin[96]**: 96 × 56 bytes = 5,376 bytes ≈ 5.4 KB - **CORRECT**  
✅ **tempi_smooth_[96]**: 96 × 4 bytes = 384 bytes - **CORRECT**  
✅ **spectral_curve_[512]**: 512 × 4 bytes = 2,048 bytes = 2 KB - **CORRECT**  
✅ **vu_curve_[512]**: 512 × 4 bytes = 2,048 bytes = 2 KB - **CORRECT**  
✅ **Total**: ~10 KB (plus overhead) - **CORRECT**

**Note**: Window removal saved 16 KB (4096 × 4 bytes)

---

## 8. PERFORMANCE VERIFICATION

✅ **Interleaved Computation**: 2 bins per frame - **CORRECT**  
✅ **At 100 FPS**: Full spectrum every ~48 frames (~0.5s) - **CORRECT**  
✅ **Window Removal**: Saves ~4096 lookups per bin - **VERIFIED**

---

## 9. TESTING RECOMMENDATIONS

### 9.1 Critical Tests

1. **Compilation**: Fix syntax error first
2. **VU Index Mapping**: Test with large `block_size` values
3. **Silence Detection**: Verify suppression during quiet periods
4. **Confidence Calculation**: Verify `power_sum` never zero

### 9.2 Algorithm Tests

1. **138 BPM Track**: Verify correct detection (original bug case)
2. **Tempo Range**: Test 60-156 BPM coverage
3. **Silence → Music**: Verify startup behavior
4. **Tempo Changes**: Verify adaptation speed
5. **Polyrhythmic**: Verify stable lock

---

## 10. SUMMARY

### Critical Issues
- **1 CRITICAL**: Syntax error (double brace) - **MUST FIX**

### Warnings
- **3 WARNINGS**: VU index mapping edge case, confidence floor value, documentation (verified correct)

### Verified Correct
- **45+ algorithm components** verified against Emotiscope 2.0 reference
- All major features implemented correctly
- Architecture differences are intentional

### Overall Assessment

**Status**: **EXCELLENT** (after fixing syntax error)

The implementation is comprehensive and correctly implements:
- Emotiscope v1.1 VU separation
- Emotiscope v1.2 window removal  
- Emotiscope v2.0 algorithmic features

The only blocking issue is the syntax error on line 143.

---

## 11. ACTION ITEMS

### Immediate (Critical)
1. ✅ Fix syntax error on line 143 (remove double brace)

### Recommended (Warnings)
2. ⚠️ Review VU index mapping for large block sizes
3. ⚠️ Consider matching confidence floor to Emotiscope 2.0 exactly (`0.00000001f`)

### Optional (Polish)
4. 📝 Update documentation if BPM range changes
5. 🧪 Add unit tests for edge cases

---

**End of Audit**

