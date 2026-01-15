/**
 * @file LGPEvanescentSkinEffect.cpp
 * @brief LGP Evanescent Skin effect implementation
 */

#include "LGPEvanescentSkinEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPEvanescentSkinEffect::LGPEvanescentSkinEffect()
    : m_time(0)
{
}

bool LGPEvanescentSkinEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPEvanescentSkinEffect::render(plugins::EffectContext& ctx) {
    // Thin shimmering layers hugging rims or edges
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    float intensityNorm = ctx.brightness / 255.0f;
    (void)intensityNorm;
    const bool rimMode = true;
    const float lambda = 4.0f;
    const float skinFreq = 7.0f;
    float anim = m_time / 256.0f;

    float ringRadius = HALF_LENGTH * 0.6f;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float brightnessF;
        float hue = (float)ctx.gHue + (i >> 1);

        if (rimMode) {
            float distFromCenter = (float)centerPairDistance(i);
            float skinDistance = fabsf(distFromCenter - ringRadius);
            float envelope = 1.0f / (1.0f + lambda * skinDistance);
            float carrier = sinf(distFromCenter * skinFreq * 0.05f + anim * TWO_PI);
            brightnessF = envelope * (carrier * 0.5f + 0.5f) * 255.0f;
        } else {
            uint16_t edgeDistance = min((uint16_t)i, (uint16_t)(STRIP_LENGTH - 1 - i));
            float distToEdge = (float)edgeDistance;
            float envelope = 1.0f / (1.0f + lambda * distToEdge * 0.4f);
            float carrier = sinf(((float)STRIP_LENGTH - distToEdge) * skinFreq * 0.04f - anim * TWO_PI);
            brightnessF = envelope * (carrier * 0.5f + 0.5f) * 255.0f;
        }

        brightnessF = constrain(brightnessF, 0.0f, 255.0f);
        uint8_t brightness = (uint8_t)brightnessF;

        // Use palette system - apply brightness scaling
        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor((uint8_t)hue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 128), brightU8);
        }
    }
}

void LGPEvanescentSkinEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPEvanescentSkinEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Evanescent Skin",
        "Surface wave propagation",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
