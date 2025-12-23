/**
 * @file BPMEffect.cpp
 * @brief BPM effect implementation
 */

#include "BPMEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

BPMEffect::BPMEffect()
{
}

bool BPMEffect::init(plugins::EffectContext& ctx) {
    return true;
}

void BPMEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN BPM - Pulses emanate from center
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Intensity decreases with distance from center
        uint8_t intensity = beat - (uint8_t)(distFromCenter * 3);
        intensity = max(intensity, (uint8_t)32);

        uint8_t colorIndex = ctx.gHue + (uint8_t)(distFromCenter * 2);
        CRGB color = ctx.palette.getColor(colorIndex, intensity);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void BPMEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& BPMEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "BPM",
        "Beat-synced pulsing sawtooth waves",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

