/**
 * @file LGPStressGlassMeltEffect.cpp
 * @brief LGP Stress Glass (Melt) effect implementation
 */

#include "LGPStressGlassMeltEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPStressGlassMeltEffect::LGPStressGlassMeltEffect()
    : m_analyser(0.0f)
{
}

bool LGPStressGlassMeltEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_analyser = 0.0f;
    return true;
}

void LGPStressGlassMeltEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN STRESS GLASS (MELT) - Phase-locked wings
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
            // Lock wings together near the melt zone: subtle shift at edges only.
            const float shiftF = (1.0f - stress);
            const uint8_t hueShift = (uint8_t)(shiftF * 10.0f);
            const uint8_t hueB = (uint8_t)(hueA + hueShift);

            // Slight dim on B to avoid perceived dominance ping-pong.
            const uint8_t brB = scale8_video(br, 245);
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}

void LGPStressGlassMeltEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStressGlassMeltEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Stress Glass (Melt)",
        "Photoelastic fringes with phase-locked wings",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
