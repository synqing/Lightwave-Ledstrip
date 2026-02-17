/**
 * @file LGPStressGlassEffect.cpp
 * @brief LGP Stress Glass effect implementation
 */

#include "LGPStressGlassEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPStressGlassEffect::LGPStressGlassEffect()
    : m_analyser(0.0f)
{
}

bool LGPStressGlassEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_analyser = 0.0f;
    return true;
}

void LGPStressGlassEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN STRESS GLASS - Photoelastic fringe field
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_analyser += 0.010f + 0.060f * speedNorm;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        float stress =
            expf(-dist * dist * 0.020f)
            + 0.65f * expf(-(dist - 6.0f) * (dist - 6.0f) * 0.030f)
            + 0.65f * expf(-(dist - 12.0f) * (dist - 12.0f) * 0.030f);

        stress = clamp01(stress);

        const float retard = (8.0f * stress) + m_analyser;
        const float fringe = sinf(retard);
        const float wave = clamp01(fringe * fringe);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(stress * 120.0f) + (int)(m_analyser * 12.0f));

        const float base = 0.08f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        ctx.leds[i] = ctx.palette.getColor(hueA, br);

        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            const uint8_t hueB = (uint8_t)(hueA + 24u);
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}

void LGPStressGlassEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStressGlassEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Stress Glass",
        "Photoelastic birefringence fringes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
