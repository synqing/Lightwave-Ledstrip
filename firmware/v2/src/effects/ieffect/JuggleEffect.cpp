/**
 * @file JuggleEffect.cpp
 * @brief Juggle effect implementation
 */

#include "JuggleEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

JuggleEffect::JuggleEffect()
{
}

bool JuggleEffect::init(plugins::EffectContext& ctx) {
    return true;
}

void JuggleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN JUGGLE - Multiple dots oscillate from center
    //
    // NOTE: dothue += 32 per dot (8 dots × 32 = 256 wrap) creates different colors per dot,
    // not rainbow cycling. Each dot uses a fixed hue per frame, not cycling through the wheel.
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    uint8_t dothue = ctx.gHue;
    for (int i = 0; i < 8; i++) {
        // Oscillate from center outward
        int distFromCenter = beatsin16(i + 7, 0, HALF_LENGTH);

        int pos1 = CENTER_RIGHT + distFromCenter;
        int pos2 = CENTER_LEFT - distFromCenter;

        // Respect active palette and brightness scaling
        uint8_t brightU8 = ctx.brightness;
        CRGB color = ctx.palette.getColor(dothue, brightU8);

        if (pos1 < STRIP_LENGTH) {
            ctx.leds[pos1] = color;
            if (pos1 + STRIP_LENGTH < ctx.ledCount) {
                int mirrorPos1 = pos1 + STRIP_LENGTH;
                ctx.leds[mirrorPos1] = color;
            }
        }
        if (pos2 >= 0) {
            ctx.leds[pos2] = color;
            if (pos2 + STRIP_LENGTH < ctx.ledCount) {
                int mirrorPos2 = pos2 + STRIP_LENGTH;
                ctx.leds[mirrorPos2] = color;
            }
        }

        dothue += 32;
    }
}

void JuggleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& JuggleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Juggle",
        "Multiple colored balls juggling",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
