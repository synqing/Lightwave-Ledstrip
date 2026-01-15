/**
 * @file ChevronWavesAmbientEffect.cpp
 * @brief Chevron Waves (Ambient) - Time-driven V-shaped wave propagation
 * 
 * Restored from commit c691ea3 - original clean implementation.
 */

#include "ChevronWavesAmbientEffect.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

ChevronWavesAmbientEffect::ChevronWavesAmbientEffect()
    : m_chevronPos(0.0f)
{
}

bool ChevronWavesAmbientEffect::init(plugins::EffectContext& ctx) {
    m_chevronPos = 0.0f;
    return true;
}

void ChevronWavesAmbientEffect::render(plugins::EffectContext& ctx) {
    using namespace lightwaveos::effects;
    
    // CENTER ORIGIN - V-shaped patterns from center

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_chevronPos += speedNorm * 2.0f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, FADE_AMOUNT);

    for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; i++) {
        float distFromCenter = ctx.getDistanceFromCenter(i) * (float)HALF_LENGTH;

        // Create V-shape from center
        float chevronPhase = distFromCenter * CHEVRON_ANGLE + m_chevronPos;
        float chevron = sinf(chevronPhase * CHEVRON_COUNT * 0.1f);

        // Sharp edges
        chevron = tanhf(chevron * 3.0f) * 0.5f + 0.5f;

        uint8_t brightness = (uint8_t)(chevron * 255.0f * intensityNorm);
        uint8_t hue = ctx.gHue + (uint8_t)(distFromCenter * 2.0f) + (uint8_t)(m_chevronPos * 0.5f);

        CRGB color = ctx.palette.getColor(hue, brightness);
        ctx.leds[i] += color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] += ctx.palette.getColor(hue + 90, brightness);
        }
    }
}

void ChevronWavesAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ChevronWavesAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Chevron Waves (Ambient)",
        "Time-driven V-shaped wave propagation from center",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
