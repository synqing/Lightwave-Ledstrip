# Custom Parallel RMT Driver Implementation Plan
## ESP32-P4 WS2812 Dual-Strip Performance Optimization
**Target**: Achieve 120 FPS with 320 LEDs (2x160) via parallel RMT transmission
**Problem**: Current FastLED implementation likely transmits strips sequentially (~10ms)
**Solution**: Custom driver using ESP-IDF RMT API with explicit parallel async transmission (~5ms)
---
## Comparative Analysis: Emotiscope vs Proposed LightwaveOS-P4 Driver
### Overview of Emotiscope Implementation
The Emotiscope project by @lixielabs represents a production-quality, battle-tested implementation of a custom RMT LED driver for ESP32-S3. Key characteristics:
**Architecture Philosophy**:
- **32-bit floating-point color pipeline** (`CRGBF` - custom float RGB type)
- **Dual RMT channels** for parallel transmission
- **Custom RMT encoder** (not using ESP-IDF led_strip component)
- **Temporal dithering** for higher perceived color depth
- **ESP-DSP SIMD acceleration** for bulk array operations
---
## Detailed Comparison
### 1. Color Representation
| Aspect | Emotiscope | My Proposed Driver |
|--------|------------|-------------------|
| **Internal Format** | `CRGBF` (3x float32) | `CRGB` (3x uint8) via FastLED |
| **Precision** | 32-bit per channel | 8-bit per channel |
| **Memory per LED** | 12 bytes | 3 bytes |
| **HDR Support** | Yes (values >1.0 allowed) | No |
| **Total Memory (320 LEDs)** | 3840 bytes | 960 bytes |
**Emotiscope Approach**:
```c
CRGBF leds[NUM_LEDS];           // 32-bit working buffer
CRGBF leds_scaled[NUM_LEDS];    // Intermediate scaling
uint8_t raw_led_data[NUM_LEDS*3]; // Final 8-bit output
```
**Key Insight**: Emotiscope maintains float precision throughout the entire effects pipeline, only quantizing to 8-bit at the final transmission stage. This enables:
- HDR rendering with soft-clipping (tanh)
- Temporal dithering for perceived 10+ bit color
- Blur/blend operations without banding artifacts
**My Proposed Approach**:
```cpp
CRGB m_strip1[160];  // 8-bit buffer (FastLED native)
CRGB m_strip2[160];
uint8_t m_rmt_buffer1[160*3];  // Final transmission buffer
```
**Trade-off Analysis**:
- **Pro (Emotiscope)**: Superior color quality, HDR pipeline, no banding
- **Con (Emotiscope)**: 4x memory usage, more CPU for conversion
- **Pro (My Approach)**: Drop-in FastLED compatibility, lower memory
- **Con (My Approach)**: 8-bit artifacts, no HDR support
---
### 2. RMT Driver Architecture
| Aspect | Emotiscope | My Proposed Driver |
|--------|------------|-------------------|
| **RMT API Level** | Low-level (`rmt_transmit()`) | High-level (`led_strip_refresh_async()`) |
| **Encoder** | Custom `rmt_led_strip_encoder_t` | ESP-IDF led_strip component |
| **DMA** | Disabled (`with_dma = 0`) | Enabled (`with_dma = true`) |
| **Memory Block** | 128 symbols | 1024 symbols |
| **Parallel Strategy** | Explicit dual channel | Explicit dual channel |
**Emotiscope RMT Init**:
```c
rmt_tx_channel_config_t tx_chan_a_config = {
    .gpio_num = LED_DATA_1_PIN,
    .resolution_hz = 10000000,     // 10 MHz
    .mem_block_symbols = 128,      // Smaller buffer
    .trans_queue_depth = 4,
    .flags = {
        .with_dma = 0,  // No DMA!
    },
};
```
**Key Discovery**: Emotiscope does NOT use DMA. This is surprising - it achieves high frame rates (hundreds of FPS) without DMA on ESP32-S3.
**My Proposed RMT Init**:
```cpp
led_strip_rmt_config_t rmt_config = {
    .resolution_hz = 10 * 1000 * 1000,
    .mem_block_symbols = 1024,
    .flags = { .with_dma = true },  // DMA enabled
};
```
**Trade-off Analysis**:
- **Pro (Emotiscope)**: Lower latency (no DMA descriptor setup), simpler debugging
- **Con (Emotiscope)**: CPU busy during transmission
- **Pro (My Approach)**: CPU free during transmission, potentially more efficient
- **Con (My Approach)**: DMA overhead, larger memory allocation
---
### 3. Parallel Transmission Strategy
**Emotiscope**:
```c
IRAM_ATTR void transmit_leds() {
    // Wait for PREVIOUS frame to complete (double-buffering pattern)
    rmt_tx_wait_all_done(tx_chan_a, -1);
    rmt_tx_wait_all_done(tx_chan_b, -1);
    
    // Quantize float to uint8 (with dithering)
    quantize_color_error(temporal_dithering);
    
    // Start BOTH channels (parallel!)
    rmt_transmit(tx_chan_a, led_encoder_a, raw_led_data, size/2, &tx_config);
    rmt_transmit(tx_chan_b, led_encoder_b, raw_led_data+(NUM_LEDS/2)*3, size/2, &tx_config);
    // Returns immediately - transmission continues in background
}
```
**My Proposed**:
```cpp
void show() {
    // Start parallel async
    led_strip_refresh_async(m_rmt_strip1);
    led_strip_refresh_async(m_rmt_strip2);
    
    // Wait for completion
    led_strip_refresh_wait_done(m_rmt_strip1);
    led_strip_refresh_wait_done(m_rmt_strip2);
}
```
**Critical Difference**: Emotiscope waits at the START of the function for the PREVIOUS frame, not at the end for the CURRENT frame. This is a **double-buffering** pattern that allows rendering to continue while transmission happens in background.
**Impact**: Emotiscope's approach gives ~5ms more CPU time per frame for rendering!
---
### 4. Temporal Dithering
**Emotiscope Implementation**:
```c
CRGBF dither_error[NUM_LEDS];  // Error accumulator
void quantize_color_error(bool temporal_dithering) {
    // Scale 0.0-1.0 → 0-255
    dsps_mulc_f32_ae32_fast((float*)leds, (float*)leds_scaled, NUM_LEDS*3, 255.0, 1, 1);
    
    if (temporal_dithering) {
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            raw_led_data[3*i+1] = (uint8_t)(leds_scaled[i].r);
            
            // Calculate quantization error
            float new_error_r = leds_scaled[i].r - raw_led_data[3*i+1];
            
            // Accumulate error above threshold
            if (new_error_r >= threshold) {
                dither_error[i].r += new_error_r;
            }
            
            // When error >= 1.0, add 1 to output and subtract from accumulator
            if (dither_error[i].r >= 1.0) {
                raw_led_data[3*i+1] += 1;
                dither_error[i].r -= 1.0;
            }
        }
    }
}
```
**My Proposed**: No dithering (initial implementation)
**Impact**: Temporal dithering significantly improves perceived color depth at high frame rates. At 120+ FPS, the human eye averages rapid brightness variations, effectively increasing color depth to 10-12 bits perceived.
---
### 5. Color Processing Pipeline
**Emotiscope Pipeline** (sophisticated):
```
render() → [CRGBF 0.0-∞]
    ↓
apply_brightness() → scale by 0.1-1.0
    ↓
apply_tonemapping() → soft_clip_hdr() (tanh compression)
    ↓
apply_warmth() → incandescent color shift
    ↓
apply_gamma_correction() → 2048-entry LUT
    ↓
multiply_by_white_balance() → {1.0, 0.9375, 0.84}
    ↓
quantize_color_error() → float32 → uint8 with dithering
    ↓
transmit_leds()
```
**My Proposed Pipeline** (minimal):
```
render() → [CRGB 0-255]
    ↓
applyBrightnessAndCorrection() → nscale8(), TypicalLEDStrip
    ↓
convertCrgbToRmt() → reorder RGB→GRB
    ↓
led_strip_set_pixel() → copy to RMT buffer
    ↓
led_strip_refresh_async()
```
**Key Features Missing from My Approach**:
1. **Tonemapping**: Emotiscope's `soft_clip_hdr()` prevents harsh clipping
2. **Warmth**: Incandescent color temperature simulation
3. **Gamma LUT**: Pre-computed 2048-entry table for fast gamma
4. **Custom white balance**: `{1.0, 0.9375, 0.84}` vs generic TypicalLEDStrip
---
### 6. ESP-DSP SIMD Usage
**Emotiscope**: Heavy use of ESP-DSP for bulk operations:
```c
// SIMD-accelerated array multiply
dsps_mulc_f32_ae32_fast(ptr, ptr, array_length * 3, scale_value, 1, 1);
// SIMD-accelerated array add
dsps_add_f32_ae32_fast(ptr_a, ptr_b, ptr_a, array_length * 3, 1);
// Fast memory copy
dsps_memcpy_aes3(leds_temp, leds, sizeof(CRGBF) * NUM_LEDS);
```
**My Proposed**: Standard C++ loops
**Performance Impact**: ESP-DSP functions can be 2-4x faster than scalar loops for large arrays. For 320 LEDs × 3 channels = 960 operations, this could save 100-200µs per frame.
---
### 7. WS2812 Timing
| Parameter | Emotiscope | My Proposed | WS2812B Spec |
|-----------|------------|-------------|--------------|
| T0H | 4 ticks (0.4µs) | 400ns | 400ns ±150ns |
| T0L | 6 ticks (0.6µs) | 850ns | 850ns ±150ns |
| T1H | 7 ticks (0.7µs) | 800ns | 800ns ±150ns |
| T1L | 6 ticks (0.6µs) | 450ns | 450ns ±150ns |
| Reset | 50µs | 50µs | >50µs |
**Note**: Both are within WS2812B spec. Emotiscope uses slightly shorter T1L (0.6µs vs 0.45µs spec) which may be more reliable.
---
### 8. Custom Encoder vs ESP-IDF Component
**Emotiscope Custom Encoder**:
```c
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;
static size_t rmt_encode_led_strip(...) {
    // State machine: 0=RGB data, 1=reset code
    switch (led_encoder->state) {
        case 0: // Encode bytes
            encoded_symbols += bytes_encoder->encode(...);
            if (complete) led_encoder->state = 1;
            break;
        case 1: // Append reset pulse
            encoded_symbols += copy_encoder->encode(...);
            break;
    }
}
```
**My Proposed**: Use ESP-IDF `led_strip` component (abstracted)
**Trade-off**:
- **Pro (Emotiscope)**: Full control, can optimize encoder, smaller memory
- **Con (Emotiscope)**: More code to maintain, potential bugs
- **Pro (Mine)**: Maintained by Espressif, proven stable
- **Con (Mine)**: Less control, larger memory (1024 vs 128 symbols)
---
## Recommendations for LightwaveOS-P4 Driver
Based on this analysis, I recommend a **hybrid approach** that takes the best from both:
### Phase 1: Core Driver (Keep as Planned)
- Use ESP-IDF `led_strip_refresh_async()` for parallel transmission
- Keep `CRGB` 8-bit format for FastLED compatibility
- Enable DMA for CPU offload
### Phase 2: Enhanced Double-Buffering (NEW)
Adopt Emotiscope's "wait at start" pattern:
```cpp
void show() {
    // Wait for PREVIOUS frame (not current!)
    static bool first_frame = true;
    if (!first_frame) {
        led_strip_refresh_wait_done(m_rmt_strip1);
        led_strip_refresh_wait_done(m_rmt_strip2);
    }
    first_frame = false;
    
    // Process current frame
    applyBrightnessAndCorrection();
    convertCrgbToRmt();
    
    // Start transmission (returns immediately)
    led_strip_refresh_async(m_rmt_strip1);
    led_strip_refresh_async(m_rmt_strip2);
    // CPU now free for next frame rendering!
}
```
### Phase 3: Add Temporal Dithering (RECOMMENDED)
Port Emotiscope's dithering algorithm:
```cpp
struct DitherError { float r, g, b; };
DitherError m_dither_error[kMaxLedsPerStrip * 2];
void quantizeWithDithering(CRGB* src, uint8_t* dst, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        // Scale and quantize
        float r = src[i].r;
        uint8_t r_out = (uint8_t)r;
        
        // Accumulate error
        float error = r - r_out;
        if (error >= 0.0275f) {
            m_dither_error[i].r += error;
        }
        
        // Apply accumulated error
        if (m_dither_error[i].r >= 1.0f) {
            r_out += 1;
            m_dither_error[i].r -= 1.0f;
        }
        
        dst[i*3] = r_out;
    }
}
```
### Phase 4: Add ESP-DSP Acceleration (OPTIONAL)
For bulk operations:
```cpp
#include "dsps_mulc.h"
#include "dsps_add.h"
void applyBrightness(float* buffer, float scale, size_t count) {
    dsps_mulc_f32_ae32_fast(buffer, buffer, count, scale, 1, 1);
}
```
### Phase 5: Consider Float Pipeline (FUTURE)
If color quality is critical, consider migrating to float internally:
```cpp
struct CRGBF { float r, g, b; };
CRGBF m_leds_float[kMaxLedsPerStrip * 2];
// All effects render to float
// Quantize only at show() time
```
---
## Updated Implementation Plan
### Files to Create/Modify
1. **`LedDriver_P4_RMT.h/cpp`** - Core driver (as previously planned)
2. **Add double-buffering** - Wait at start, not end
3. **Add dithering** - Port Emotiscope algorithm
4. **Optional: ESP-DSP acceleration** - For bulk operations
### Memory Budget
| Component | Emotiscope | My Original | My Updated |
|-----------|------------|-------------|------------|
| Color buffers | 3840 bytes | 960 bytes | 960 bytes |
| RMT buffers | 960 bytes | 960 bytes | 960 bytes |
| Dither error | 3840 bytes | 0 | 3840 bytes |
| **Total** | **8640 bytes** | **1920 bytes** | **5760 bytes** |
Dithering adds 3840 bytes but significantly improves visual quality.
### Performance Budget (per frame @ 120 FPS)
| Operation | Emotiscope | My Proposed |
|-----------|------------|-------------|
| Rendering | 3000 µs | 2000 µs |
| Color processing | 1000 µs | 500 µs |
| Quantization/dithering | 500 µs | 500 µs |
| **Transmission** | **4800 µs** | **4800 µs** |
| **Total** | **9300 µs** | **7800 µs** |
Note: Transmission time overlaps with next frame rendering in double-buffer mode!
---
## Key Lessons from Emotiscope
1. **Double-buffering is critical** - Wait for previous frame, not current
2. **Temporal dithering is worth it** - Significant visual improvement
3. **DMA is optional** - Emotiscope achieves excellent performance without it
4. **Float precision matters** - For effects with blur/blend, consider float pipeline
5. **ESP-DSP acceleration** - Free performance for bulk operations
6. **Smaller RMT buffers work** - 128 symbols is sufficient without DMA
---
## Final Recommendation
**Implement in this order**:
1. **Core parallel RMT driver** (as planned) - Get basic parallel working
2. **Add double-buffering pattern** - Critical for CPU utilization
3. **Add temporal dithering** - Significant visual quality boost
4. **Benchmark and optimize** - Only add ESP-DSP if needed
This approach gives us 80% of Emotiscope's quality with much lower implementation complexity, while maintaining FastLED compatibility.
---
## Conclusion
The Emotiscope codebase represents years of refinement for music visualization. Its key innovations:
- Float color pipeline for HDR
- Temporal dithering for perceived higher bit depth
- Double-buffered transmission for CPU efficiency
- ESP-DSP SIMD acceleration
My proposed driver should adopt:
1. ✅ Parallel RMT transmission (already planned)
2. ✅ Double-buffering pattern (new recommendation)
3. ✅ Temporal dithering (new recommendation)
4. ⏸️ Float pipeline (future consideration)
5. ⏸️ ESP-DSP (if profiling shows need)
This gives us a performant, high-quality driver while maintaining FastLED ecosystem compatibility.