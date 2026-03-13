/**
 * @file EdgeMixer.h
 * @brief Dual-edge hue splitting for Light Guide Plate colour differentiation
 *
 * The EdgeMixer takes the rendered strip buffers and applies harmonic hue
 * shifts to Strip 2, creating perceived depth through the LGP. Three modes:
 *   - MIRROR: No processing (current behaviour, default)
 *   - ANALOGOUS: Shifts Strip 2 hue by +spread (warm/cool split)
 *   - COMPLEMENTARY: Shifts Strip 2 hue by 180° with saturation reduction
 *
 * Performance: Operates in-place on m_strip2[160]. Target: <0.5ms per frame.
 * Black pixel skip optimisation bypasses HSV conversion for (r|g|b)==0 pixels.
 *
 * Thread safety: Call only from render thread (Core 1).
 */

#pragma once

#include <FastLED.h>
#include <Preferences.h>

namespace lightwaveos {
namespace enhancement {

/**
 * @brief Edge mixer operating mode
 */
enum class EdgeMixerMode : uint8_t {
    MIRROR        = 0,  ///< No processing (default, current behaviour)
    ANALOGOUS     = 1,  ///< Hue shift +spread on Strip 2
    COMPLEMENTARY = 2   ///< 180° hue shift with saturation reduction on Strip 2
};

/**
 * @class EdgeMixer
 * @brief Singleton post-processor for dual-edge LGP colour differentiation
 *
 * Inserted in showLeds() between silence gates and TAP C capture.
 * MIRROR mode returns immediately with zero overhead.
 */
class EdgeMixer {
public:
    static EdgeMixer& getInstance() {
        static EdgeMixer instance;
        return instance;
    }

    /**
     * @brief Apply hue splitting to strip buffer in-place
     *
     * Only strip2 is modified. MIRROR mode returns immediately.
     *
     * @param strip2 Pointer to LEDS_PER_STRIP (160) CRGB pixels
     * @param count Number of pixels
     */
    void process(CRGB* strip2, uint16_t count) {
        if (m_mode == EdgeMixerMode::MIRROR || !strip2 || count == 0) {
            return;
        }

        if (m_mode == EdgeMixerMode::ANALOGOUS) {
            processAnalogous(strip2, count);
        } else {
            processComplementary(strip2, count);
        }
    }

    // === Configuration ===

    void setMode(EdgeMixerMode mode) { m_mode = mode; }
    EdgeMixerMode getMode() const { return m_mode; }

    /**
     * @brief Get human-readable name for current mode
     */
    static const char* modeName(EdgeMixerMode mode) {
        static const char* const names[] = {"mirror", "analogous", "complementary"};
        const uint8_t idx = static_cast<uint8_t>(mode);
        return (idx <= 2) ? names[idx] : "unknown";
    }
    const char* modeName() const { return modeName(m_mode); }

    /**
     * @brief Set analogous hue spread in degrees (0-60)
     *
     * Converted internally to FastLED 0-255 hue units.
     * Default: 30° ≈ 21 in FastLED units.
     */
    void setSpread(uint8_t spreadDegrees) {
        if (spreadDegrees > 60) spreadDegrees = 60;
        m_spreadDegrees = spreadDegrees;
        // Convert degrees to FastLED hue units: degrees * 256 / 360
        // Use integer maths to avoid float in config path
        m_spreadHue = static_cast<uint8_t>((static_cast<uint16_t>(spreadDegrees) * 256 + 180) / 360);
    }
    uint8_t getSpread() const { return m_spreadDegrees; }

    /**
     * @brief Set mix strength (0-255)
     *
     * 0 = no effect (identical to MIRROR), 255 = full hue shift.
     * Intermediate values blend between original and shifted colour.
     */
    void setStrength(uint8_t strength) { m_strength = strength; }
    uint8_t getStrength() const { return m_strength; }

    // === NVS Persistence ===

    void saveToNVS() {
        Preferences prefs;
        if (prefs.begin("edgemixer", false)) {
            prefs.putUChar("mode", static_cast<uint8_t>(m_mode));
            prefs.putUChar("spread", m_spreadDegrees);
            prefs.putUChar("strength", m_strength);
            prefs.end();
        }
    }

    void loadFromNVS() {
        Preferences prefs;
        if (prefs.begin("edgemixer", true)) {
            uint8_t mode = prefs.getUChar("mode", 0);
            if (mode <= 2) {
                m_mode = static_cast<EdgeMixerMode>(mode);
            }
            setSpread(prefs.getUChar("spread", 30));
            m_strength = prefs.getUChar("strength", 255);
            prefs.end();
        }
    }

private:
    EdgeMixer()
        : m_mode(EdgeMixerMode::MIRROR)
        , m_spreadDegrees(30)
        , m_spreadHue(21)   // 30° in FastLED units: (30*256+180)/360 = 21
        , m_strength(255)
    {}

    EdgeMixer(const EdgeMixer&) = delete;
    EdgeMixer& operator=(const EdgeMixer&) = delete;

    /**
     * @brief Analogous mode: shift hue by +spread
     *
     * Strip 1 stays at original hue, Strip 2 shifts by the full spread value.
     * The perceptual separation through the LGP equals the spread setting.
     */
    void processAnalogous(CRGB* strip2, uint16_t count) {
        const uint8_t hueShift = m_spreadHue;  // Full spread on Strip 2
        if (hueShift == 0 || m_strength == 0) return;

        for (uint16_t i = 0; i < count; ++i) {
            // Black pixel skip — critical optimisation
            if ((strip2[i].r | strip2[i].g | strip2[i].b) == 0) continue;

            CHSV hsv = rgb2hsv_approximate(strip2[i]);
            hsv.hue += hueShift;  // uint8_t wrapping handles modular arithmetic

            CRGB shifted;
            hsv2rgb_rainbow(hsv, shifted);

            // Strength blending: interpolate original → shifted
            if (m_strength < 255) {
                strip2[i] = blend(strip2[i], shifted, m_strength);
            } else {
                strip2[i] = shifted;
            }
        }
    }

    /**
     * @brief Complementary mode: 180° shift + saturation reduction
     */
    void processComplementary(CRGB* strip2, uint16_t count) {
        if (m_strength == 0) return;
        for (uint16_t i = 0; i < count; ++i) {
            // Black pixel skip
            if ((strip2[i].r | strip2[i].g | strip2[i].b) == 0) continue;

            CHSV hsv = rgb2hsv_approximate(strip2[i]);
            hsv.hue += 128;  // 180° in FastLED units
            // Reduce saturation by ~15%: scale8(sat, 217) ≈ sat * 0.85
            hsv.sat = scale8(hsv.sat, 217);

            CRGB shifted;
            hsv2rgb_rainbow(hsv, shifted);

            if (m_strength < 255) {
                strip2[i] = blend(strip2[i], shifted, m_strength);
            } else {
                strip2[i] = shifted;
            }
        }
    }

    EdgeMixerMode m_mode;
    uint8_t m_spreadDegrees;  ///< User-facing spread in degrees (0-60)
    uint8_t m_spreadHue;      ///< Spread converted to FastLED hue units (0-42)
    uint8_t m_strength;       ///< Mix strength (0-255)
};

} // namespace enhancement
} // namespace lightwaveos
