/**
 * @file MirrorModifier.cpp
 * @brief Symmetry modifier implementation
 */

#include "MirrorModifier.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/unit/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace modifiers {

MirrorModifier::MirrorModifier(MirrorMode mode)
    : m_mode(mode)
    , m_enabled(true)
{
}

bool MirrorModifier::init(const plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void MirrorModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled) return;

    // Apply mirroring to each strip independently
    // Strip 1: LEDs 0-159
    mirrorStrip(ctx.leds, 160, ctx.centerPoint);

    // Strip 2: LEDs 160-319
    if (ctx.ledCount >= 320) {
        mirrorStrip(ctx.leds + 160, 160, ctx.centerPoint);
    }
}

void MirrorModifier::unapply() {
    // No cleanup needed
}

const ModifierMetadata& MirrorModifier::getMetadata() const {
    static ModifierMetadata metadata(
        "Mirror",
        "Creates CENTER ORIGIN symmetry by mirroring LED patterns",
        ModifierType::MIRROR,
        1
    );
    return metadata;
}

bool MirrorModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "mode") == 0) {
        setMode(static_cast<MirrorMode>(static_cast<uint8_t>(value)));
        return true;
    }
    return false;
}

float MirrorModifier::getParameter(const char* name) const {
    if (strcmp(name, "mode") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_mode));
    }
    return 0.0f;
}

void MirrorModifier::setMode(MirrorMode mode) {
    m_mode = mode;
}

void MirrorModifier::mirrorStrip(CRGB* strip, uint16_t count, uint16_t center) {
    if (!strip || count == 0) return;

    // Ensure center is within bounds
    if (center >= count) center = count / 2;

    switch (m_mode) {
        case MirrorMode::LEFT_TO_RIGHT: {
            // Mirror left half (0 to center-1) to right half (center to count-1)
            for (uint16_t i = 0; i < center; i++) {
                uint16_t mirrorPos = center + (center - 1 - i);
                if (mirrorPos < count) {
                    strip[mirrorPos] = strip[i];
                }
            }
            break;
        }

        case MirrorMode::RIGHT_TO_LEFT: {
            // Mirror right half (center to count-1) to left half (0 to center-1)
            for (uint16_t i = center; i < count; i++) {
                uint16_t mirrorPos = center - 1 - (i - center);
                if (mirrorPos < center) {
                    strip[mirrorPos] = strip[i];
                }
            }
            break;
        }

        case MirrorMode::CENTER_OUT: {
            // Create identical patterns on both sides
            // Take average of both sides and mirror outward
            for (uint16_t i = 0; i < center; i++) {
                uint16_t rightPos = center + (center - 1 - i);
                if (rightPos < count) {
                    // Blend left and right, then mirror
                    CRGB blended;
                    blended.r = (strip[i].r + strip[rightPos].r) / 2;
                    blended.g = (strip[i].g + strip[rightPos].g) / 2;
                    blended.b = (strip[i].b + strip[rightPos].b) / 2;

                    strip[i] = blended;
                    strip[rightPos] = blended;
                }
            }
            break;
        }

        case MirrorMode::KALEIDOSCOPE: {
            // Alternating quarter patterns
            uint16_t quarter = center / 2;

            // Mirror first quarter to second quarter
            for (uint16_t i = 0; i < quarter; i++) {
                uint16_t mirrorPos = quarter + (quarter - 1 - i);
                if (mirrorPos < center) {
                    strip[mirrorPos] = strip[i];
                }
            }

            // Mirror first half to second half
            for (uint16_t i = 0; i < center; i++) {
                uint16_t mirrorPos = center + (center - 1 - i);
                if (mirrorPos < count) {
                    strip[mirrorPos] = strip[i];
                }
            }
            break;
        }
    }
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
