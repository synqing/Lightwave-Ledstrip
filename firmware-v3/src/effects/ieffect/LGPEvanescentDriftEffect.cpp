// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPEvanescentDriftEffect.cpp
 * @brief LGP Evanescent Drift effect implementation
 */

#include "LGPEvanescentDriftEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPEvanescentDriftEffect::LGPEvanescentDriftEffect()
    : m_phase1(0)
    , m_phase2(32768)
{
}

bool LGPEvanescentDriftEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0;
    m_phase2 = 32768;
    return true;
}

void LGPEvanescentDriftEffect::render(plugins::EffectContext& ctx) {
    // Exponentially fading waves from edges - subtle ambient effect
    m_phase1 = (uint16_t)(m_phase1 + ctx.speed);
    m_phase2 = (uint16_t)(m_phase2 - ctx.speed);

    uint8_t alpha = (uint8_t)(255 - ctx.brightness);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t distFromCenter = centerPairDistance(i);
        uint8_t dist8 = (uint8_t)distFromCenter;

        // Exponential decay approximation
        uint8_t decay = 255;
        for (uint16_t j = 0; j < distFromCenter && j < 8; j++) {
            decay = scale8(decay, alpha);
        }

        // Wave patterns
        uint8_t wave1 = sin8((dist8 << 2) + (m_phase1 >> 8));
        uint8_t wave2 = sin8((dist8 << 2) + (m_phase2 >> 8));

        // Apply decay
        wave1 = scale8(wave1, decay);
        wave2 = scale8(wave2, decay);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + dist8), wave1);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + dist8 + 85), wave2);
        }
    }
}

void LGPEvanescentDriftEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPEvanescentDriftEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Evanescent Drift",
        "Ghostly drifting particles",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
