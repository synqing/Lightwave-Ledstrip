// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPColorTemperatureEffect.cpp
 * @brief LGP Color Temperature effect implementation
 */

#include "LGPColorTemperatureEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPColorTemperatureEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void LGPColorTemperatureEffect::render(plugins::EffectContext& ctx) {
    // Warm colors from edges meet cool colors at center, creating white
    float intensity = ctx.brightness / 255.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        CRGB warm, cool;

        // Warm side (reds/oranges)
        warm.r = 255;
        warm.g = (uint8_t)(180.0f - normalizedDist * 100.0f);
        warm.b = (uint8_t)(50.0f + normalizedDist * 50.0f);

        // Cool side (blues/cyans)
        cool.r = (uint8_t)(150.0f + normalizedDist * 50.0f);
        cool.g = (uint8_t)(200.0f + normalizedDist * 55.0f);
        cool.b = 255;

        ctx.leds[i] = warm.scale8((uint8_t)(intensity * 255.0f));
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = cool.scale8((uint8_t)(intensity * 255.0f));
        }
    }
}

void LGPColorTemperatureEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPColorTemperatureEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Color Temperature",
        "Blackbody radiation gradients",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
