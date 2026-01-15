/**
 * @file LGPStarBurstAmbientEffect.cpp
 * @brief LGP Star Burst (Ambient) - Time-driven radial star patterns
 * 
 * Restored from commit c691ea3 - original clean implementation.
 */

#include "LGPStarBurstAmbientEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPStarBurstAmbientEffect::LGPStarBurstAmbientEffect()
    : m_phase(0.0f)
{
}

bool LGPStarBurstAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPStarBurstAmbientEffect::render(plugins::EffectContext& ctx) {
    using namespace lightwaveos::effects;
    
    // CENTER ORIGIN - Star-like patterns radiating from center
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * 0.03f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, 20);

    for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; i++) {
        float distFromCenter = (float)centerPairDistance(i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Star equation - radially symmetric from center
        float star = sinf(distFromCenter * 0.3f + m_phase) * expf(-normalizedDist * 2.0f);

        // Pulsing
        star *= 0.5f + 0.5f * sinf(m_phase * 3.0f);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * star * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(distFromCenter + star * 50.0f);

        ctx.leds[i] += ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] += ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 85), brightness);
        }
    }
}

void LGPStarBurstAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStarBurstAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Star Burst (Ambient)",
        "Time-driven explosive radial lines with pulsing",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
