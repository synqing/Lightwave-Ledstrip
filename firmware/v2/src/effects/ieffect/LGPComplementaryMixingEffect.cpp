/**
 * @file LGPComplementaryMixingEffect.cpp
 * @brief LGP Complementary Mixing effect implementation
 */

#include "LGPComplementaryMixingEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPComplementaryMixingEffect::LGPComplementaryMixingEffect()
    : m_phase(0.0f)
{
}

bool LGPComplementaryMixingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPComplementaryMixingEffect::render(plugins::EffectContext& ctx) {
    // Dynamic complementary pairs create neutral zones
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    const float variation = 0.5f;

    m_phase += speed * 0.01f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_phase * 255.0f));
        // Complementary color pairing: Strip 2 uses hue + 128 (180deg offset) for
        // intentional complementary color contrast, not rainbow cycling.
        uint8_t complementHue = (uint8_t)(baseHue + 128);

        uint8_t edgeIntensity = (uint8_t)(255.0f * (1.0f - normalizedDist * variation));

        if (normalizedDist > 0.5f) {
            ctx.leds[i] = CHSV(baseHue, 255, (uint8_t)(edgeIntensity * intensity));
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = CHSV(complementHue, 255, (uint8_t)(edgeIntensity * intensity));
            }
        } else {
            uint8_t saturation = (uint8_t)(255.0f * (normalizedDist * 2.0f));
            ctx.leds[i] = CHSV(baseHue, saturation, (uint8_t)(128.0f * intensity));
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = CHSV(complementHue, saturation, (uint8_t)(128.0f * intensity));
            }
        }
    }
}

void LGPComplementaryMixingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPComplementaryMixingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Complementary Mixing",
        "Complementary color gradients",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
