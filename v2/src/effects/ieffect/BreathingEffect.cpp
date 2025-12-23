/**
 * @file BreathingEffect.cpp
 * @brief Breathing effect implementation
 */

#include "BreathingEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

BreathingEffect::BreathingEffect()
    : m_breathPhase(0.0f)
{
}

bool BreathingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_breathPhase = 0.0f;
    return true;
}

void BreathingEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN BREATHING - Smooth expansion/contraction from center
    float breath = (sinf(m_breathPhase) + 1.0f) / 2.0f;
    float radius = breath * (float)HALF_LENGTH;

    fadeToBlackBy(ctx.leds, ctx.ledCount, 15);

    if (radius > 0.001f) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = (float)centerPairDistance((uint16_t)i);

            if (distFromCenter <= radius) {
                float intensity = 1.0f - (distFromCenter / radius) * 0.5f;
                uint8_t brightness = (uint8_t)(255.0f * intensity * breath);

                CRGB color = ctx.palette.getColor((uint8_t)(ctx.gHue + (uint8_t)distFromCenter), brightness);

                ctx.leds[i] = color;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = color;
                }
            }
        }
    }

    m_breathPhase += ctx.speed / 100.0f;
    if (m_breathPhase > 2.0f * PI) {
        m_breathPhase -= 2.0f * PI;
    }
}

void BreathingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& BreathingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Breathing",
        "Slow rhythmic brightness pulsing",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
