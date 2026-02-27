// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPRadialRippleEffect.cpp
 * @brief LGP Radial Ripple effect implementation
 */

#include "LGPRadialRippleEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPRadialRippleEffect::LGPRadialRippleEffect()
    : m_time(0)
{
}

bool LGPRadialRippleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPRadialRippleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Concentric rings that expand from center
    const uint8_t ringCount = 6;
    uint16_t ringSpeed = (uint16_t)(ctx.speed << 2);

    m_time = (uint16_t)(m_time + ringSpeed);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i) / (float)HALF_LENGTH;

        // Square the distance for circular appearance
        uint16_t distSquared = (uint16_t)(distFromCenter * distFromCenter * 65535.0f);

        // Create expanding rings
        int16_t wave = sin16((distSquared >> 1) * ringCount - m_time);

        // Convert to brightness
        uint8_t brightness = (uint8_t)((wave + 32768) >> 8);
        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = (uint8_t)(ctx.gHue + (distSquared >> 10));
        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 64), brightness);
        }
    }
}

void LGPRadialRippleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPRadialRippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Radial Ripple",
        "Complex radial wave interference",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
