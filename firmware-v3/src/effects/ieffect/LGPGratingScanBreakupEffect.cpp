/**
 * @file LGPGratingScanBreakupEffect.cpp
 * @brief LGP Grating Scan (Breakup) effect implementation
 */

#include "LGPGratingScanBreakupEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPGratingScanBreakupEffect::LGPGratingScanBreakupEffect()
    : m_pos(0.0f)
{
}

bool LGPGratingScanBreakupEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_pos = 0.0f;
    return true;
}

void LGPGratingScanBreakupEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN GRATING SCAN (BREAKUP) - Halo spatter decay
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

        // Breakup mask grows with distance from the scan core (halo spatter only).
        const float breakupAmt = clamp01(dx * 0.08f);
        uint32_t h = 2166136261u;
        h ^= (uint32_t)i;
        h *= 16777619u;
        h ^= (uint32_t)(m_pos * 1000.0f);
        h *= 16777619u;
        const float noise = ((h & 1023u) / 1023.0f);
        const float breakup = (noise > breakupAmt) ? 1.0f : 0.0f;

        const float angle = (dist - m_pos) * 0.08f;
        const float spec = 0.5f + 0.5f * tanhf(angle);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(spec * 96.0f));
        const uint8_t hueB = (uint8_t)(ctx.gHue + (int)((1.0f - spec) * 96.0f));

        const float base = 0.06f;
        const float wave = clamp01(0.15f * (halo * breakup) + 0.85f * core);
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}

void LGPGratingScanBreakupEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGratingScanBreakupEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Grating Scan (Breakup)",
        "Diffraction scan with halo breakup",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
