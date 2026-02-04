# LedDriver_P4_RMT - Custom Parallel RMT LED Driver

High-performance parallel RMT driver for ESP32-P4 WS2812 LED strips.

## Features

- **Parallel Transmission**: Both LED strips transmit simultaneously (~5ms vs ~10ms sequential)
- **Double-Buffering**: Wait at frame start, not end (Emotiscope pattern for CPU efficiency)
- **Temporal Dithering**: Perceived 10-12 bit color from 8-bit LEDs
- **No DMA**: Simpler debugging, smaller memory footprint (Emotiscope-style)
- **CRGB Compatible**: Drop-in replacement for existing FastLED-based code

## Performance Targets

| Metric | Value |
|--------|-------|
| Show time | <5ms for 320 LEDs |
| Frame rate | 120+ FPS |
| LED count | 2x160 (320 total) |
| Strips | 2 (parallel) |

## Integration

### Step 1: Copy Files to Firmware

Copy the driver files to your firmware source tree:

```bash
cp -r src/hal/esp32p4/LedDriver_P4_RMT.* \
    ../firmware/v2/src/hal/esp32p4/
```

### Step 2: Update HalFactory.h

Edit `firmware/v2/src/hal/HalFactory.h`:

```cpp
#if CHIP_ESP32_P4
    // Option 1: Use new parallel RMT driver (recommended)
    #define USE_CUSTOM_RMT_DRIVER 1
    
    #if USE_CUSTOM_RMT_DRIVER
        #include "hal/esp32p4/LedDriver_P4_RMT.h"
        inline ILedDriver* createLedDriver() {
            return new LedDriver_P4_RMT();
        }
    #else
        // Fallback to FastLED driver
        #include "hal/esp32p4/LedDriver_P4.h"
        inline ILedDriver* createLedDriver() {
            return new LedDriver_P4();
        }
    #endif
#endif
```

### Step 3: Update CMakeLists.txt

Ensure RMT driver is linked. Edit `firmware/v2/esp-idf-p4/main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS ${LW_SOURCES} "main.cpp"
    INCLUDE_DIRS "." "${LW_SRC_DIR}"
    REQUIRES arduino-compat FastLED driver esp_timer nvs_flash es8311
    # Note: 'driver' component includes RMT support
)
```

### Step 4: Build and Test

```bash
cd LightwaveOS-P4
./build_with_idf55.sh
./flash_and_monitor.sh
```

## Expected Serial Output

On successful initialization:
```
I (xxx) LedDriver_P4_RMT: RMT driver init: 2x160 LEDs on GPIO 20/21 (dual strip, parallel)
```

Periodic timing logs (every ~2 seconds):
```
I (xxx) LedDriver_P4_RMT: Show timing: wait=0us, quantize=450us, transmit_start=50us, total=500us
```

## Configuration

### Disable Dithering

If you experience issues or want simpler output:

```cpp
LedDriver_P4_RMT* driver = static_cast<LedDriver_P4_RMT*>(ledDriver);
driver->setDitheringEnabled(false);
```

### GPIO Pins

Default pins are defined in `chip_esp32p4.h`:
- Strip 1: GPIO 20
- Strip 2: GPIO 21

## Troubleshooting

### LEDs Show Wrong Colors

1. Check GRB vs RGB order (WS2812 uses GRB)
2. Verify timing constants match your LED chipset
3. Try disabling dithering

### Frame Drops / Stuttering

1. Check show() timing in serial output
2. Ensure dual-strip mode is enabled
3. Verify effects aren't taking >3ms per frame

### No LED Output

1. Verify GPIO pins match hardware
2. Check RMT channel initialization logs
3. Ensure power supply is adequate

## Technical Details

### WS2812 Timing (at 10 MHz RMT clock)

| Symbol | Duration | Ticks |
|--------|----------|-------|
| T0H | 0.4µs | 4 |
| T0L | 0.6µs | 6 |
| T1H | 0.7µs | 7 |
| T1L | 0.6µs | 6 |
| Reset | 25µs | 250 |

### Memory Usage

| Buffer | Size |
|--------|------|
| CRGB strip1 | 480 bytes |
| CRGB strip2 | 480 bytes |
| Raw output | 960 bytes |
| Dither error | 3840 bytes |
| **Total** | **5760 bytes** |

## Credits

Based on analysis of [Emotiscope 2.0](https://github.com/lixielabs/emotiscope) by @lixielabs (GPLv3).
