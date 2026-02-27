// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPRGBPrismEffect.cpp
 * @brief LGP RGB Prism effect implementation
 */

#include "LGPRGBPrismEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPRGBPrismEffect::LGPRGBPrismEffect()
    : m_prismAngle(0.0f)
{
}

bool LGPRGBPrismEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_prismAngle = 0.0f;
    return true;
}

void LGPRGBPrismEffect::render(plugins::EffectContext& ctx) {
    // Simulates light passing through a prism
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    const float dispersion = 1.5f;

    m_prismAngle += speed * 0.02f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float redAngle = sinf(normalizedDist * dispersion + m_prismAngle);
        float greenAngle = sinf(normalizedDist * dispersion * 1.1f + m_prismAngle);
        float blueAngle = sinf(normalizedDist * dispersion * 1.2f + m_prismAngle);

        // Strip 1: Red channel dominant
        ctx.leds[i].r = (uint8_t)((128.0f + 127.0f * redAngle) * intensity);
        ctx.leds[i].g = (uint8_t)(64.0f * fabsf(greenAngle) * intensity);
        ctx.leds[i].b = 0;

        // Strip 2: Blue channel dominant
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH].r = 0;
            ctx.leds[i + STRIP_LENGTH].g = (uint8_t)(64.0f * fabsf(greenAngle) * intensity);
            ctx.leds[i + STRIP_LENGTH].b = (uint8_t)((128.0f + 127.0f * blueAngle) * intensity);
        }

        // Green emerges at center
        if (distFromCenter < 10.0f) {
            ctx.leds[i].g += (uint8_t)(128.0f * intensity);
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH].g += (uint8_t)(128.0f * intensity);
            }
        }
    }
}

void LGPRGBPrismEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPRGBPrismEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP RGB Prism",
        "RGB component splitting",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
