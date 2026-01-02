# LedFeedback Controller

LED feedback system for K1.8encoderS3 using the M5 8ROTATE unit's 9 RGB LEDs.

## Files

- **LedFeedback.h** - Class declaration and ConnectionStatus enum
- **LedFeedback.cpp** - Implementation with animations and brightness control
- **LedFeedback_Example.cpp** - Usage examples and integration patterns

## LED Mapping

| LED Index | Purpose | Description |
|-----------|---------|-------------|
| 0-7 | Palette Preview | Mirrors current LightwaveOS palette colors |
| 8 | Connection Status | Shows WiFi/WebSocket connection state |

## Connection Status Colors

| Status | Color | RGB | Behavior |
|--------|-------|-----|----------|
| CONNECTING | Blue | (0, 0, 255) | Breathing effect |
| CONNECTED | Green | (0, 255, 0) | Solid |
| DISCONNECTED | Red | (255, 0, 0) | Solid |
| RECONNECTING | Yellow | (255, 255, 0) | Breathing effect |

## Features

### 1. Status Indication
```cpp
leds.setStatus(ConnectionStatus::CONNECTED);
```
- Automatic breathing animation for CONNECTING/RECONNECTING
- Smooth sine-wave breathing (30-100% intensity)
- 20ms update interval for fluid motion

### 2. Palette Preview
```cpp
RGBColor colors[8] = { /* palette colors from LightwaveOS */ };
leds.setPaletteColors(colors);
```
- Updates LEDs 0-7 to match current palette
- Preserves flash animations on active encoders

### 3. Encoder Feedback
```cpp
leds.flashEncoder(encoderIndex);  // 0-7
```
- 150ms brightness boost when encoder turns
- Adds 100 to each RGB component (clamped to 255)
- Visual confirmation of user interaction

### 4. Brightness Control
```cpp
leds.setBrightness(128);  // 0-255
```
- Global brightness scaling
- Affects all LEDs proportionally
- Useful for ambient light adaptation

### 5. Direct LED Control
```cpp
leds.setLed(index, r, g, b);           // Individual control
leds.setLed(index, RGBColor(r, g, b)); // Using color struct
```

### 6. Animations
```cpp
leds.update();  // MUST call in main loop
```
- Handles breathing effect (status LED)
- Manages flash animations (encoder feedback)
- Non-blocking, time-based updates

## Integration Example

```cpp
#include "M5AtomS3.h"
#include "M5ROTATE8.h"
#include "LedFeedback.h"

M5ROTATE8 encoder;
LedFeedback leds(encoder);

void setup() {
    AtomS3.begin();
    Wire.begin();
    encoder.begin();
    leds.begin();
    leds.setStatus(ConnectionStatus::CONNECTING);
}

void loop() {
    leds.update();  // Required for animations

    // Update status based on connection
    if (websocketConnected) {
        leds.setStatus(ConnectionStatus::CONNECTED);
    }

    // Update palette when data arrives
    if (newPaletteData) {
        leds.setPaletteColors(paletteFromLightwaveOS);
    }

    // Flash on encoder change
    for (uint8_t i = 0; i < 8; i++) {
        if (encoder.getRelCounter(i) != 0) {
            leds.flashEncoder(i);
        }
    }
}
```

## Technical Details

### M5ROTATE8 LED Access
- Uses `M5ROTATE8::writeRGB(channel, r, g, b)`
- Channels 0-8 (9 total LEDs)
- Hardware register: 0x70 + (channel × 3)

### Animation Timing
- **Breathing**: 20ms update interval, 256-phase sine wave
- **Flash**: 150ms duration, +100 brightness boost
- **Non-blocking**: Time-based state machine

### Brightness Scaling
```cpp
scaled = (original * brightness) / 255
```
- Applied to all color components
- Brightness 255 = no scaling (optimization)
- Maintains color ratios

### Memory Footprint
- **State**: ~50 bytes (colors, status, animation state)
- **Stack**: Minimal (no dynamic allocation)
- **Code**: ~2KB compiled (with optimizations)

## Dependencies

- M5ROTATE8 library (I2C LED control)
- Arduino.h (millis(), sin(), PI)
- Wire library (I2C communication)

## Notes

1. Always call `leds.update()` in the main loop
2. Flash animations take priority over palette updates
3. Breathing effect only activates for CONNECTING/RECONNECTING
4. Brightness changes apply immediately to all LEDs
5. LED index validation prevents invalid access
6. Compatible with both M5ROTATE8 firmware V1 and V2
