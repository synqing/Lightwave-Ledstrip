/**
 * @file LGPPhaseTransitionEffect.cpp
 * @brief LGP Phase Transition effect implementation
 */

#include "LGPPhaseTransitionEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPhaseTransitionEffect::LGPPhaseTransitionEffect()
    : m_phaseAnimation(0.0f)
{
}

bool LGPPhaseTransitionEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phaseAnimation = 0.0f;
    return true;
}

void LGPPhaseTransitionEffect::render(plugins::EffectContext& ctx) {
    // Colors undergo state changes like matter
    float temperature = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    const float pressure = 0.5f;

    m_phaseAnimation += temperature * 0.1f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float localTemp = temperature + (normalizedDist * pressure);

        CRGB color;
        uint8_t paletteOffset = 0;
        uint8_t brightness = 0;

        if (localTemp < 0.25f) {
            // Solid phase
            float crystal = sinf(distFromCenter * 0.3f) * 0.5f + 0.5f;
            paletteOffset = (uint8_t)(crystal * 5.0f);
            brightness = (uint8_t)(255.0f * intensity);
            color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
        } else if (localTemp < 0.5f) {
            // Liquid phase (sin(k*dist - phase) = OUTWARD flow)
            float flow = sinf(distFromCenter * 0.1f - m_phaseAnimation);
            paletteOffset = (uint8_t)(10 + flow * 5.0f);
            brightness = (uint8_t)(200.0f * intensity);
            color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
        } else if (localTemp < 0.75f) {
            // Gas phase
            float dispersion = random8() / 255.0f;
            if (dispersion < 0.3f) {
                paletteOffset = 20;
                brightness = (uint8_t)(150.0f * intensity);
                color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
            } else {
                color = CRGB::Black;
            }
        } else {
            // Plasma phase (sin(k*dist - phase) = OUTWARD, 10x for sharp plasma bands)
            float plasma = sinf(distFromCenter * 0.5f - m_phaseAnimation * 10.0f);
            paletteOffset = (uint8_t)(30 + plasma * 10.0f);
            brightness = (uint8_t)(255.0f * intensity);
            color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
        }

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset + 60), brightness);
        }
    }
}

void LGPPhaseTransitionEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPhaseTransitionEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Phase Transition",
        "State change simulation",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
