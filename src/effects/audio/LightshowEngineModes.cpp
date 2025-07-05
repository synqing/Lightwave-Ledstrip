#include "core/LightshowEngine.h"
#include <FastLED.h>

// External LED buffers declared in main.cpp
extern CRGB strip1[];
extern CRGB strip2[];

// External globals
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;

// -----------------------------------------------------------------------------
// Colour Providers
// -----------------------------------------------------------------------------
static CRGB prismProvider(uint16_t ledIdx, uint16_t ledCount, uint8_t zoneIdx, uint8_t zoneCount) {
    // Rainbow across LEDs, shifted by zone index for subtle colour variety
    uint8_t hue = (static_cast<uint32_t>(ledIdx) * 255 / ledCount + zoneIdx * 4 + gHue) & 0xFF;
    return ColorFromPalette(currentPalette, hue, 255);
}

// -----------------------------------------------------------------------------
// Effect wrappers – can be registered in effects[]
// -----------------------------------------------------------------------------
void spectrumLightshowEngine() {
    static LightshowEngine engine;
    static bool configured = false;
    if (!configured) {
        engine.configure(32, prismProvider, true);  // 32 perceptual zones, log mapping
        configured = true;
    }

    engine.update();

    // Render separately to each strip but using the same data for symmetry
    engine.render(strip1, HardwareConfig::STRIP_LENGTH);
    engine.render(strip2, HardwareConfig::STRIP_LENGTH);
} 