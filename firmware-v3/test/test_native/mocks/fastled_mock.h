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
#include <cstdlib>
#include <array>
#include <algorithm>

// PROGMEM is an Arduino macro for placing constants in flash; the native
// host build has no such concept — define it as a no-op so any PROGMEM
// declarations in production headers compile cleanly under NATIVE_BUILD.
#ifndef PROGMEM
#define PROGMEM
#endif

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
    constexpr CRGB() : r(0), g(0), b(0) {}
    constexpr CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
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

    // FastLED CRGB::nscale8 — non-destructive in-place per-channel scale.
    inline CRGB& nscale8(uint8_t scale) {
        r = (r * scale) / 255;
        g = (g * scale) / 255;
        b = (b * scale) / 255;
        return *this;
    }

    inline CRGB& nscale8_video(uint8_t scale) {
        r = (r == 0 || scale == 0) ? 0 : static_cast<uint8_t>(((r * scale) / 255) + 1);
        g = (g == 0 || scale == 0) ? 0 : static_cast<uint8_t>(((g * scale) / 255) + 1);
        b = (b == 0 || scale == 0) ? 0 : static_cast<uint8_t>(((b * scale) / 255) + 1);
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
//
// FastLED's real CHSV exposes BOTH .h/.s/.v and .hue/.sat/.val via a union.
// The mock mirrors that so production code reading either name compiles.
struct CHSV {
    union {
        struct {
            union { uint8_t h; uint8_t hue; };
            union { uint8_t s; uint8_t sat; uint8_t saturation; };
            union { uint8_t v; uint8_t val; uint8_t value; };
        };
        uint8_t raw[3];
    };

    inline CHSV() : h(0), s(0), v(0) {}
    inline CHSV(uint8_t hue_, uint8_t sat_, uint8_t val_) : h(hue_), s(sat_), v(val_) {}
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
extern const CRGB TypicalLEDStrip;

//==============================================================================
// CRGBPalette16 - 16-color palette
//==============================================================================

// FastLED PROGMEM-resident gradient palette typedef (ESP-IDF builds keep
// this in flash; native test build treats it as a plain byte pointer).
typedef uint8_t TProgmemRGBGradientPalette_byte;
typedef const TProgmemRGBGradientPalette_byte* TProgmemRGBGradientPaletteRef;

class CRGBPalette16 {
public:
    CRGB entries[16];

    CRGBPalette16() {
        for (int i = 0; i < 16; i++) {
            entries[i] = CRGB::Black;
        }
    }

    // Construct from a PROGMEM gradient palette table. The native build
    // doesn't decode the gradient — tests only need a populated array, so
    // we deterministically derive 16 entries from the byte stream.
    CRGBPalette16(TProgmemRGBGradientPaletteRef table) {
        if (table == nullptr) {
            for (int i = 0; i < 16; i++) entries[i] = CRGB::Black;
            return;
        }
        // Copy first 48 bytes as 16 RGB triples.
        for (int i = 0; i < 16; i++) {
            entries[i] = CRGB(table[i * 3 + 0], table[i * 3 + 1], table[i * 3 + 2]);
        }
    }

    CRGB& operator[](int index) {
        return entries[index & 15];
    }

    const CRGB& operator[](int index) const {
        return entries[index & 15];
    }
};

enum TBlendType : uint8_t {
    NOBLEND = 0,
    LINEARBLEND = 1
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

inline void nscale8_video(CRGB* leds, int numLeds, uint8_t scale) {
    for (int i = 0; i < numLeds; i++) {
        leds[i].nscale8_video(scale);
    }
}

// Color temperature correction (simplified)
inline CRGB ColorFromPalette(const CRGB* palette, uint8_t index,
                             uint8_t brightness = 255,
                             uint8_t blendType = 0) {
    (void)blendType;
    // Simplified: just return palette color scaled by brightness
    CRGB color = palette[index % 16];  // Assume 16-color palette
    color *= brightness;
    return color;
}

inline CRGB ColorFromPalette(const CRGBPalette16& palette, uint8_t index,
                             uint8_t brightness = 255,
                             TBlendType blendType = LINEARBLEND) {
    return ColorFromPalette(palette.entries, index, brightness, static_cast<uint8_t>(blendType));
}

//==============================================================================
// Arduino-style random() — int-arg overloads
//
// The host C library exposes random(void); we deliberately introduce overloads
// that take an int/long argument so production code calling random(N) and
// random(min, max) resolves to these here, not to the 0-arg POSIX symbol.
//==============================================================================

inline long random(long max) {
    if (max <= 0) return 0;
    return static_cast<long>(std::rand()) % max;
}

inline long random(long min, long max) {
    if (max <= min) return min;
    return min + static_cast<long>(std::rand()) % (max - min);
}

//==============================================================================
// Perlin-noise placeholder — deterministic uint8_t output
//
// Real FastLED inoise8() returns smoothed Perlin noise in [0,255]. The mock
// only needs a defined uint8 value derived deterministically from the input
// coordinates so unit tests have stable behaviour.
//==============================================================================

inline uint8_t inoise8(uint16_t x) {
    return static_cast<uint8_t>((x * 73u) ^ (x >> 3));
}

inline uint8_t inoise8(uint16_t x, uint16_t y) {
    return static_cast<uint8_t>((x * 73u + y * 31u) ^ ((x >> 3) ^ (y >> 5)));
}

inline uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z) {
    return static_cast<uint8_t>((x * 73u + y * 31u + z * 17u)
                                ^ ((x >> 3) ^ (y >> 5) ^ (z >> 7)));
}

//==============================================================================
// rgb2hsv_approximate — RGB → HSV conversion
//
// FastLED ships a fast approximation; the mock uses the standard float-based
// algorithm. Tests only need correct hue ordering + saturation/value bytes,
// not bitwise parity with FastLED's lookup tables.
//==============================================================================

inline CHSV rgb2hsv_approximate(const CRGB& rgb) {
    const uint8_t r = rgb.r;
    const uint8_t g = rgb.g;
    const uint8_t b = rgb.b;

    const uint8_t maxC = std::max({r, g, b});
    const uint8_t minC = std::min({r, g, b});
    const uint8_t delta = static_cast<uint8_t>(maxC - minC);

    CHSV out;
    out.v = maxC;

    if (maxC == 0 || delta == 0) {
        out.h = 0;
        out.s = 0;
        return out;
    }

    out.s = static_cast<uint8_t>((static_cast<uint16_t>(delta) * 255u) / maxC);

    // Hue in [0, 255] (FastLED-style 8-bit wheel, not 360°).
    float hueF;
    if (maxC == r) {
        hueF = static_cast<float>(static_cast<int>(g) - static_cast<int>(b)) /
               static_cast<float>(delta);
    } else if (maxC == g) {
        hueF = 2.0f + static_cast<float>(static_cast<int>(b) - static_cast<int>(r)) /
                       static_cast<float>(delta);
    } else {
        hueF = 4.0f + static_cast<float>(static_cast<int>(r) - static_cast<int>(g)) /
                       static_cast<float>(delta);
    }
    hueF *= (256.0f / 6.0f);
    if (hueF < 0.0f) hueF += 256.0f;
    if (hueF >= 256.0f) hueF -= 256.0f;
    out.h = static_cast<uint8_t>(hueF);
    return out;
}

inline void hsv2rgb_spectrum(const CHSV& hsv, CRGB& rgb) {
    rgb.setHSV(hsv.h, hsv.s, hsv.v);
}

#endif // NATIVE_BUILD
