/**
 * @file ConfettiEffect.cpp
 * @brief Confetti effect implementation
 */

#include "ConfettiEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

ConfettiEffect::ConfettiEffect()
{
}

bool ConfettiEffect::init(plugins::EffectContext& ctx) {
    // Clear buffer for stateful effect
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    return true;
}

void ConfettiEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN CONFETTI - Sparks spawn at center 79/80 and fade outward
    //
    // BUFFER-FEEDBACK EFFECT: This effect reads from ctx.leds[i+1] and ctx.leds[i-1]
    // to propagate confetti particles outward. This relies on the previous frame's
    // LED buffer state, making it a stateful effect. Identified in PatternRegistry::isStatefulEffect().

    // Fade all LEDs
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Spawn new confetti at CENTER PAIR
    if (random8() < 80) {
        int centerPos = CENTER_LEFT + random8(2);  // 79 or 80
        CRGB color = ctx.palette.getColor(ctx.gHue + random8(64), 255);

        ctx.leds[centerPos] = color;
        if (centerPos + STRIP_LENGTH < ctx.ledCount) {
            int mirrorPos = centerPos + STRIP_LENGTH;
            ctx.leds[mirrorPos] = color;
        }
    }

    // Propagate confetti outward (buffer feedback)
    // Left side propagation
    for (int i = CENTER_LEFT - 1; i >= 0; i--) {
        if (ctx.leds[i + 1]) {
            ctx.leds[i] = ctx.leds[i + 1];
            ctx.leds[i].fadeToBlackBy(ctx.fadeAmount);

            // Mirror to strip 2
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.leds[i];
            }
        }
    }

    // Right side propagation
    for (int i = CENTER_RIGHT + 1; i < STRIP_LENGTH; i++) {
        if (ctx.leds[i - 1]) {
            ctx.leds[i] = ctx.leds[i - 1];
            ctx.leds[i].fadeToBlackBy(ctx.fadeAmount);

            // Mirror to strip 2
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.leds[i];
            }
        }
    }
}

void ConfettiEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ConfettiEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Confetti",
        "Random colored speckles fading",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

