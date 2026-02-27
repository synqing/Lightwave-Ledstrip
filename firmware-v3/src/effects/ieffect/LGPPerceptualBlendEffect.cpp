/**
 * @file LGPPerceptualBlendEffect.cpp
 * @brief LGP Perceptual Blend effect implementation
 */

#include "LGPPerceptualBlendEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerceptualBlendEffect::LGPPerceptualBlendEffect()
    : m_phase(0.0f)
{
}

bool LGPPerceptualBlendEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPPerceptualBlendEffect::render(plugins::EffectContext& ctx) {
    // Uses perceptually uniform color space for natural mixing
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    m_phase += speed * 0.01f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float L = 50.0f + 50.0f * sinf(m_phase);
        float a = 50.0f * cosf(m_phase + normalizedDist * PI);
        float b = 50.0f * sinf(m_phase - normalizedDist * PI);

        CRGB color;
        color.r = (uint8_t)(constrain(L + a * 2.0f, 0.0f, 255.0f) * intensity);
        color.g = (uint8_t)(constrain(L - a - b, 0.0f, 255.0f) * intensity);
        color.b = (uint8_t)(constrain(L + b * 2.0f, 0.0f, 255.0f) * intensity);

        ctx.leds[i] = color;

        if (i + STRIP_LENGTH < ctx.ledCount) {
            CRGB color2;
            color2.r = (uint8_t)(constrain(L - a * 2.0f, 0.0f, 255.0f) * intensity);
            color2.g = (uint8_t)(constrain(L + a + b, 0.0f, 255.0f) * intensity);
            color2.b = (uint8_t)(constrain(L - b * 2.0f, 0.0f, 255.0f) * intensity);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerceptualBlendEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerceptualBlendEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perceptual Blend",
        "Lab color space mixing",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
