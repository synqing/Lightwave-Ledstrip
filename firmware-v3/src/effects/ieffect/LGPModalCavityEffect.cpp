// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPModalCavityEffect.cpp
 * @brief LGP Modal Cavity effect implementation
 */

#include "LGPModalCavityEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPModalCavityEffect::LGPModalCavityEffect()
    : m_time(0)
{
}

bool LGPModalCavityEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPModalCavityEffect::render(plugins::EffectContext& ctx) {
    // Excite specific waveguide modes
    m_time = (uint16_t)(m_time + ctx.speed);

    const uint8_t modeNumber = 8;
    const uint8_t beatMode = (uint8_t)(modeNumber + 2);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)centerPairDistance(i) / (float)HALF_LENGTH;

        // Primary mode
        int16_t mode1 = sin16((uint16_t)(x * modeNumber * 32768.0f));

        // Beat mode
        int16_t mode2 = sin16((uint16_t)(x * beatMode * 32768.0f) + m_time);

        // Combine modes
        int16_t combined = (int16_t)((mode1 >> 1) + (mode2 >> 2));
        uint8_t brightness = (uint8_t)((combined + 32768) >> 8);

        // Apply cosine taper
        uint8_t taper = (uint8_t)(cos8((uint8_t)(x * 255.0f)) >> 1);
        brightness = scale8(brightness, (uint8_t)(128 + taper));
        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = (uint8_t)(ctx.gHue + (modeNumber * 12));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 64), brightness);
        }
    }
}

void LGPModalCavityEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPModalCavityEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Modal Cavity",
        "Resonant optical cavity modes",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
