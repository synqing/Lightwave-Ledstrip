#pragma once

#ifdef NATIVE_BUILD
#include "fastled_mock.h"

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

inline uint8_t qadd8(uint8_t a, uint8_t b) {
    uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(b);
    return (sum > 255) ? 255 : static_cast<uint8_t>(sum);
}

inline uint8_t scale8(uint8_t value, uint8_t scale) {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale) / 255);
}

using TProgmemRGBGradientPalette_byte = uint8_t;
using TProgmemRGBGradientPaletteRef = const TProgmemRGBGradientPalette_byte*;
#else
#include <FastLED.h>
#endif
