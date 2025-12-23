/**
 * @file ChevronWavesEffect.cpp
 * @brief LGP Chevron Waves implementation
 */

#include "ChevronWavesEffect.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

ChevronWavesEffect::ChevronWavesEffect()
    : m_chevronPos(0.0f)
{
}

bool ChevronWavesEffect::init(plugins::EffectContext& ctx) {
    m_chevronPos = 0.0f;
    return true;
}

void ChevronWavesEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - V-shaped patterns from center

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_chevronPos += speedNorm * 2.0f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, FADE_AMOUNT);

    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
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

void ChevronWavesEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ChevronWavesEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chevron Waves",
        "V-shaped wave propagation from center",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

