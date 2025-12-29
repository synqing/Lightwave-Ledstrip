/**
 * @file SinelonEffect.cpp
 * @brief Sinelon effect implementation
 */

#include "SinelonEffect.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

SinelonEffect::SinelonEffect()
{
}

bool SinelonEffect::init(plugins::EffectContext& ctx) {
    return true;
}

void SinelonEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN SINELON - Oscillates outward from center
    using namespace utils;
    fadeToBlackBy(ctx.leds, ctx.ledCount, 20);

    // Oscillate from center outward using utility function
    int distFromCenter = fastled_beatsin16(13, 0, HALF_LENGTH);

    // Set both sides of center
    int pos1 = CENTER_RIGHT + distFromCenter;  // Right side
    int pos2 = CENTER_LEFT - distFromCenter;   // Left side

    CRGB color1 = ctx.palette.getColor(ctx.gHue, 192);
    CRGB color2 = ctx.palette.getColor(ctx.gHue + 128, 192);

    if (pos1 < STRIP_LENGTH) {
        ctx.leds[pos1].r = qadd8(ctx.leds[pos1].r, color1.r);
        ctx.leds[pos1].g = qadd8(ctx.leds[pos1].g, color1.g);
        ctx.leds[pos1].b = qadd8(ctx.leds[pos1].b, color1.b);
        if (pos1 + STRIP_LENGTH < ctx.ledCount) {
            int mirrorPos1 = pos1 + STRIP_LENGTH;
            ctx.leds[mirrorPos1].r = qadd8(ctx.leds[mirrorPos1].r, color1.r);
            ctx.leds[mirrorPos1].g = qadd8(ctx.leds[mirrorPos1].g, color1.g);
            ctx.leds[mirrorPos1].b = qadd8(ctx.leds[mirrorPos1].b, color1.b);
        }
    }
    if (pos2 >= 0) {
        ctx.leds[pos2].r = qadd8(ctx.leds[pos2].r, color2.r);
        ctx.leds[pos2].g = qadd8(ctx.leds[pos2].g, color2.g);
        ctx.leds[pos2].b = qadd8(ctx.leds[pos2].b, color2.b);
        if (pos2 + STRIP_LENGTH < ctx.ledCount) {
            int mirrorPos2 = pos2 + STRIP_LENGTH;
            ctx.leds[mirrorPos2].r = qadd8(ctx.leds[mirrorPos2].r, color2.r);
            ctx.leds[mirrorPos2].g = qadd8(ctx.leds[mirrorPos2].g, color2.g);
            ctx.leds[mirrorPos2].b = qadd8(ctx.leds[mirrorPos2].b, color2.b);
        }
    }
}

void SinelonEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& SinelonEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Sinelon",
        "Bouncing particle with palette trails",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

