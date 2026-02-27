// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPHolographicVortexEffect.cpp
 * @brief LGP Holographic Vortex effect implementation
 */

#include "LGPHolographicVortexEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPHolographicVortexEffect::LGPHolographicVortexEffect()
    : m_time(0)
{
}

bool LGPHolographicVortexEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPHolographicVortexEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Spiral interference pattern with depth illusion
    m_time = (uint16_t)(m_time + (ctx.speed << 1));

    const uint8_t spiralCount = 3;
    uint8_t tightness = ctx.brightness >> 2;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i);
        float r = distFromCenter / (float)HALF_LENGTH;

        // Symmetric azimuthal angle
        uint16_t theta = (uint16_t)(distFromCenter * 410.0f);

        // Spiral phase
        uint16_t phase = (uint16_t)(spiralCount * theta + (uint16_t)(tightness * r * 65535.0f) - m_time);

        uint8_t brightness = sin8(phase >> 8);
        uint8_t paletteIndex = (uint8_t)(phase >> 10);

        // Add depth via brightness modulation
        brightness = scale8(brightness, (uint8_t)(255 - (uint8_t)(r * 127.0f)));
        brightness = scale8(brightness, ctx.brightness);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 128), brightness);
        }
    }
}

void LGPHolographicVortexEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPHolographicVortexEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Holographic Vortex",
        "Deep 3D vortex illusion",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
