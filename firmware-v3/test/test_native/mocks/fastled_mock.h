// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

/**
 * FastLED Mock for Native Unit Tests
 *
 * Provides minimal FastLED API implementation for testing LED buffer
 * operations without requiring actual WS2812 hardware.
 *
 * Features:
 * - CRGB color type with basic operations
 * - Global FastLED controller for brightness/show tracking
 * - Named color constants
 * - Test instrumentation (show count, brightness state)
 */

#ifdef NATIVE_BUILD

#include <cstdint>
#include <array>
#include <algorithm>

//==============================================================================
// CRGB Color Type
//==============================================================================

struct CHSV; // Forward declaration

struct CRGB {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        uint8_t raw[3];
    };

    // Constructors
    inline CRGB() : r(0), g(0), b(0) {}
    inline CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    inline CRGB(uint32_t colorcode) :
        r((colorcode >> 16) & 0xFF),
        g((colorcode >> 8) & 0xFF),
        b(colorcode & 0xFF) {}

    // Assignment operators
    inline CRGB& operator=(const CRGB& rhs) {
        r = rhs.r;
        g = rhs.g;
        b = rhs.b;
        return *this;
    }

    inline CRGB& operator=(uint32_t colorcode) {
        r = (colorcode >> 16) & 0xFF;
        g = (colorcode >> 8) & 0xFF;
        b = colorcode & 0xFF;
        return *this;
    }

    // Comparison operators
    inline bool operator==(const CRGB& rhs) const {
        return (r == rhs.r) && (g == rhs.g) && (b == rhs.b);
    }

    inline bool operator!=(const CRGB& rhs) const {
        return !(*this == rhs);
    }

    // Arithmetic operators
    inline CRGB& operator+=(const CRGB& rhs) {
        r = static_cast<uint8_t>(std::min(255, r + rhs.r));
        g = static_cast<uint8_t>(std::min(255, g + rhs.g));
        b = static_cast<uint8_t>(std::min(255, b + rhs.b));
        return *this;
    }

    inline CRGB& operator-=(const CRGB& rhs) {
        r = static_cast<uint8_t>(std::max(0, r - rhs.r));
        g = static_cast<uint8_t>(std::max(0, g - rhs.g));
        b = static_cast<uint8_t>(std::max(0, b - rhs.b));
        return *this;
    }

    inline CRGB& operator*=(uint8_t scale) {
        r = (r * scale) / 255;
        g = (g * scale) / 255;
        b = (b * scale) / 255;
        return *this;
    }

    inline CRGB& operator/=(uint8_t scale) {
        if (scale != 0) {
            r = (r * 255) / scale;
            g = (g * 255) / scale;
            b = (b * 255) / scale;
        }
        return *this;
    }

    // Named color constants
    static const CRGB Black;
    static const CRGB White;
    static const CRGB Red;
    static const CRGB Green;
    static const CRGB Blue;
    static const CRGB Yellow;
    static const CRGB Cyan;
    static const CRGB Magenta;
    static const CRGB Orange;
    static const CRGB Purple;

    // Utility methods
    inline uint8_t getLuma() const {
        // Approximation of perceived brightness
        return (r * 54 + g * 183 + b * 19) >> 8;
    }

    inline uint8_t getAverageLight() const {
        return (r + g + b) / 3;
    }

    inline uint8_t getMaxChannel() const {
        return std::max({r, g, b});
    }

    // HSV conversion (simplified)
    CRGB& setHSV(uint8_t hue, uint8_t sat, uint8_t val);
    CRGB(const CHSV& hsv);
};

// CHSV color type (simplified)
struct CHSV {
    uint8_t h;
    uint8_t s;
    uint8_t v;

    inline CHSV() : h(0), s(0), v(0) {}
    inline CHSV(uint8_t hue, uint8_t sat, uint8_t val) : h(hue), s(sat), v(val) {}
};

//==============================================================================
// FastLED Controller
//==============================================================================

class CFastLED {
public:
    CFastLED() : m_brightness(255), m_showCount(0) {}

    // Brightness control
    void setBrightness(uint8_t brightness) { m_brightness = brightness; }
    uint8_t getBrightness() const { return m_brightness; }

    // Show function (updates instrumentation)
    void show() { m_showCount++; }
    void show(uint8_t brightness) {
        m_brightness = brightness;
        m_showCount++;
    }

    // Clear all LEDs (must be called with external LED array)
    void clear(bool writeToStrip = false) {
        if (writeToStrip) {
            m_showCount++;
        }
    }

    // Test instrumentation
    uint32_t getShowCount() const { return m_showCount; }
    void resetShowCount() { m_showCount = 0; }

    // Mock reset (for testing)
    void reset() {
        m_brightness = 255;
        m_showCount = 0;
    }

    // Delay function (uses FreeRTOS mock)
    void delay(uint32_t ms);

private:
    uint8_t m_brightness;
    uint32_t m_showCount;
};

// Global FastLED instance
extern CFastLED FastLED;

//==============================================================================
// CRGBPalette16 - 16-color palette
//==============================================================================

class CRGBPalette16 {
public:
    CRGB entries[16];

    CRGBPalette16() {
        for (int i = 0; i < 16; i++) {
            entries[i] = CRGB::Black;
        }
    }

    CRGB& operator[](int index) {
        return entries[index & 15];
    }

    const CRGB& operator[](int index) const {
        return entries[index & 15];
    }
};

//==============================================================================
// Helper Functions
//==============================================================================

// Fill array with solid color
template<typename T>
void fill_solid(T* leds, int numLeds, const CRGB& color) {
    for (int i = 0; i < numLeds; i++) {
        leds[i] = color;
    }
}

// Fill array with gradient
template<typename T>
void fill_gradient_RGB(T* leds, int numLeds, const CRGB& c1, const CRGB& c2) {
    for (int i = 0; i < numLeds; i++) {
        uint8_t ratio = (i * 255) / (numLeds - 1);
        leds[i].r = c1.r + ((c2.r - c1.r) * ratio) / 255;
        leds[i].g = c1.g + ((c2.g - c1.g) * ratio) / 255;
        leds[i].b = c1.b + ((c2.b - c1.b) * ratio) / 255;
    }
}

// Fade to black
template<typename T>
void fadeToBlackBy(T* leds, int numLeds, uint8_t fadeBy) {
    for (int i = 0; i < numLeds; i++) {
        leds[i].r = (leds[i].r * (255 - fadeBy)) / 255;
        leds[i].g = (leds[i].g * (255 - fadeBy)) / 255;
        leds[i].b = (leds[i].b * (255 - fadeBy)) / 255;
    }
}

// Blur effect (simplified)
template<typename T>
void blur1d(T* leds, int numLeds, uint8_t blur_amount) {
    uint8_t keep = 255 - blur_amount;
    uint8_t seep = blur_amount >> 1;

    CRGB carryover = CRGB::Black;
    for (int i = 0; i < numLeds; i++) {
        CRGB cur = leds[i];
        CRGB part = cur;
        part *= seep;
        cur *= keep;
        cur += carryover;
        if (i > 0) leds[i-1] += part;
        leds[i] = cur;
        carryover = part;
    }
}

// Nscale8 - scale down by 8-bit value
inline void nscale8(CRGB* leds, int numLeds, uint8_t scale) {
    for (int i = 0; i < numLeds; i++) {
        leds[i] *= scale;
    }
}

// Color temperature correction (simplified)
inline CRGB ColorFromPalette(const CRGB* palette, uint8_t index,
                             uint8_t brightness = 255,
                             uint8_t blendType = 0) {
    // Simplified: just return palette color scaled by brightness
    CRGB color = palette[index % 16];  // Assume 16-color palette
    color *= brightness;
    return color;
}

#endif // NATIVE_BUILD
