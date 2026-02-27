/**
 * FastLED Mock Implementation
 *
 * Minimal FastLED implementation for native unit tests.
 */

#ifdef NATIVE_BUILD

#include "fastled_mock.h"
#include "freertos_mock.h"
#include <cmath>

//==============================================================================
// Named Color Constants
//==============================================================================

const CRGB CRGB::Black(0, 0, 0);
const CRGB CRGB::White(255, 255, 255);
const CRGB CRGB::Red(255, 0, 0);
const CRGB CRGB::Green(0, 255, 0);
const CRGB CRGB::Blue(0, 0, 255);
const CRGB CRGB::Yellow(255, 255, 0);
const CRGB CRGB::Cyan(0, 255, 255);
const CRGB CRGB::Magenta(255, 0, 255);
const CRGB CRGB::Orange(255, 165, 0);
const CRGB CRGB::Purple(128, 0, 128);

//==============================================================================
// HSV to RGB Conversion
//==============================================================================

CRGB& CRGB::setHSV(uint8_t hue, uint8_t sat, uint8_t val) {
    // Simplified HSV to RGB conversion
    if (sat == 0) {
        // Grayscale
        r = g = b = val;
        return *this;
    }

    uint8_t region = hue / 43;  // 256/6 = 42.67
    uint8_t remainder = (hue - (region * 43)) * 6;

    uint8_t p = (val * (255 - sat)) >> 8;
    uint8_t q = (val * (255 - ((sat * remainder) >> 8))) >> 8;
    uint8_t t = (val * (255 - ((sat * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            r = val; g = t; b = p;
            break;
        case 1:
            r = q; g = val; b = p;
            break;
        case 2:
            r = p; g = val; b = t;
            break;
        case 3:
            r = p; g = q; b = val;
            break;
        case 4:
            r = t; g = p; b = val;
            break;
        default:
            r = val; g = p; b = q;
            break;
    }

    return *this;
}

CRGB::CRGB(const CHSV& hsv) {
    setHSV(hsv.h, hsv.s, hsv.v);
}

//==============================================================================
// FastLED Controller
//==============================================================================

CFastLED FastLED;

void CFastLED::delay(uint32_t ms) {
    ::delay(ms);  // Use FreeRTOS mock delay
}

#endif // NATIVE_BUILD
