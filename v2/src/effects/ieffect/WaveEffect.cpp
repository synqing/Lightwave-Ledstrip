/**
 * @file WaveEffect.cpp
 * @brief Wave effect implementation
 */

#include "WaveEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

WaveEffect::WaveEffect()
    : m_waveOffset(0)
{
}

bool WaveEffect::init(plugins::EffectContext& ctx) {
    m_waveOffset = 0;
    return true;
}

void WaveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN WAVE - Waves propagate from center
    m_waveOffset += ctx.speed;
    if (m_waveOffset > 65535) m_waveOffset = m_waveOffset % 65536;

    // Gentle fade
    fadeToBlackBy(ctx.leds, ctx.ledCount, 20);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Wave propagates outward from center
        uint8_t brightness = sin8((uint16_t)(distFromCenter * 15) + (m_waveOffset >> 4));
        uint8_t colorIndex = (uint8_t)(distFromCenter * 8) + (m_waveOffset >> 6);

        CRGB color = ctx.palette.getColor(colorIndex, brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void WaveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& WaveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Wave",
        "Simple sine wave propagation",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

