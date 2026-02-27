// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPSpiralVortexEffect.cpp
 * @brief LGP Spiral Vortex effect implementation
 */

#include "LGPSpiralVortexEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPSpiralVortexEffect::LGPSpiralVortexEffect()
    : m_phase(0.0f)
{
}

bool LGPSpiralVortexEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPSpiralVortexEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Creates rotating spiral patterns from center
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * 0.05f;

    const int spiralArms = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Spiral equation
        float spiralAngle = normalizedDist * spiralArms * TWO_PI + m_phase;
        float spiral = sinf(spiralAngle);

        // Radial fade
        spiral *= (1.0f - normalizedDist * 0.5f);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * spiral * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(spiralAngle * 255.0f / TWO_PI);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 128), brightness);
        }
    }
}

void LGPSpiralVortexEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSpiralVortexEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Spiral Vortex",
        "Rotating spiral arms",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
