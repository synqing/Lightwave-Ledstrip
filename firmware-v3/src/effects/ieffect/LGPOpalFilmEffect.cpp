/**
 * @file LGPOpalFilmEffect.cpp
 * @brief LGP Opal Film effect implementation
 */

#include "LGPOpalFilmEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPOpalFilmEffect::LGPOpalFilmEffect()
    : m_time(0.0f)
    , m_flow(0.0f)
{
}

bool LGPOpalFilmEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0.0f;
    m_flow = 0.0f;
    return true;
}

void LGPOpalFilmEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN OPAL FILM - Thin-film inspired iridescence
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_time += 0.010f + speedNorm * 0.040f;
    m_flow += 0.006f + speedNorm * 0.020f;

    const float kR = 1.00f;
    const float kG = 1.23f;
    const float kB = 1.55f;
    const float hueBias = (ctx.gHue / 255.0f) * 0.15f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        float thickness =
            0.55f
            + 0.20f * sinf(dist * 0.045f + m_time)
            + 0.12f * sinf(dist * 0.110f - m_flow * 1.3f)
            + 0.06f * sinf(dist * 0.240f + m_time * 1.7f);

        thickness = clamp01(0.5f + 0.5f * tanhf((thickness - 0.5f) * 2.0f));

        const float phase = 6.2831853f * (thickness + hueBias * 0.15f);

        float r = 0.5f + 0.5f * cosf(phase * kR + 0.3f);
        float g = 0.5f + 0.5f * cosf(phase * kG + 1.1f);
        float b = 0.5f + 0.5f * cosf(phase * kB + 2.0f);

        float luma = 0.20f + 0.80f * (0.333f * (r + g + b));
        r = clamp01(0.65f * r + 0.35f * luma);
        g = clamp01(0.65f * g + 0.35f * luma);
        b = clamp01(0.65f * b + 0.35f * luma);

        const float base = 0.12f;
        const float wave = clamp01(luma);
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        CRGB cA((uint8_t)(255.0f * r), (uint8_t)(255.0f * g), (uint8_t)(255.0f * b));
        CRGB cB((uint8_t)(255.0f * b), (uint8_t)(255.0f * r), (uint8_t)(255.0f * g));

        cA.nscale8_video(br);
        cB.nscale8_video(br);

        ctx.leds[i] = cA;
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = cB;
        }
    }
}

void LGPOpalFilmEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPOpalFilmEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Opal Film",
        "Thin-film iridescence bands",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
