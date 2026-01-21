/**
 * @file ColorShiftModifier.cpp
 * @brief Palette rotation modifier implementation
 */

#include "ColorShiftModifier.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/unit/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace modifiers {

ColorShiftModifier::ColorShiftModifier(
    ColorShiftMode mode,
    uint8_t hueOffset,
    float rotationSpeed
)
    : m_mode(mode)
    , m_hueOffset(hueOffset)
    , m_rotationSpeed(rotationSpeed)
    , m_accumulatedHue(0.0f)
    , m_enabled(true)
{
}

bool ColorShiftModifier::init(const plugins::EffectContext& ctx) {
    (void)ctx;
    m_accumulatedHue = static_cast<float>(m_hueOffset);
    return true;
}

void ColorShiftModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled) return;

    // Calculate current hue offset
    uint8_t offset = calculateOffset(ctx);

    // Apply hue shift to all LEDs
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        CRGB& led = ctx.leds[i];

        // Convert RGB to HSV
        CHSV hsv = rgb2hsv_approximate(led);

        // Shift hue
        hsv.hue += offset;

        // Convert back to RGB
        ctx.leds[i] = hsv;
    }
}

void ColorShiftModifier::unapply() {
    // No cleanup needed
}

const ModifierMetadata& ColorShiftModifier::getMetadata() const {
    static ModifierMetadata metadata(
        "Color Shift",
        "Rotates hue of all LEDs by fixed or dynamic offset",
        ModifierType::COLOR_SHIFT,
        1
    );
    return metadata;
}

bool ColorShiftModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "mode") == 0) {
        setMode(static_cast<ColorShiftMode>(static_cast<uint8_t>(value)));
        return true;
    }
    if (strcmp(name, "offset") == 0) {
        setHueOffset(static_cast<uint8_t>(value));
        return true;
    }
    if (strcmp(name, "speed") == 0) {
        setRotationSpeed(value);
        return true;
    }
    return false;
}

float ColorShiftModifier::getParameter(const char* name) const {
    if (strcmp(name, "mode") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_mode));
    }
    if (strcmp(name, "offset") == 0) {
        return static_cast<float>(m_hueOffset);
    }
    if (strcmp(name, "speed") == 0) {
        return m_rotationSpeed;
    }
    return 0.0f;
}

void ColorShiftModifier::setMode(ColorShiftMode mode) {
    m_mode = mode;
}

void ColorShiftModifier::setHueOffset(uint8_t offset) {
    m_hueOffset = offset;
    if (m_mode == ColorShiftMode::FIXED) {
        m_accumulatedHue = static_cast<float>(offset);
    }
}

void ColorShiftModifier::setRotationSpeed(float speed) {
    m_rotationSpeed = speed;
}

uint8_t ColorShiftModifier::calculateOffset(const plugins::EffectContext& ctx) {
    switch (m_mode) {
        case ColorShiftMode::FIXED:
            return m_hueOffset;

        case ColorShiftMode::AUTO_ROTATE: {
            // Accumulate hue based on time
            float deltaSec = ctx.getSafeDeltaSeconds();
            m_accumulatedHue += m_rotationSpeed * deltaSec;

            // Wrap to 0-255 range
            while (m_accumulatedHue >= 256.0f) m_accumulatedHue -= 256.0f;
            while (m_accumulatedHue < 0.0f) m_accumulatedHue += 256.0f;

            return static_cast<uint8_t>(m_accumulatedHue);
        }

        case ColorShiftMode::AUDIO_CHROMA: {
#if FEATURE_AUDIO_SYNC
            // Use chromagram-based hue (if available)
            if (ctx.audio.available) {
                // Map dominant chroma bin to hue (0-11 bins -> 0-255 hue)
                // This is a simplified implementation - full version would use
                // actual chromagram data from ControlBus
                float chroma = ctx.audio.treble();  // Placeholder
                return static_cast<uint8_t>(chroma * 255.0f);
            }
#endif
            // Fallback to fixed offset
            return m_hueOffset;
        }

        case ColorShiftMode::BEAT_PULSE: {
#if FEATURE_AUDIO_SYNC
            // Pulse hue on beats
            if (ctx.audio.available) {
                float beatPhase = ctx.audio.beatPhase();

                // Create a pulse envelope (0->1->0 over beat)
                float pulse = 1.0f - fabsf(beatPhase - 0.5f) * 2.0f;

                // Modulate hue offset
                return m_hueOffset + static_cast<uint8_t>(pulse * 64.0f);
            }
#endif
            // Fallback to fixed offset
            return m_hueOffset;
        }

        default:
            return m_hueOffset;
    }
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
