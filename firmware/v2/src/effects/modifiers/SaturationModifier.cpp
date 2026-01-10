/**
 * @file SaturationModifier.cpp
 * @brief Color intensity adjustment modifier implementation
 */

#include "SaturationModifier.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/unit/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace modifiers {

SaturationModifier::SaturationModifier(SatMode mode, int16_t saturation, bool preserveLuminance)
    : m_mode(mode)
    , m_saturation(saturation)
    , m_preserveLuminance(preserveLuminance)
    , m_enabled(true)
{
}

bool SaturationModifier::init(const plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void SaturationModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled) return;

    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        CRGB& led = ctx.leds[i];

        // Convert RGB to HSV
        CHSV hsv = rgb2hsv_approximate(led);
        uint8_t originalValue = hsv.val;  // Store for luminance preservation

        switch (m_mode) {
            case SatMode::ABSOLUTE:
                // Set saturation to absolute value (clamped 0-255)
                hsv.sat = (m_saturation < 0) ? 0 : ((m_saturation > 255) ? 255 : static_cast<uint8_t>(m_saturation));
                break;

            case SatMode::RELATIVE: {
                // Add/subtract from current saturation
                int16_t newSat = hsv.sat + m_saturation;
                hsv.sat = (newSat < 0) ? 0 : ((newSat > 255) ? 255 : static_cast<uint8_t>(newSat));
                break;
            }

            case SatMode::VIBRANCE: {
                // Boost low-saturation colors more than high-saturation
                // This creates a more "intelligent" saturation boost
                float satNorm = hsv.sat / 255.0f;

                // Inverse relationship: low sat gets more boost
                // Boost amount scaled by m_saturation (0-255 = 0-100% boost)
                float boostFactor = (1.0f - satNorm) * (m_saturation / 255.0f);

                // Apply boost (max boost = +128 at sat=0)
                int16_t newSat = hsv.sat + static_cast<int16_t>(boostFactor * 128.0f);
                hsv.sat = (newSat > 255) ? 255 : static_cast<uint8_t>(newSat);
                break;
            }
        }

        // Convert back to RGB
        led = hsv;

        // Optionally restore original luminance
        if (m_preserveLuminance && originalValue > 0) {
            // Scale to maintain original brightness
            CHSV restored = rgb2hsv_approximate(led);
            if (restored.val > 0) {
                // Simple scaling - not perfect but close
                float scale = static_cast<float>(originalValue) / static_cast<float>(restored.val);
                led.r = static_cast<uint8_t>(led.r * scale > 255 ? 255 : led.r * scale);
                led.g = static_cast<uint8_t>(led.g * scale > 255 ? 255 : led.g * scale);
                led.b = static_cast<uint8_t>(led.b * scale > 255 ? 255 : led.b * scale);
            }
        }
    }
}

void SaturationModifier::unapply() {
    // No cleanup needed
}

const ModifierMetadata& SaturationModifier::getMetadata() const {
    static ModifierMetadata metadata(
        "Saturation",
        "Color intensity adjustment with vibrance mode",
        ModifierType::SATURATION,
        1
    );
    return metadata;
}

bool SaturationModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "mode") == 0) {
        setMode(static_cast<SatMode>(static_cast<uint8_t>(value)));
        return true;
    }
    if (strcmp(name, "saturation") == 0) {
        setSaturation(static_cast<int16_t>(value));
        return true;
    }
    if (strcmp(name, "preserveLuminance") == 0) {
        setPreserveLuminance(value > 0.5f);
        return true;
    }
    return false;
}

float SaturationModifier::getParameter(const char* name) const {
    if (strcmp(name, "mode") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_mode));
    }
    if (strcmp(name, "saturation") == 0) {
        return static_cast<float>(m_saturation);
    }
    if (strcmp(name, "preserveLuminance") == 0) {
        return m_preserveLuminance ? 1.0f : 0.0f;
    }
    return 0.0f;
}

void SaturationModifier::setMode(SatMode mode) {
    m_mode = mode;
}

void SaturationModifier::setSaturation(int16_t sat) {
    m_saturation = sat;
}

void SaturationModifier::setPreserveLuminance(bool preserve) {
    m_preserveLuminance = preserve;
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
