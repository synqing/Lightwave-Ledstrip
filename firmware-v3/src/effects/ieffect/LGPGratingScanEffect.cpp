/**
 * @file LGPGratingScanEffect.cpp
 * @brief LGP Grating Scan effect implementation
 */

#include "LGPGratingScanEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPGratingScanEffect::LGPGratingScanEffect()
    : m_pos(0.0f)
{
}

bool LGPGratingScanEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_pos = 0.0f;
    return true;
}

void LGPGratingScanEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN GRATING SCAN - Spectral scan highlight
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_pos += (0.6f + 2.2f * speedNorm);
    if (m_pos > (float)STRIP_LENGTH) {
        m_pos -= (float)STRIP_LENGTH;
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        const float dx = fabsf(dist - m_pos);
        const float core = expf(-dx * dx * 0.020f);
        const float halo = expf(-dx * dx * 0.006f);

        const float angle = (dist - m_pos) * 0.08f;
        const float spec = 0.5f + 0.5f * tanhf(angle);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(spec * 96.0f));
        const uint8_t hueB = (uint8_t)(ctx.gHue + (int)((1.0f - spec) * 96.0f));

        const float base = 0.06f;
        const float wave = clamp01(0.2f * halo + 0.8f * core);
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}

void LGPGratingScanEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGratingScanEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Grating Scan",
        "Diffraction scan highlight",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
