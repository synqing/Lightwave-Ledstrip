/**
 * @file LGPSchlierenFlowEffect.cpp
 * @brief LGP Schlieren Flow effect implementation
 */

#include "LGPSchlierenFlowEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPSchlierenFlowEffect::LGPSchlierenFlowEffect()
    : m_t(0.0f)
{
}

bool LGPSchlierenFlowEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_t = 0.0f;
    return true;
}

void LGPSchlierenFlowEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN SCHLIEREN - Knife-edge gradient flow
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_t += 0.012f + 0.070f * speedNorm;

    const float f1 = 0.060f;
    const float f2 = 0.145f;
    const float f3 = 0.310f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = (float)i;
        const float dist = (float)centerPairDistance((uint16_t)i);

        float rho =
            sinf(x * f1 + m_t)
            + 0.7f * sinf(x * f2 - m_t * 1.2f)
            + 0.3f * sinf(x * f3 + m_t * 2.1f);

        float grad =
            f1 * cosf(x * f1 + m_t)
            + 0.7f * f2 * cosf(x * f2 - m_t * 1.2f)
            + 0.3f * f3 * cosf(x * f3 + m_t * 2.1f);

        float edge = 0.5f + 0.5f * tanhf(grad * 6.0f);

        const float mid = (STRIP_LENGTH - 1) * 0.5f;
        const float dmid = (x - mid);
        const float melt = expf(-(dmid * dmid) * 0.0028f);

        float wave = clamp01(0.65f * (edge * melt + 0.25f * melt) + 0.35f * (0.5f + 0.5f * sinf(rho)));

        const float base = 0.08f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t brA = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(dist * 0.7f) + (int)(edge * 40.0f));

        const uint8_t hueB = (uint8_t)(hueA + 5u);
        const uint8_t brB = scale8_video(brA, 245);

        ctx.leds[i] = ctx.palette.getColor(hueA, brA);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}

void LGPSchlierenFlowEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSchlierenFlowEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Schlieren Flow",
        "Knife-edge gradient flow",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
