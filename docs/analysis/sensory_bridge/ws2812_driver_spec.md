# WS2812 LED Driver Implementation Specification

## Overview

The Sensory Bridge firmware uses the **FastLED library** to drive WS2812B (NeoPixel) and APA102 (DotStar) addressable RGB LEDs. This document reverse-engineers the LED driver implementation from the source code.

## FastLED Library Integration

### Initialisation (`led_utilities.h::init_leds()`)

**LED Type Selection**:

```cpp
if (CONFIG.LED_TYPE == LED_NEOPIXEL) {
    if (CONFIG.LED_COLOR_ORDER == RGB) {
        FastLED.addLeds<WS2812B, LED_DATA_PIN, RGB>(leds_out, CONFIG.LED_COUNT);
    }
    else if (CONFIG.LED_COLOR_ORDER == GRB) {
        FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds_out, CONFIG.LED_COUNT);
    }
    else if (CONFIG.LED_COLOR_ORDER == BGR) {
        FastLED.addLeds<WS2812B, LED_DATA_PIN, BGR>(leds_out, CONFIG.LED_COUNT);
    }
}
else if (CONFIG.LED_TYPE == LED_DOTSTAR) {
    // Similar for DOTSTAR with clock pin
    FastLED.addLeds<DOTSTAR, LED_DATA_PIN, LED_CLOCK_PIN, GRB>(leds_out, CONFIG.LED_COUNT);
}
```

**Configuration**:
- **Template Parameters**:
  - `WS2812B` or `DOTSTAR` - LED chip type
  - `LED_DATA_PIN` - GPIO pin for data (default: 36)
  - `RGB/GRB/BGR` - Colour order (default: GRB for NeoPixel)
  - `LED_CLOCK_PIN` - GPIO pin for clock (DotStar only, default: 37)

- **Runtime Parameters**:
  - `leds_out` - Pointer to `CRGB` array
  - `CONFIG.LED_COUNT` - Number of LEDs (default: 128)

**Power Limiting**:
```cpp
FastLED.setMaxPowerInVoltsAndMilliamps(5.0, CONFIG.MAX_CURRENT_MA);
```
- Default: 1500 mA maximum current
- FastLED automatically scales brightness to stay within limit

## Colour Data Structures

### CRGB Structure (FastLED)

**Definition**: `struct CRGB { uint8_t r, g, b; }`

**Memory Layout**: 3 bytes per LED (24 bits total)

**Native Resolution**: 128 LEDs = 384 bytes

**Output Buffer**: `leds_out[]` - Dynamically allocated based on `CONFIG.LED_COUNT`

## Colour Manipulation Pipeline

### Stage 1: Mode Rendering → HSV Conversion

**Location**: `lightshow_modes.h` (various mode functions)

**Process**:
1. Calculate brightness from spectrogram/chromagram (0.0-1.0 float)
2. Apply contrast enhancement (squaring operation)
3. Convert to 8-bit brightness: `uint8_t brightness = 254 * bin`
4. Select hue (chromatic mode or user-selected)
5. **HSV to RGB Conversion**:
   ```cpp
   hsv2rgb_spectrum(CHSV(led_hue, 255, brightness_levels[i]), col1);
   ```

**HSV Parameters**:
- **Hue**: 0-255 (wraps, 256 = full cycle)
- **Saturation**: Always 255 (full saturation)
- **Value (Brightness)**: 0-255 (from spectrogram magnitude)

**`hsv2rgb_spectrum()` Function**:
- FastLED's "spectrum" variant (better low-light colour resolution than default "rainbow")
- Uses lookup tables for fast conversion
- Output: `CRGB` structure with `r`, `g`, `b` values

### Stage 2: Temporal Dithering

**Location**: `lightshow_modes.h` (mode functions), `led_utilities.h::show_leds()`

**Purpose**: Simulate higher bit-depth brightness by alternating between adjacent brightness levels

**Implementation**:
```cpp
float led_brightness_raw = 254 * bin;
uint16_t led_brightness = led_brightness_raw;
float fract = led_brightness_raw - led_brightness;  // Fractional part

if (CONFIG.TEMPORAL_DITHERING == true) {
    if (fract >= dither_table[dither_step]) {
        led_brightness += 1;  // Round up
    }
    // else: Round down (truncate)
}
```

**Dither Table**:
```cpp
const float dither_table[8] = {
    0.125, 0.250, 0.375, 0.500,
    0.625, 0.750, 0.875, 1.000
};
```

**Step Counter**:
```cpp
dither_step++;
if (dither_step >= 8) dither_step = 0;  // Cycles every 8 frames
```

**Effect**: 
- Provides 8× brightness resolution (effectively 11-bit brightness from 8-bit)
- Reduces banding in low-light conditions
- Imperceptible at high frame rates (>60 FPS)

### Stage 3: Resolution Conversion

**Location**: `led_utilities.h::show_leds()`

**Purpose**: Map 128-LED native resolution to arbitrary LED count

#### Direct Copy (Native Resolution)

```cpp
if (CONFIG.LED_COUNT == NATIVE_RESOLUTION) {
    memcpy(leds_out, leds, sizeof(leds));
}
```

**Timing**: ~0.1 ms (384-byte memcpy)

#### Linear Interpolation (Non-Native, Enabled)

```cpp
if (CONFIG.LED_INTERPOLATION == true) {
    float index_push = float(1) / float(CONFIG.LED_COUNT);
    float index = 0.0;
    
    for (uint16_t i = 0; i < CONFIG.LED_COUNT; i++) {
        leds_out[i] = lerp_led(index, leds);
        index += index_push;
    }
}
```

**`lerp_led()` Function**:
```cpp
CRGB lerp_led(float index, CRGB* led_array) {
    float NUM_LEDS_NATIVE = NATIVE_RESOLUTION - 1;
    float index_f = index * NUM_LEDS_NATIVE;
    int index_i = (int)index_f;
    float index_f_frac = index_f - index_i;
    
    CRGB out_col = CRGB(0, 0, 0);
    out_col.r += (1 - index_f_frac) * led_array[index_i].r;
    out_col.g += (1 - index_f_frac) * led_array[index_i].g;
    out_col.b += (1 - index_f_frac) * led_array[index_i].b;
    
    out_col.r += index_f_frac * led_array[index_i + 1].r;
    out_col.g += index_f_frac * led_array[index_i + 1].g;
    out_col.b += index_f_frac * led_array[index_i + 1].b;
    
    return out_col;
}
```

**Process**:
- Maps LED index in output array to fractional position in native array
- Linearly interpolates RGB values between adjacent native LEDs
- Provides smooth colour transitions for non-native resolutions

**Timing**: ~0.5-2 ms (depends on LED count, ~10-20 μs per LED)

#### Direct Sampling (Non-Native, Disabled)

```cpp
else {
    float index_push = float(NATIVE_RESOLUTION) / float(CONFIG.LED_COUNT);
    float index = 0.0;
    
    for (uint16_t i = 0; i < CONFIG.LED_COUNT; i++) {
        leds_out[i] = leds[uint8_t(index)];
        index += index_push;
    }
}
```

**Process**:
- Maps LED index to nearest native LED (no interpolation)
- Faster but may show aliasing/stepping

**Timing**: ~0.1-0.5 ms (depends on LED count, ~1-5 μs per LED)

### Stage 4: Reverse Order

**Location**: `led_utilities.h::show_leds()`

**Purpose**: Flip LED strip orientation

```cpp
if (CONFIG.REVERSE_ORDER == true) {
    reverse_leds(leds_out, CONFIG.LED_COUNT);
}
```

**Implementation**:
```cpp
void reverse_leds(CRGB arr[], uint16_t size) {
    uint16_t start = 0;
    uint16_t end = size - 1;
    while (start < end) {
        CRGB temp = arr[start];
        arr[start] = arr[end];
        arr[end] = temp;
        start++;
        end--;
    }
}
```

**Timing**: ~0.2 ms (64 swaps for 128 LEDs)

### Stage 5: Brightness Application

**Location**: `led_utilities.h::show_leds()`

**Brightness Calculation**:
```cpp
FastLED.setBrightness((255 * MASTER_BRIGHTNESS) * 
                      (CONFIG.PHOTONS * CONFIG.PHOTONS) * 
                      silent_scale);
```

**Components**:
1. **`MASTER_BRIGHTNESS`**: 0.0-1.0 fade-in control (boot animation)
   - Starts at 0.0
   - Increments by 0.005 per frame after 1 second
   - Reaches 1.0 after ~200 frames (~2 seconds)

2. **`CONFIG.PHOTONS²`**: User brightness knob (squared for logarithmic response)
   - Range: 0.0-1.0 (from analog knob)
   - Squared: `PHOTONS * PHOTONS` (0.0-1.0 range maintained)
   - Provides more intuitive control (50% knob = ~25% brightness)

3. **`silent_scale`**: Standby dimming (0.0-1.0)
   - 1.0 during audio
   - 0.0 during silence (if `STANDBY_DIMMING` enabled)
   - Smoothed: `silent_scale = silent_scale_raw * 0.1 + silent_scale_last * 0.9`

**Final Brightness**: `0-255` range (8-bit)

**Implementation**: FastLED applies brightness **after** colour data is prepared (scales RGB values)

### Stage 6: FastLED Dithering

**Location**: `led_utilities.h::show_leds()`

```cpp
FastLED.setDither(CONFIG.TEMPORAL_DITHERING);
```

**Purpose**: Additional temporal dithering at library level

**Effect**: FastLED may apply its own dithering algorithm (library-dependent)

**Note**: This is **in addition to** the manual dithering applied in mode rendering

### Stage 7: Hardware Output (`FastLED.show()`)

**Location**: `led_utilities.h::show_leds()`

```cpp
FastLED.show();
```

**Process**: FastLED library sends `leds_out[]` data to hardware

## WS2812B Protocol Implementation (FastLED)

### Protocol Specification

**Bit Encoding** (WS2812B):
- **Bit Period**: 1.25 μs (800 kHz)
- **"0" Bit**: 0.3 μs HIGH, 0.95 μs LOW
- **"1" Bit**: 0.6 μs HIGH, 0.65 μs LOW
- **Reset Time**: 50+ μs LOW (between frames)

**Data Format**:
- **24 bits per LED**: 8 bits Green, 8 bits Red, 8 bits Blue (GRB order)
- **Transmission Order**: MSB first for each colour
- **Total Bits**: `LED_COUNT × 24 bits`

### FastLED Implementation Details

**Hardware Method**: FastLED uses **ESP32 RMT peripheral** or **bit-banging** (implementation-dependent)

**RMT Method** (Preferred):
- ESP32 RMT (Remote Control) peripheral generates precise timing
- DMA-driven (no CPU overhead during transmission)
- Supports multiple channels (if multiple strips)

**Bit-Banging Method** (Fallback):
- Software-generated timing (CPU-intensive)
- Less precise timing
- Blocks CPU during transmission

**Transmission Time Calculation**:
```
Time = (LED_COUNT × 24 bits × 1.25 μs) + 50 μs
     = (128 × 24 × 1.25) + 50
     = 3,840 + 50
     = 3,890 μs
     = 3.89 ms
```

**Measured**: ~3.8-4.0 ms (includes protocol overhead)

### DotStar (APA102) Protocol

**Protocol Specification**:
- **SPI-based**: Uses standard SPI peripheral
- **Start Frame**: 32 bits of zeros
- **LED Frame**: 32 bits per LED (8-bit brightness + 24-bit RGB)
- **End Frame**: `LED_COUNT / 2` bits of ones (latch)

**Advantages**:
- Faster than WS2812B (SPI clock can be higher)
- More reliable (clocked protocol)
- Supports brightness control per LED

**Disadvantages**:
- Requires clock pin (4-wire vs 3-wire)
- Slightly more complex wiring

**Transmission Time**: ~2-3 ms for 128 LEDs (SPI-dependent, typically faster than WS2812B)

## Colour Order Handling

### Configuration

**Colour Orders Supported**:
- `RGB` - Red, Green, Blue
- `GRB` - Green, Red, Blue (WS2812B default)
- `BGR` - Blue, Green, Red

**Default**: `GRB` for NeoPixel, `BGR` for DotStar

**Why Different Orders?**:
- Different LED manufacturers use different bit orders
- FastLED handles reordering automatically based on template parameter

### FastLED Reordering

**Process**: FastLED reorders RGB data **before transmission** based on template parameter

**Example** (GRB order):
```cpp
// Input: CRGB{r=255, g=128, b=64}
// FastLED reorders to: {g=128, r=255, b=64}
// Transmitted as: Green(128), Red(255), Blue(64)
```

**Timing**: Negligible (compile-time reordering or fast runtime swap)

## Hardware-Specific Optimisations

### ESP32-S3 Optimisations

1. **RMT Peripheral**: Hardware timing generation (no CPU during transmission)
2. **DMA**: Direct memory access for buffer transfers
3. **Dual Core**: LED thread runs on Core 1 (isolated from audio processing)

### FastLED Optimisations

1. **Lookup Tables**: HSV→RGB conversion uses precomputed tables
2. **SIMD**: May use SIMD instructions for brightness scaling (if available)
3. **DMA Buffering**: Double-buffering for smooth updates

## Performance Characteristics

### Transmission Timing

| LED Count | WS2812B Time | DotStar Time | Notes |
|-----------|--------------|--------------|-------|
| 64 | ~1.9 ms | ~1-1.5 ms | Half resolution |
| 128 | ~3.8 ms | ~2-3 ms | Native resolution |
| 256 | ~7.7 ms | ~4-6 ms | Double resolution |
| 512 | ~15.4 ms | ~8-12 ms | Large strip |

### CPU Usage

**During Transmission**:
- **RMT Method**: ~0% CPU (DMA-driven)
- **Bit-Banging Method**: ~100% CPU (blocks during transmission)

**During Processing**:
- Colour conversion: ~0.3 ms (128 LEDs)
- Interpolation: ~0.5-2 ms (if enabled)
- Brightness scaling: Negligible (FastLED internal)

### Memory Usage

| Component | Size | Location |
|-----------|------|----------|
| `leds[]` | 384 bytes | SRAM |
| `leds_out[]` | Variable | SRAM (dynamically allocated) |
| FastLED buffers | ~1-2 KB | SRAM (library-managed) |
| **Total** | **~2-3 KB** | **SRAM** |

## Limitations and Constraints

### Protocol Limitations

1. **Reset Time**: 50+ μs required between frames (limits max update rate)
   - **Max Rate**: ~20,000 FPS theoretical (50 μs reset)
   - **Practical Rate**: 100-300+ FPS (limited by processing, not protocol)

2. **Timing Precision**: WS2812B requires precise timing (±150 ns tolerance)
   - FastLED handles this via RMT peripheral
   - Bit-banging may have timing issues at high CPU loads

3. **Cable Length**: Signal degradation over long cables
   - Recommended: <5m without signal regeneration
   - Can use level shifters/repeaters for longer runs

### Hardware Limitations

1. **Power Supply**: LEDs draw significant current at full brightness
   - **Per LED**: ~60 mA at full white (20 mA per colour)
   - **128 LEDs**: ~7.7 A at full white
   - **Power Limiting**: FastLED limits to `MAX_CURRENT_MA` (default: 1500 mA)

2. **Voltage Drop**: Long strips may have voltage drop
   - **Solution**: Power injection at multiple points
   - **FastLED**: Doesn't handle voltage drop (hardware issue)

### Software Limitations

1. **Brightness Resolution**: 8-bit (256 levels) per colour
   - Temporal dithering improves perceived resolution
   - Not true 16-bit colour (but sufficient for most applications)

2. **Colour Accuracy**: HSV→RGB conversion may have slight inaccuracies
   - FastLED's "spectrum" variant improves low-light accuracy
   - Not perceptible in typical use

## Debugging and Diagnostics

### FPS Measurement

```cpp
LED_FPS = FastLED.getFPS();  // Returns actual LED update rate
```

**Usage**: Serial command `led_fps` or `stream=fps`

**Typical Values**: 100-300+ FPS

### Visual Diagnostics

**Boot Animation**: Tests LED functionality on startup
- Fade-in brightness test
- Colour cycle test
- Particle animation

**Blocking Flash**: `blocking_flash(CRGB col)` - Flashes all LEDs in specified colour
- Used for error indication
- Used for unit identification (P2P network)

## Recommendations

1. **Use RMT Method**: Ensure FastLED uses RMT peripheral (default on ESP32-S3)
2. **Power Limiting**: Set `MAX_CURRENT_MA` appropriately for your power supply
3. **Cable Quality**: Use quality cables for long runs (reduce signal degradation)
4. **Power Injection**: Inject power at multiple points for long strips (>100 LEDs)
5. **Temporal Dithering**: Enable for smoother brightness transitions (minimal CPU cost)

