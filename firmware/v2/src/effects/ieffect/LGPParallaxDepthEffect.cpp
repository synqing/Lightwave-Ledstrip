/**
 * @file LGPParallaxDepthEffect.cpp
 * @brief LGP Parallax Depth effect implementation
 */

#include "LGPParallaxDepthEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPParallaxDepthEffect::LGPParallaxDepthEffect()
    : m_time(0.0f)
{
}

bool LGPParallaxDepthEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0.0f;
    return true;
}

void LGPParallaxDepthEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN PARALLAX DEPTH - Two-layer refractive field
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_time += 0.010f + 0.060f * speedNorm;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        float a =
            sinf(dist * 0.060f + m_time)
            + 0.7f * sinf(dist * 0.160f + m_time * 1.3f)
            + 0.4f * sinf(dist * 0.360f - m_time * 1.9f);
        a = 0.5f + 0.5f * tanhf(a / 2.0f);

        const float distB = dist + 0.8f * sinf(m_time * 0.7f);
        float b =
            sinf(distB * 0.058f + m_time * 1.05f + 0.9f)
            + 0.7f * sinf(distB * 0.150f + m_time * 1.35f + 1.7f)
            + 0.4f * sinf(distB * 0.330f - m_time * 2.05f + 2.6f);
        b = 0.5f + 0.5f * tanhf(b / 2.0f);

        const float wave = clamp01(0.5f * (a + b));
        const float base = 0.10f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(dist * 0.4f) + (int)(a * 50.0f));
        const uint8_t hueB = (uint8_t)((int)ctx.gHue + 96 - (int)(dist * 0.4f) + (int)(b * 50.0f));

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}

void LGPParallaxDepthEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPParallaxDepthEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Parallax Depth",
        "Two-layer chromatic parallax",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
