// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPTimeCrystalEffect.cpp
 * @brief LGP Time Crystal effect implementation
 */

#include "LGPTimeCrystalEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPTimeCrystalEffect::LGPTimeCrystalEffect()
    : m_phase1(0)
    , m_phase2(0)
    , m_phase3(0)
{
}

bool LGPTimeCrystalEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0;
    m_phase2 = 0;
    m_phase3 = 0;
    return true;
}

void LGPTimeCrystalEffect::render(plugins::EffectContext& ctx) {
    // Perpetual motion patterns with non-repeating periods
    float speedNorm = ctx.speed / 50.0f;

    m_phase1 = (uint16_t)(m_phase1 + (uint16_t)(speedNorm * 100.0f));
    m_phase2 = (uint16_t)(m_phase2 + (uint16_t)(speedNorm * 161.8f));  // Golden ratio
    m_phase3 = (uint16_t)(m_phase3 + (uint16_t)(speedNorm * 271.8f));  // e

    uint8_t crystallinity = ctx.brightness;
    const uint8_t dimensions = 3;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i) / (float)HALF_LENGTH;

        float crystal = 0.0f;

        // Dimension 1
        crystal += sin16((uint16_t)(m_phase1 + i * 400)) / 32768.0f;

        // Dimension 2
        crystal += sin16((uint16_t)(m_phase2 + i * 650)) / 65536.0f;

        // Dimension 3
        crystal += sin16((uint16_t)(m_phase3 + i * 1050)) / 131072.0f;

        crystal = crystal / dimensions;
        uint8_t brightness = (uint8_t)(128 + (int8_t)(crystal * crystallinity));

        uint8_t paletteIndex = (uint8_t)(crystal * 20.0f) + (uint8_t)(distFromCenter * 20.0f);

        // Use bright color from palette instead of white (paletteIndex=0)
        if (fabsf(crystal) > 0.9f) {
            brightness = 240;  // Slightly below 255 to avoid white guardrail
            paletteIndex = (uint8_t)(fabsf(crystal) * 50.0f);  // Use crystal value for color
        }

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 85), brightness);
        }
    }
}

void LGPTimeCrystalEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPTimeCrystalEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Time Crystal",
        "Periodic structure in time",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
