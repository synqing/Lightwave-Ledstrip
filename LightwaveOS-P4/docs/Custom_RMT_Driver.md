# Custom Parallel RMT Driver Implementation Plan
## ESP32-P4 WS2812 Dual-Strip Performance Optimization
**Target**: Achieve 120 FPS with 320 LEDs (2x160) via parallel RMT transmission
**Problem**: Current FastLED implementation likely transmits strips sequentially (~10ms)
**Solution**: Custom driver using ESP-IDF RMT API with explicit parallel async transmission (~5ms)
---
## Executive Summary
### The Physics Problem
- **Sequential transmission**: 320 LEDs × 24 bits × 1.25µs = 9.6ms → 104 FPS max
- **Parallel transmission**: 160 LEDs × 24 bits × 1.25µs = 4.8ms → 208 FPS capability
- **Target**: 8.33ms frame budget (120 FPS)
### Why Custom Driver?
1. FastLED's `FastLED.show()` likely calls controllers sequentially
2. ESP-IDF `led_strip` component already has async API (`led_strip_refresh_async()`)
3. The async API exists in the vendored FastLED code but is unused
4. Explicit control guarantees parallel transmission
### Evidence
- File: `components/FastLED/src/third_party/espressif/led_strip/src/led_strip.h`
  - Lines 84-86: `led_strip_refresh_async()` and `led_strip_refresh_wait_done()` exist
- File: `components/FastLED/src/platforms/esp/32/rmt_5/strip_rmt.cpp`
  - Lines 102-120: FastLED's RmtStrip implements async but called from single show() path
- Current LedDriver_P4.cpp line 121: `FastLED.show()` - black box, likely sequential
---
## Architecture Design
### Component Overview
```
┌─────────────────────────────────────────────────────────────┐
│                    LedDriver_P4_RMT                         │
│  (implements ILedDriver interface - drop-in replacement)    │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ├── CRGB m_strip1[160]  ← FastLED color buffers
                   ├── CRGB m_strip2[160]
                   │
                   ├── led_strip_handle_t m_rmt_strip1  ← ESP-IDF handles
                   ├── led_strip_handle_t m_rmt_strip2
                   │
                   └── Parallel Show Pipeline:
                       1. Apply brightness/color correction
                       2. led_strip_refresh_async(strip1)  ← Start strip1
                       3. led_strip_refresh_async(strip2)  ← Start strip2 (parallel!)
                       4. led_strip_refresh_wait_done(strip1)
                       5. led_strip_refresh_wait_done(strip2)
```
### Key Design Decisions
#### 1. Interface Compatibility
- **Keep `ILedDriver` interface**: No changes to effect rendering code
- **Keep `CRGB` buffers**: Continue using FastLED's color types
- **Keep stats tracking**: Maintain performance monitoring
#### 2. Buffer Management Strategy
```cpp
// Two separate approaches for color data:
Option A: Direct CRGB → RMT (current plan)
┌──────────┐  copy & convert  ┌──────────┐  RMT encoder  ┌──────┐
│ CRGB[160]│ ──────────────→  │ uint8_t  │ ────────────→ │ LEDs │
│  buffer  │   per show()     │ [160*3]  │   via DMA     │      │
└──────────┘                  └──────────┘               └──────┘
  FastLED types                RMT pixel buffer          Hardware
Option B: Shared buffer (optimization for later)
┌──────────────────────┐  RMT encoder  ┌──────┐
│ Union {              │ ────────────→ │ LEDs │
│   CRGB crgb[160];   │   via DMA     │      │
│   uint8_t raw[480]; │               │      │
│ }                    │               │      │
└──────────────────────┘               └──────┘
  Zero-copy buffer                     Hardware
```
**Decision**: Use Option A initially (simpler, safer). Option B is an optimization for later if profiling shows significant overhead.
#### 3. Color Processing Pipeline
```cpp
void show() {
    // 1. Apply brightness (software gamma)
    //    - Iterate through CRGB buffers
    //    - Scale RGB values by brightness (0-255)
    //    - Optional: apply gamma correction table
    
    // 2. Apply color correction
    //    - Multiply by TypicalLEDStrip correction
    //    - Result: corrected CRGB values
    
    // 3. Convert CRGB → RMT format
    //    - For each LED: extract R, G, B bytes
    //    - Store in RMT's pixel_buf (GRB order for WS2812)
    
    // 4. Transmit in parallel
    //    - Start both RMT channels asynchronously
    //    - Wait for both to complete
}
```
#### 4. Power Limiting Strategy
FastLED's `setMaxPowerInVoltsAndMilliamps()` is complex. Initial implementation:
- **Simple approach**: Calculate total power, scale brightness if exceeded
- **Formula**: `power_mW = sum(R+G+B) * 60mA / 255 * 5V`
- **If over limit**: `brightness_scale = max_power / calculated_power`
#### 5. DMA Configuration
- **Enable DMA**: `rmt_config.flags.with_dma = true`
- **Memory block**: 1024 symbols (sufficient for WS2812 encoding)
- **Transaction queue**: 4 buffers for double-buffering
---
## Implementation Phases
### Phase 1: Instrumentation (Validate Diagnosis)
**Goal**: Confirm FastLED is sequential and measure actual show() time
**Steps**:
1. Add detailed timing to existing `LedDriver_P4::show()`
   ```cpp
   void show() {
       uint32_t start = esp_timer_get_time();
       
       uint32_t pre_show = esp_timer_get_time();
       FastLED.show();
       uint32_t post_show = esp_timer_get_time();
       
       updateShowStats(post_show - start);
       
       // Log detailed breakdown every 120 frames
       if (m_stats.frameCount % 120 == 0) {
           LW_LOGI("Show timing: total=%uus, fastled=%uus",
                   post_show - start, post_show - pre_show);
       }
   }
   ```
2. Build and flash current firmware
3. Monitor serial output at 120 FPS rendering load
4. **Expected result**: 9-10ms show time (confirms sequential bottleneck)
5. **If show time is <6ms**: Stop here, problem is elsewhere
**Files Modified**:
- `firmware/v2/src/hal/esp32p4/LedDriver_P4.cpp` (add logging)
**Success Criteria**:
- Measured show() time >8ms with 320 LEDs
- Log output confirms diagnosis
---
### Phase 2: Minimal Parallel Driver Prototype
**Goal**: Create working parallel RMT driver with minimal features
**Files to Create**:
1. `firmware/v2/src/hal/esp32p4/LedDriver_P4_RMT.h`
2. `firmware/v2/src/hal/esp32p4/LedDriver_P4_RMT.cpp`
**Implementation Details**:
#### LedDriver_P4_RMT.h
```cpp
#pragma once
#include "hal/interface/ILedDriver.h"
#include "config/chip_config.h"
#ifndef NATIVE_BUILD
#include "led_strip.h"  // ESP-IDF component
#endif
namespace lightwaveos {
namespace hal {
class LedDriver_P4_RMT : public ILedDriver {
public:
    LedDriver_P4_RMT();
    ~LedDriver_P4_RMT() override;
    // ILedDriver interface implementation
    bool init(const LedStripConfig& config) override;
    bool initDual(const LedStripConfig& config1, const LedStripConfig& config2) override;
    void deinit() override;
    CRGB* getBuffer() override;
    CRGB* getBuffer(uint8_t stripIndex) override;
    uint16_t getTotalLedCount() const override;
    uint16_t getLedCount(uint8_t stripIndex) const override;
    void show() override;
    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() const override;
    void setMaxPower(uint8_t volts, uint16_t milliamps) override;
    void clear(bool show = false) override;
    void fill(CRGB color, bool show = false) override;
    void setPixel(uint16_t index, CRGB color) override;
    bool isInitialized() const override { return m_initialized; }
    const LedDriverStats& getStats() const override { return m_stats; }
    void resetStats() override;
private:
    static constexpr uint16_t kMaxLedsPerStrip = 160;
    static constexpr uint8_t kBytesPerPixel = 3;  // GRB for WS2812
    // Configuration
    LedStripConfig m_config1{};
    LedStripConfig m_config2{};
    uint16_t m_stripCounts[2] = {0, 0};
    uint16_t m_totalLeds = 0;
    uint8_t m_brightness = 128;
    uint16_t m_maxMilliamps = 3000;
    bool m_initialized = false;
    bool m_dual = false;
    // FastLED color buffers (effects write here)
    CRGB m_strip1[kMaxLedsPerStrip];
    CRGB m_strip2[kMaxLedsPerStrip];
#ifndef NATIVE_BUILD
    // ESP-IDF RMT handles
    led_strip_handle_t m_rmt_strip1 = nullptr;
    led_strip_handle_t m_rmt_strip2 = nullptr;
    
    // RMT pixel buffers (GRB byte arrays)
    uint8_t m_rmt_buffer1[kMaxLedsPerStrip * kBytesPerPixel];
    uint8_t m_rmt_buffer2[kMaxLedsPerStrip * kBytesPerPixel];
#endif
    // Statistics
    LedDriverStats m_stats{};
    // Helper methods
    bool initRmtStrip(uint8_t gpio, uint16_t ledCount, 
                      led_strip_handle_t* handle);
    void updateShowStats(uint32_t showUs);
    void applyBrightnessAndCorrection();
    void convertCrgbToRmt(const CRGB* src, uint8_t* dst, uint16_t count);
    uint32_t calculatePowerMilliwatts() const;
    void scaleBrightnessForPower();
};
} // namespace hal
} // namespace lightwaveos
```
#### LedDriver_P4_RMT.cpp - Core Implementation
**Key Functions**:
##### 1. Initialization
```cpp
bool LedDriver_P4_RMT::initDual(const LedStripConfig& config1, 
                                 const LedStripConfig& config2) {
    m_dual = true;
    m_config1 = config1;
    m_config2 = config2;
    m_stripCounts[0] = config1.ledCount;
    m_stripCounts[1] = config2.ledCount;
    m_totalLeds = config1.ledCount + config2.ledCount;
    m_brightness = config1.brightness;
    // Validate counts
    if (config1.ledCount > kMaxLedsPerStrip || 
        config2.ledCount > kMaxLedsPerStrip) {
        LW_LOGE("LED count exceeds max: %u/%u > %u", 
                config1.ledCount, config2.ledCount, kMaxLedsPerStrip);
        return false;
    }
#ifndef NATIVE_BUILD
    // Initialize RMT strip 1
    if (!initRmtStrip(chip::gpio::LED_STRIP1_DATA, 
                      config1.ledCount, &m_rmt_strip1)) {
        LW_LOGE("Failed to init RMT strip 1");
        return false;
    }
    // Initialize RMT strip 2
    if (!initRmtStrip(chip::gpio::LED_STRIP2_DATA, 
                      config2.ledCount, &m_rmt_strip2)) {
        LW_LOGE("Failed to init RMT strip 2");
        led_strip_del(m_rmt_strip1);
        return false;
    }
    // Clear both strips
    memset(m_rmt_buffer1, 0, sizeof(m_rmt_buffer1));
    memset(m_rmt_buffer2, 0, sizeof(m_rmt_buffer2));
    
    // Initial refresh to clear LEDs
    led_strip_clear(m_rmt_strip1);
    led_strip_clear(m_rmt_strip2);
#endif
    memset(m_strip1, 0, sizeof(m_strip1));
    memset(m_strip2, 0, sizeof(m_strip2));
    
    m_initialized = true;
    LW_LOGI("RMT driver init: 2x%u LEDs on GPIO %u/%u", 
            config1.ledCount, 
            chip::gpio::LED_STRIP1_DATA, 
            chip::gpio::LED_STRIP2_DATA);
    return true;
}
bool LedDriver_P4_RMT::initRmtStrip(uint8_t gpio, uint16_t ledCount,
                                     led_strip_handle_t* handle) {
    // WS2812 timing (from datasheet)
    led_strip_encoder_timings_t timings = {
        .t0h = 400,   // T0H: 0.4µs ± 150ns
        .t0l = 850,   // T0L: 0.85µs ± 150ns
        .t1h = 800,   // T1H: 0.8µs ± 150ns
        .t1l = 450,   // T1L: 0.45µs ± 150ns
        .reset = 50000 // Reset: >50µs
    };
    // LED strip configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = ledCount,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
        .timings = timings
    };
    // RMT configuration
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10 MHz
        .mem_block_symbols = 1024,          // DMA mode needs larger buffer
        .flags = {
            .with_dma = true,  // CRITICAL: Enable DMA for performance
        },
        .interrupt_priority = 1,
    };
    // Create RMT device
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, 
                                              &rmt_config, 
                                              handle);
    if (ret != ESP_OK) {
        LW_LOGE("led_strip_new_rmt_device failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}
```
##### 2. Parallel Show (Core Performance Feature)
```cpp
void LedDriver_P4_RMT::show() {
#ifndef NATIVE_BUILD
    uint32_t start = esp_timer_get_time();
    // Step 1: Apply brightness and color correction to CRGB buffers
    //         (modifies m_strip1/m_strip2 in-place)
    uint32_t t1 = esp_timer_get_time();
    applyBrightnessAndCorrection();
    uint32_t t2 = esp_timer_get_time();
    // Step 2: Convert CRGB → RMT byte format (GRB order)
    convertCrgbToRmt(m_strip1, m_rmt_buffer1, m_stripCounts[0]);
    if (m_dual) {
        convertCrgbToRmt(m_strip2, m_rmt_buffer2, m_stripCounts[1]);
    }
    uint32_t t3 = esp_timer_get_time();
    // Step 3: Copy buffers to RMT internal memory
    for (uint16_t i = 0; i < m_stripCounts[0]; i++) {
        uint8_t* pixel = &m_rmt_buffer1[i * kBytesPerPixel];
        led_strip_set_pixel(m_rmt_strip1, i, pixel[1], pixel[0], pixel[2]);
    }
    if (m_dual) {
        for (uint16_t i = 0; i < m_stripCounts[1]; i++) {
            uint8_t* pixel = &m_rmt_buffer2[i * kBytesPerPixel];
            led_strip_set_pixel(m_rmt_strip2, i, pixel[1], pixel[0], pixel[2]);
        }
    }
    uint32_t t4 = esp_timer_get_time();
    // Step 4: START PARALLEL TRANSMISSION (key optimization!)
    esp_err_t ret1 = led_strip_refresh_async(m_rmt_strip1);
    esp_err_t ret2 = ESP_OK;
    if (m_dual) {
        ret2 = led_strip_refresh_async(m_rmt_strip2);
    }
    uint32_t t5 = esp_timer_get_time();
    // Step 5: Wait for BOTH to complete
    if (ret1 == ESP_OK) {
        led_strip_refresh_wait_done(m_rmt_strip1);
    }
    if (m_dual && ret2 == ESP_OK) {
        led_strip_refresh_wait_done(m_rmt_strip2);
    }
    uint32_t end = esp_timer_get_time();
    // Update statistics
    updateShowStats(end - start);
    // Detailed timing log every 2 seconds (~240 frames at 120 FPS)
    if (m_stats.frameCount % 240 == 0) {
        LW_LOGI("Show breakdown: brightness=%uus, convert=%uus, "
                "copy=%uus, transmit=%uus, total=%uus",
                t2 - t1, t3 - t2, t4 - t3, end - t5, end - start);
    }
#else
    m_stats.frameCount++;
#endif
}
```
##### 3. Brightness and Color Correction
```cpp
void LedDriver_P4_RMT::applyBrightnessAndCorrection() {
    // Apply brightness scaling to both strips
    // Note: FastLED's nscale8() is optimized for this
    
    if (m_brightness == 255) {
        // No scaling needed, skip
        return;
    }
    // Strip 1
    for (uint16_t i = 0; i < m_stripCounts[0]; i++) {
        m_strip1[i].nscale8(m_brightness);
    }
    // Strip 2
    if (m_dual) {
        for (uint16_t i = 0; i < m_stripCounts[1]; i++) {
            m_strip2[i].nscale8(m_brightness);
        }
    }
    // Color correction (TypicalLEDStrip)
    // R: 255, G: 176, B: 240
    // Formula: color = (color * correction) / 255
    
    CRGB correction = m_config1.colorCorrection;
    if (correction != CRGB(255, 255, 255)) {
        for (uint16_t i = 0; i < m_stripCounts[0]; i++) {
            m_strip1[i].r = (m_strip1[i].r * correction.r) / 255;
            m_strip1[i].g = (m_strip1[i].g * correction.g) / 255;
            m_strip1[i].b = (m_strip1[i].b * correction.b) / 255;
        }
        
        if (m_dual) {
            for (uint16_t i = 0; i < m_stripCounts[1]; i++) {
                m_strip2[i].r = (m_strip2[i].r * correction.r) / 255;
                m_strip2[i].g = (m_strip2[i].g * correction.g) / 255;
                m_strip2[i].b = (m_strip2[i].b * correction.b) / 255;
            }
        }
    }
}
```
##### 4. CRGB to RMT Conversion
```cpp
void LedDriver_P4_RMT::convertCrgbToRmt(const CRGB* src, 
                                         uint8_t* dst, 
                                         uint16_t count) {
    // WS2812 uses GRB order (not RGB!)
    for (uint16_t i = 0; i < count; i++) {
        dst[i * 3 + 0] = src[i].g;  // Green first
        dst[i * 3 + 1] = src[i].r;  // Red second
        dst[i * 3 + 2] = src[i].b;  // Blue third
    }
}
```
##### 5. Power Limiting (Simplified)
```cpp
void LedDriver_P4_RMT::setMaxPower(uint8_t volts, uint16_t milliamps) {
    m_maxMilliamps = milliamps;
}
uint32_t LedDriver_P4_RMT::calculatePowerMilliwatts() const {
    // Approximation: 60mA per LED at full white
    // Power = (sum of RGB values / 765) * 60mA * LED_count * 5V
    
    uint32_t totalBrightness = 0;
    
    for (uint16_t i = 0; i < m_stripCounts[0]; i++) {
        totalBrightness += m_strip1[i].r + m_strip1[i].g + m_strip1[i].b;
    }
    
    if (m_dual) {
        for (uint16_t i = 0; i < m_stripCounts[1]; i++) {
            totalBrightness += m_strip2[i].r + m_strip2[i].g + m_strip2[i].b;
        }
    }
    
    // Power in milliwatts: (brightness / 765) * 60mA * 5000mV
    // Simplified: brightness * 392 / 1000
    return (totalBrightness * 392) / 1000;
}
void LedDriver_P4_RMT::scaleBrightnessForPower() {
    uint32_t powerMw = calculatePowerMilliwatts();
    uint32_t maxPowerMw = m_maxMilliamps * 5;  // 5V supply
    
    if (powerMw > maxPowerMw) {
        // Scale down brightness proportionally
        uint8_t scaledBrightness = (m_brightness * maxPowerMw) / powerMw;
        m_brightness = scaledBrightness;
    }
}
```
**Success Criteria**:
- Compiles without errors
- Initializes both RMT channels successfully
- `show()` executes in <5ms with 320 LEDs
- LEDs display correct colors
---
### Phase 3: Integration & Testing
**Goal**: Replace FastLED driver with RMT driver and validate performance
**Steps**:
1. **Update HalFactory.h**:
   ```cpp
   #if CHIP_ESP32_P4
       #include "hal/esp32p4/LedDriver_P4_RMT.h"
       inline ILedDriver* createLedDriver() {
           return new LedDriver_P4_RMT();
       }
   #elif CHIP_ESP32_S3
       // ... existing S3 code
   #endif
   ```
2. **Build Configuration**:
   - Add to `esp-idf-p4/main/CMakeLists.txt`:
     ```cmake
     # Ensure led_strip component is linked
     idf_component_register(
         ...
         REQUIRES ... led_strip  # Add this
     )
     ```
3. **Flash and Test**:
   - Build with `./build_with_idf55.sh`
   - Flash with `./flash_and_monitor.sh`
   - Monitor serial output for timing logs
4. **Performance Validation**:
   - Observe show() timing: **expect <5ms**
   - Verify FPS: **expect 120+ FPS sustained**
   - Check LED output: colors accurate, no flickering
**Files Modified**:
- `firmware/v2/src/hal/HalFactory.h`
- `firmware/v2/esp-idf-p4/main/CMakeLists.txt`
**Success Criteria**:
- Show time consistently <5ms
- No visual artifacts on LEDs
- Effects render smoothly at 120 FPS
---
### Phase 4: Feature Parity (Optional Enhancements)
**Goal**: Add features to match FastLED capabilities
**Optional Features** (in priority order):
1. **Dithering** (low priority):
   - Temporal dithering to reduce banding
   - Add noise to LSBs before scaling
2. **Gamma Correction** (medium priority):
   - Apply gamma curve for perceptually linear brightness
   - Use lookup table for performance
3. **Enhanced Power Limiting** (medium priority):
   - Per-channel current monitoring
   - Soft limiting with gradual scale-down
4. **Error Recovery** (high priority):
   - Detect RMT transmission failures
   - Retry logic with exponential backoff
**Implementation Note**: Only add features if profiling shows available CPU time.
---
## File Structure
```
firmware/v2/src/hal/esp32p4/
├── LedDriver_P4.cpp           # OLD: FastLED-based (keep for fallback)
├── LedDriver_P4.h
├── LedDriver_P4_RMT.cpp       # NEW: Custom RMT driver
└── LedDriver_P4_RMT.h
firmware/v2/src/hal/
└── HalFactory.h               # MODIFIED: Switch to new driver
firmware/v2/esp-idf-p4/main/
└── CMakeLists.txt             # MODIFIED: Add led_strip dependency
```
---
## Testing Strategy
### Unit Testing (Phase 2)
1. **Single strip mode**:
   - Init with 160 LEDs, verify buffer allocation
   - Set all pixels to red, verify RMT buffer conversion
   - Call show(), measure timing
2. **Dual strip mode**:
   - Init with 2x160 LEDs
   - Set strip1 to red, strip2 to blue
   - Verify both display correctly
3. **Brightness scaling**:
   - Set brightness to 128
   - Verify pixels are scaled (not full intensity)
### Integration Testing (Phase 3)
1. **Effect rendering**:
   - Run standard effects (solid color, rainbow, etc.)
   - Verify visual output matches expected
2. **Performance monitoring**:
   - Log show() timing every second
   - Verify <5ms consistently
3. **Power limiting**:
   - Set max power to 1A (1000mA)
   - Display full white on all LEDs
   - Verify brightness scales down
### Stress Testing
1. **Sustained load**:
   - Run at 120 FPS for 10 minutes
   - Monitor for memory leaks, crashes
   - Verify timing remains consistent
2. **Rapid effect switching**:
   - Change effects every 100ms
   - Verify no corruption or artifacts
---
## Performance Expectations
### Timing Breakdown (Target)
| Operation | Time (µs) | % of Frame |
|-----------|-----------|------------|
| Brightness/correction | 500 | 6% |
| CRGB → RMT conversion | 300 | 4% |
| RMT buffer copy | 400 | 5% |
| **Parallel transmission** | **4800** | **58%** |
| **Total show()** | **6000** | **72%** |
| Effect rendering | 2000 | 24% |
| **Frame total** | **8000** | **96%** |
**Headroom**: 333µs (4%) for other tasks
### Expected Improvements
| Metric | Before (FastLED) | After (Custom RMT) | Improvement |
|--------|------------------|-------------------|-------------|
| Show time | ~10ms | ~5ms | **2x faster** |
| Max FPS | 100 | 120+ | **20% gain** |
| Frame budget usage | 120% | 96% | **24% margin** |
| LED update rate | Sequential | Parallel | **Guaranteed** |
---
## Risk Mitigation
### Risk 1: RMT Timing Issues
**Symptom**: LEDs display wrong colors or flicker
**Mitigation**:
- Use proven WS2812 timings (from datasheet)
- Compare with FastLED's working values
- Add logic analyzer validation
### Risk 2: Memory Overhead
**Symptom**: Out of memory during init
**Mitigation**:
- RMT buffers: 960 bytes (2×160×3)
- CRGB buffers: 1920 bytes (already allocated)
- Total new allocation: <1KB → acceptable
### Risk 3: DMA Interference
**Symptom**: Audio or other DMA peripherals conflict
**Mitigation**:
- ESP32-P4 has 4 RMT channels, 2 needed
- Audio uses I2S DMA (separate controller)
- No expected conflicts
### Risk 4: Show() Still Slow
**Symptom**: Measured time still >8ms
**Mitigation**:
- Profile with detailed timing logs
- Identify bottleneck (conversion? RMT setup?)
- Optimize hot path (e.g., zero-copy buffers)
---
## Rollback Plan
If custom driver fails or doesn't improve performance:
1. **Keep LedDriver_P4.cpp** (FastLED version) as fallback
2. **Compile-time switch** in HalFactory.h:
   ```cpp
   #define USE_CUSTOM_RMT_DRIVER 1  // Set to 0 to revert
   ```
3. **No changes to application code** - interface is identical
---
## Dependencies
### ESP-IDF Components
- `led_strip` v3.0.2 (already in dependencies.lock)
- `driver` (RMT peripheral drivers)
- `esp_timer` (microsecond timing)
### Build System
- ESP-IDF v5.5.2 (locked)
- CMake 3.16+
### External Libraries
- FastLED (for CRGB types only, not RMT backend)
---
## Success Criteria Summary
### Phase 1 (Instrumentation)
- Measured show() time >8ms confirms sequential hypothesis
- Detailed timing logs available for analysis
### Phase 2 (Prototype)
- Custom driver compiles without errors
- Both RMT channels initialize successfully
- Show() executes in <5ms
- LEDs display correct colors
### Phase 3 (Integration)
- Effects render correctly at 120 FPS
- No visual artifacts or flickering
- Performance metrics show consistent <5ms show time
### Final Validation
- Sustained 120 FPS operation for 10+ minutes
- Power limiting works correctly
- No memory leaks or stability issues
- Audio and LED synchronization maintained
---
## Timeline Estimate
| Phase | Time | Cumulative |
|-------|------|------------|
| Phase 1: Instrumentation | 30 minutes | 30 min |
| Phase 2: Driver Implementation | 4-6 hours | 5 hours |
| Phase 3: Integration & Testing | 2-3 hours | 8 hours |
| Phase 4: Polish (optional) | 2-4 hours | 12 hours |
**Recommended approach**: Complete Phases 1-3 in one session for momentum.
---
## Key Technical References
### WS2812B Timing (from datasheet)
- T0H: 0.4µs ± 150ns (logic 0, high time)
- T0L: 0.85µs ± 150ns (logic 0, low time)
- T1H: 0.8µs ± 150ns (logic 1, high time)
- T1L: 0.45µs ± 150ns (logic 1, low time)
- Reset: >50µs (low time between frames)
### ESP-IDF RMT API
- `led_strip_new_rmt_device()`: Initialize RMT channel
- `led_strip_set_pixel()`: Set RGB values
- `led_strip_refresh_async()`: Start transmission (non-blocking)
- `led_strip_refresh_wait_done()`: Wait for completion
- `led_strip_del()`: Free resources
### FastLED Color Types
- `CRGB`: 3-byte RGB struct
- `CRGB::nscale8()`: Brightness scaling
- `TypicalLEDStrip`: Color correction constant (255, 176, 240)
---
## Conclusion
This custom RMT driver implementation directly addresses the physics constraint of sequential LED transmission. By leveraging ESP-IDF's async RMT API, we guarantee parallel transmission of both strips, cutting show time in half from ~10ms to ~5ms. This brings the ESP32-P4's 320-LED music visualizer solidly within the 120 FPS performance target.
The implementation is **low-risk**: it reuses proven ESP-IDF components, maintains the existing ILedDriver interface, and has a clear rollback path. The **high-reward** is guaranteed parallel transmission and elimination of the performance bottleneck.
**Recommendation**: Proceed with implementation.