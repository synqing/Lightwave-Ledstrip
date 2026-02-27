/**
 * @file LGPMoireSilkEffect.cpp
 * @brief LGP Moire Silk effect implementation
 */

#include "LGPMoireSilkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPMoireSilkEffect::LGPMoireSilkEffect()
    : m_phaseA(0.0f)
    , m_phaseB(0.0f)
{
}

bool LGPMoireSilkEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phaseA = 0.0f;
    m_phaseB = 0.0f;
    return true;
}

void LGPMoireSilkEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN MOIRE SILK - Two-lattice beat envelope
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_phaseA += 0.012f + 0.050f * speedNorm;
    m_phaseB += 0.010f + 0.041f * speedNorm;

    const float f1 = 0.180f;
    const float f2 = 0.198f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        const float g1 = sinf(dist * f1 + m_phaseA);
        const float g2 = sinf(dist * f2 + m_phaseB);

        float field = g1 * g2;
        field = 0.5f + 0.5f * tanhf(field * 2.2f);

        const float rib = 0.5f + 0.5f * sinf(dist * 0.70f - m_phaseA * 1.7f);
        field = clamp01(0.78f * field + 0.22f * rib);

        const float base = 0.10f;
        const float out = clamp01(base + (1.0f - base) * field) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(field * 80.0f));
        const uint8_t hueB = (uint8_t)(ctx.gHue + (int)((1.0f - field) * 80.0f));

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}

void LGPMoireSilkEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPMoireSilkEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Moire Silk",
        "Two-lattice moire beat pattern",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
