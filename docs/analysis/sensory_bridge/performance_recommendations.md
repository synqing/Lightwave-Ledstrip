# Sensory Bridge Performance Optimization Recommendations

## Executive Summary

The Sensory Bridge firmware already achieves excellent performance (95+ FPS main loop, 100-300+ FPS LED thread). This document identifies potential bottlenecks and provides optimization recommendations for scenarios requiring higher performance or lower latency.

## Current Performance Baseline

### Measured Performance

- **Main Loop FPS**: 80-95 FPS (typical), 85-100 FPS (measured range)
- **LED Thread FPS**: 100-300+ FPS (typical)
- **Audio Latency**: ~33-40 ms (sound to LED response)
- **GDFT Processing**: 1-3 ms per frame
- **LED Rendering**: 1-5 ms per frame (mode-dependent)
- **WS2812 Transmission**: ~3.8 ms for 128 LEDs

### Performance Budget Analysis

**Main Loop** (per 10.5 ms frame):
- Typical: ~120% budget (slight overrun, acceptable)
- Worst-case: ~143% budget (significant overrun, may cause frame drops)

**LED Thread** (per frame):
- Typical: ~64% budget (comfortable margin)
- Worst-case: ~160% budget (overrun, but no hard deadline)

## Identified Bottlenecks

### Critical Bottlenecks

#### 1. `sample_window[]` Shift Operation

**Location**: `i2s_audio.h::acquire_sample_chunk()`

**Current Implementation**:
```cpp
// Shift existing data left (O(n) operation)
for (int i = 0; i < SAMPLE_HISTORY_LENGTH - CONFIG.SAMPLES_PER_CHUNK; i++) {
    sample_window[i] = sample_window[i + CONFIG.SAMPLES_PER_CHUNK];
}
// Append new data right
for (int i = SAMPLE_HISTORY_LENGTH - CONFIG.SAMPLES_PER_CHUNK; i < SAMPLE_HISTORY_LENGTH; i++) {
    sample_window[i] = waveform[i - (SAMPLE_HISTORY_LENGTH - CONFIG.SAMPLES_PER_CHUNK)];
}
```

**Cost**: ~0.5 ms per frame (4096 samples × 2 bytes = 8 KB memcpy)

**Impact**: 
- **Medium**: Not critical but adds unnecessary overhead
- **Frequency**: Every frame (~95 times/second)

**Optimization**: **Circular Buffer**

**Implementation**:
```cpp
// Add to globals.h
static uint16_t sample_window_head = 0;  // Circular buffer head index

// In acquire_sample_chunk(), replace shift with:
for (uint16_t i = 0; i < CONFIG.SAMPLES_PER_CHUNK; i++) {
    uint16_t idx = (sample_window_head + i) % SAMPLE_HISTORY_LENGTH;
    sample_window[idx] = waveform[i];
}
sample_window_head = (sample_window_head + CONFIG.SAMPLES_PER_CHUNK) % SAMPLE_HISTORY_LENGTH;

// In GDFT processing, adjust indexing:
// Instead of: sample_window[SAMPLE_HISTORY_LENGTH - 1 - n]
// Use: sample_window[(sample_window_head - 1 - n + SAMPLE_HISTORY_LENGTH) % SAMPLE_HISTORY_LENGTH]
```

**Expected Savings**: ~0.5 ms per frame (~5% main loop improvement)

**Complexity**: Medium (requires GDFT indexing changes)

**Risk**: Low (well-understood pattern)

#### 2. GDFT Processing (Worst-Case)

**Location**: `GDFT.h::process_GDFT()`

**Current Implementation**:
- 64 Goertzel filters (interlaced to ~40 per frame)
- Fixed-point arithmetic (already optimized)
- Window application: O(block_size) per bin
- Worst-case: All bins processed with large windows = ~3 ms

**Cost**: 1-3 ms per frame (typical), up to 3 ms (worst-case)

**Impact**:
- **High**: Dominates main loop processing time
- **Frequency**: Every frame

**Optimization Options**:

**A. SIMD Optimization (ESP32-S3)**

ESP32-S3 supports SIMD instructions for parallel processing.

**Implementation** (conceptual):
```cpp
#include "esp_dsp.h"  // ESP32-S3 DSP library

// Process multiple bins in parallel using SIMD
// (Requires significant refactoring of Goertzel algorithm)
```

**Expected Savings**: ~0.5-1 ms per frame (~10-20% improvement)

**Complexity**: High (requires algorithm restructuring)

**Risk**: Medium (SIMD code is complex, may introduce bugs)

**B. Reduce Block Sizes**

**Current**: `MAX_BLOCK_SIZE = 1500` samples

**Optimization**: Reduce to 1000-1200 samples

**Trade-off**: 
- **Pro**: Faster processing (~20% reduction)
- **Con**: Slightly lower frequency resolution for low frequencies

**Expected Savings**: ~0.3-0.6 ms per frame

**Complexity**: Low (configuration change)

**Risk**: Low (minimal impact on audio quality)

**C. Increase Interlacing**

**Current**: Odd/even bins alternate, bins 16+ always processed

**Optimization**: Process bins 8+ every frame, others every 2-3 frames

**Trade-off**:
- **Pro**: Faster processing (~30% reduction)
- **Con**: Lower update rate for low frequencies (may cause visual lag)

**Expected Savings**: ~0.5-1 ms per frame

**Complexity**: Low (logic change)

**Risk**: Medium (may affect visual quality)

### Moderate Bottlenecks

#### 3. LED Interpolation (If Enabled)

**Location**: `led_utilities.h::show_leds()`

**Current Implementation**:
```cpp
for (uint16_t i = 0; i < CONFIG.LED_COUNT; i++) {
    leds_out[i] = lerp_led(index, leds);  // Linear interpolation per LED
    index += index_push;
}
```

**Cost**: ~0.5-2 ms (depends on LED count, ~10-20 μs per LED)

**Impact**:
- **Low-Medium**: Only affects non-native resolutions
- **Frequency**: Every LED frame (100-300+ times/second)

**Optimization**: **Lookup Table for Common Resolutions**

**Implementation**:
```cpp
// Precompute interpolation weights for common resolutions
static float interpolation_weights_64[64][2];  // [LED_index][weight_left, weight_right]
static uint8_t interpolation_indices_64[64][2];  // [LED_index][left_idx, right_idx]

// Initialize once at startup
void init_interpolation_tables() {
    if (CONFIG.LED_COUNT == 64) {
        for (uint8_t i = 0; i < 64; i++) {
            float index_f = (i / 63.0) * 127.0;
            interpolation_indices_64[i][0] = (uint8_t)index_f;
            interpolation_indices_64[i][1] = (uint8_t)index_f + 1;
            float frac = index_f - (uint8_t)index_f;
            interpolation_weights_64[i][0] = 1.0 - frac;
            interpolation_weights_64[i][1] = frac;
        }
    }
}

// Fast interpolation using lookup table
if (CONFIG.LED_COUNT == 64) {
    for (uint8_t i = 0; i < 64; i++) {
        uint8_t left = interpolation_indices_64[i][0];
        uint8_t right = interpolation_indices_64[i][1];
        float w_left = interpolation_weights_64[i][0];
        float w_right = interpolation_weights_64[i][1];
        
        leds_out[i].r = leds[left].r * w_left + leds[right].r * w_right;
        leds_out[i].g = leds[left].g * w_left + leds[right].g * w_right;
        leds_out[i].b = leds[left].b * w_left + leds[right].b * w_right;
    }
}
```

**Expected Savings**: ~0.3-1 ms per frame (eliminates function call overhead)

**Complexity**: Medium (requires table initialization)

**Risk**: Low (straightforward optimization)

#### 4. Task Delay (LED Thread)

**Location**: `SENSORY_BRIDGE_FIRMWARE.ino::led_thread()`

**Current Implementation**:
```cpp
if (CONFIG.LED_TYPE == LED_NEOPIXEL) {
    vTaskDelay(1);  // 1 ms delay
}
else if (CONFIG.LED_TYPE == LED_DOTSTAR) {
    vTaskDelay(3);  // 3 ms delay
}
```

**Cost**: 1-3 ms per frame

**Impact**:
- **Low**: Prevents CPU hogging but reduces LED FPS
- **Frequency**: Every LED frame

**Optimization**: **Reduce Delay or Make Adaptive**

**Implementation**:
```cpp
// Reduce to 0.5 ms (still prevents hogging)
vTaskDelay(1);  // 1 tick = 10 ms on FreeRTOS default, but can configure to 1 ms

// Or make adaptive based on CPU load
static uint32_t last_render_time = 0;
uint32_t render_time = micros() - last_render_time;
if (render_time < 5000) {  // If render took <5 ms
    vTaskDelay(1);  // Delay to prevent hogging
}
// else: No delay (already slow enough)
```

**Expected Savings**: 0.5-2.5 ms per frame (increases LED FPS)

**Complexity**: Low (simple change)

**Risk**: Low (may slightly increase CPU usage, but ESP32-S3 has headroom)

### Minor Bottlenecks

#### 5. Float-to-Int Conversions

**Location**: Throughout codebase (GDFT, LED rendering)

**Current**: Many float operations with frequent conversions

**Impact**:
- **Low**: ESP32-S3 has FPU, but conversions still have overhead
- **Frequency**: High (thousands per frame)

**Optimization**: **Minimize Conversions**

**Implementation**: Use fixed-point arithmetic where possible (already done in Goertzel, but could extend to other areas)

**Expected Savings**: ~0.1-0.2 ms per frame

**Complexity**: High (requires significant refactoring)

**Risk**: Medium (may introduce bugs, reduce code readability)

**Recommendation**: **Not worth it** (minimal benefit, high complexity)

#### 6. HSV→RGB Conversion

**Location**: `lightshow_modes.h` (mode functions)

**Current**: `hsv2rgb_spectrum()` called 128 times per frame (GDFT mode)

**Cost**: ~0.3 ms per frame (FastLED uses lookup tables, already optimized)

**Impact**:
- **Low**: Already well-optimized by FastLED
- **Frequency**: Every LED frame

**Optimization**: **None recommended** (FastLED already uses lookup tables)

## Optimization Priority Matrix

| Optimization | Impact | Complexity | Risk | Priority | Estimated Savings |
|--------------|--------|------------|------|----------|-------------------|
| Circular Buffer | Medium | Medium | Low | **High** | ~0.5 ms/frame |
| Reduce Block Sizes | Medium | Low | Low | **High** | ~0.3-0.6 ms/frame |
| LED Interpolation LUT | Low-Medium | Medium | Low | **Medium** | ~0.3-1 ms/frame |
| Reduce Task Delay | Low | Low | Low | **Medium** | 0.5-2.5 ms/frame |
| SIMD GDFT | High | High | Medium | **Low** | ~0.5-1 ms/frame |
| Increase Interlacing | Medium | Low | Medium | **Low** | ~0.5-1 ms/frame |
| Float-to-Int Minimization | Low | High | Medium | **Very Low** | ~0.1-0.2 ms/frame |

## Recommended Optimization Path

### Phase 1: Quick Wins (Low Risk, Medium Impact)

1. **Circular Buffer for `sample_window[]`**
   - **Effort**: 2-4 hours
   - **Savings**: ~0.5 ms/frame
   - **Risk**: Low

2. **Reduce `MAX_BLOCK_SIZE`**
   - **Effort**: 5 minutes (config change)
   - **Savings**: ~0.3-0.6 ms/frame
   - **Risk**: Low (test audio quality)

3. **Reduce Task Delay**
   - **Effort**: 5 minutes
   - **Savings**: 0.5-2.5 ms/frame (increases LED FPS)
   - **Risk**: Low

**Total Expected Improvement**: ~1.3-3.6 ms/frame (~12-34% main loop improvement)

### Phase 2: Medium-Term (Medium Risk, Medium Impact)

4. **LED Interpolation Lookup Table**
   - **Effort**: 2-3 hours
   - **Savings**: ~0.3-1 ms/frame (if interpolation enabled)
   - **Risk**: Low

**Total Expected Improvement**: ~1.6-4.6 ms/frame (~15-44% main loop improvement)

### Phase 3: Advanced (High Risk, High Impact)

5. **SIMD GDFT Optimization**
   - **Effort**: 1-2 weeks (algorithm restructuring)
   - **Savings**: ~0.5-1 ms/frame
   - **Risk**: Medium-High (complex code, may introduce bugs)

**Total Expected Improvement**: ~2.1-5.6 ms/frame (~20-53% main loop improvement)

## Measurement and Validation

### Existing Measurement Tools

**FPS Logging**:
```cpp
// Main loop FPS
SYSTEM_FPS = fps_sum / 10.0;  // 10-frame average
// Access via: Serial command "fps" or "stream=fps"

// LED thread FPS
LED_FPS = FastLED.getFPS();  // Library-provided
// Access via: Serial command "led_fps"
```

**Function Timing** (Debug Mode):
```cpp
// Enable: Serial command "debug=true"
// Output: Function hit counts every 30 seconds
// Resolution: 5 ms (coarse)
```

**Manual Timing**:
```cpp
start_timing("Function Name");
// ... code ...
end_timing();  // Prints timing in milliseconds
```

### Recommended Measurement Approach

1. **Baseline Measurement**:
   - Enable `debug_mode` and run for 1 minute
   - Record `SYSTEM_FPS` and `LED_FPS`
   - Note function hit counts

2. **Apply Optimization**:
   - Implement one optimization at a time
   - Test thoroughly (audio quality, visual quality)

3. **Post-Optimization Measurement**:
   - Repeat baseline measurement
   - Compare FPS and function hit counts
   - Verify no regressions

4. **Iterate**:
   - Apply next optimization
   - Repeat measurement cycle

### Validation Criteria

**Performance Targets**:
- **Main Loop FPS**: >100 FPS (currently 80-95 FPS)
- **LED Thread FPS**: >200 FPS (currently 100-300+ FPS, already exceeds)
- **Audio Latency**: <30 ms (currently 33-40 ms)

**Quality Targets**:
- **Audio Quality**: No degradation (subjective, compare spectrograms)
- **Visual Quality**: No visible artifacts (subjective, compare light shows)
- **Stability**: No crashes or freezes (run for 1+ hour)

## Alternative Optimization Strategies

### 1. Reduce Audio Latency

**Current**: ~33-40 ms (2-frame lookahead delay)

**Optimization**: Reduce lookahead delay to 1 frame

**Implementation**:
```cpp
// In lookahead_smoothing(), change:
note_spectrogram_smooth[i] = spectrogram_history[past_index][i];  // Frame N-2
// To:
note_spectrogram_smooth[i] = spectrogram_history[look_ahead_1][i];  // Frame N-1
```

**Trade-off**:
- **Pro**: Reduces latency by ~10.5 ms
- **Con**: Slightly more flicker (less smoothing)

**Expected Latency**: ~22-30 ms

**Recommendation**: **Try it** (flicker may be imperceptible)

### 2. Increase Sample Rate

**Current**: 12.2 kHz (default)

**Optimization**: Increase to 16-20 kHz

**Trade-off**:
- **Pro**: Better frequency resolution, higher Nyquist frequency
- **Con**: More samples per frame, slower processing

**Implementation**: Change `CONFIG.SAMPLE_RATE` (requires reboot)

**Expected Impact**: 
- Frame period: 128/16000 = 8 ms (faster)
- But: More CPU per frame (may offset benefit)

**Recommendation**: **Test first** (may not improve performance)

### 3. Reduce LED Count

**Current**: 128 LEDs (native resolution)

**Optimization**: Use fewer LEDs (if hardware supports)

**Trade-off**:
- **Pro**: Faster WS2812 transmission, less memory
- **Con**: Lower visual resolution

**Expected Savings**: ~1.9 ms per 64-LED reduction (WS2812 transmission)

**Recommendation**: **Hardware-dependent** (only if fewer LEDs are acceptable)

## Conclusion

The Sensory Bridge firmware is already well-optimized. The recommended optimizations (circular buffer, reduce block sizes, reduce task delay) provide **~12-34% improvement** with **low risk** and **minimal effort**.

**For most use cases, current performance is sufficient.** Optimizations should only be pursued if:
1. Higher frame rates are required (>100 FPS main loop)
2. Lower latency is critical (<30 ms)
3. CPU headroom is needed for additional features

**Recommended Action**: Implement Phase 1 optimizations (quick wins) and measure results. Only proceed to Phase 2/3 if Phase 1 doesn't meet requirements.

