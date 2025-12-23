/**
 * @file LGPWaveCollisionEffect.cpp
 * @brief LGP Wave Collision effect implementation
 */

#include "LGPWaveCollisionEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPWaveCollisionEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void LGPWaveCollisionEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN WAVE COLLISION - Wave packets expand outward from center and collide
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // Wave packets expand outward from center (LEDs 79/80)
    float packet1Radius = fmodf(ctx.frameNumber * speedNorm * 0.5f, (float)HALF_LENGTH);
    float packet2Radius = fmodf(ctx.frameNumber * speedNorm * 0.5f, (float)HALF_LENGTH);

    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // CENTER ORIGIN: Calculate distance from center pair
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Wave packet 1 (expanding rightward from center)
        float dist1 = fabsf(distFromCenter - packet1Radius);
        float packet1 = expf(-dist1 * 0.05f) * cosf(dist1 * 0.5f);

        // Wave packet 2 (expanding leftward from center, with phase offset)
        float dist2 = fabsf(distFromCenter - packet2Radius);
        float packet2 = expf(-dist2 * 0.05f) * cosf(dist2 * 0.5f + PI);

        // Interference
        float interference = packet1 + packet2;

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * interference * intensityNorm);

        // CENTER ORIGIN color mapping
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2.0f + interference * 50.0f);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 128), brightness);
        }
    }
}

void LGPWaveCollisionEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPWaveCollisionEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Wave Collision",
        "Colliding wave fronts creating standing nodes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
