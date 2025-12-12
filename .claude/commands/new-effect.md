---
description: Guide for creating a new CENTER ORIGIN compliant LED effect
---

# New Effect Checklist

I'll help you create a new effect. First, confirm these requirements:

1. **Effect Name**: What should it be called?
2. **Visual Description**: What does it look like?
3. **Propagation**: Outward from center OR inward from edges?
4. **Palette**: Use current palette or specific colors?
5. **Parameters**: Speed-sensitive? Brightness-aware?

## Implementation Steps

1. Create effect function in `src/effects/strip/`
2. Follow CENTER ORIGIN pattern (LED 79/80 is center)
3. Register in `main.cpp` effects[] array (~line 202)
4. Test with serial menu ('e' command)
5. Verify works with zone system

## CENTER ORIGIN Effect Template

```cpp
// Effect: [NAME]
// Description: [DESCRIPTION]
// Propagation: [OUTWARD/INWARD]

void effectName() {
    // CENTER ORIGIN: Effects radiate from LED 79/80
    static uint8_t offset = 0;

    // Use fadeToBlackBy for smooth trails
    fadeToBlackBy(leds, 320, fadeAmount);

    for (int i = 0; i < 160; i++) {
        // Calculate distance from center (79/80)
        int distFromCenter = abs(i - 79);

        // Apply effect based on distance from center
        uint8_t hue = offset + distFromCenter * 2;
        uint8_t brightness = qsub8(255, distFromCenter * 2);

        // Set LED using current palette
        leds[i] = ColorFromPalette(currentPalette, hue, brightness);

        // Mirror to second strip (symmetric)
        leds[319 - i] = leds[i];
    }

    // Animate
    offset += beatsin8(10, 1, 3);  // Speed varies with beat
}
```

## Registration in main.cpp

```cpp
// Add to effects[] array (~line 202)
Effect effects[] = {
    // ... existing effects ...
    {"Effect Name", effectName, EFFECT_TYPE_STANDARD},
};
```

## Global State Available

Effects can access these globals (declared in effects.h):
- `leds[320]` - LED buffer
- `currentPalette` - Active color palette
- `gHue` - Global rotating hue
- `fadeAmount` - Trail fade amount
- `brightnessVal` - Current brightness (0-255)

## Testing

1. Build: `pio run -t upload`
2. Open serial: `pio device monitor -b 115200`
3. Press `e` for effects menu
4. Select your effect by number
5. Verify CENTER ORIGIN compliance visually
