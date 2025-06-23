# Light Crystals Migration Summary

## Hardware Configuration Changes

### Config.h Updates:
- Changed from 2 strips of 160 LEDs to 1 strip of 81 LEDs
- Updated LED data pin from GPIO 14 to GPIO 6
- Changed button pin from GPIO 41 to GPIO 0
- Added POWER_PIN on GPIO 5
- Removed M5Stack-specific pins (STATUS_LED_PIN, I2C pins)
- Added audio input pins for future use (I2S_SCK, I2S_WS, I2S_DIN)

### Main Code Changes:

1. **Removed M5Stack Dependencies:**
   - Removed `#include <M5Unified.h>`
   - Removed `#include "EncoderControl.h"`
   - Removed M5.begin() and M5.update() calls
   - Added direct GPIO initialization for button and power pin

2. **LED Configuration:**
   - Changed from 2 LED arrays to single array
   - Removed status LED functionality
   - Updated FastLED initialization for single strip

3. **Control Changes:**
   - Removed encoder support (M5Rotate8)
   - Button now cycles through visual modes instead of palettes
   - Removed second strip palette management

4. **Function Updates:**
   - Updated all effect functions to work with single strip
   - Simplified noise arrays from 2D to 1D
   - Updated movement functions to work without strip parameter
   - Removed all references to second strip in display functions

5. **Web Interface:**
   - Removed second_palette_index from JSON status
   - Simplified parameter handling for single strip

## Usage:
The modified code now works with the Light Crystals hardware:
- 81 WS2812 LEDs on GPIO 6
- Button on GPIO 0 to cycle through 3 visual modes
- Power control on GPIO 5
- Web interface for remote control
- Same 33 color palettes available
- Same 3 visual effects (Gradient, Fibonacci waves, Noise smear)