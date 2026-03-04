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

LGPColorTemperatureEffect::LGPColorTemperatureEffect()
    : m_phase(0.0f)
{
}

bool LGPColorTemperatureEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPColorTemperatureEffect::render(plugins::EffectContext& ctx) {
    // Palette-driven warm↔cool temperature gradient with animated convergence
    // Warm end (gHue) and cool end (gHue+128) converge toward neutral at center
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;

    // Trail persistence — long fade for smooth ambient colour blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, 8);

    m_phase += speed * 0.015f * 60.0f * dt;

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        float normalizedDist = (float)dist / (float)HALF_LENGTH;

        // Animated breathing modulation on the temperature spread
        float breathe = 0.7f + 0.3f * sinf(m_phase * 0.5f);
        float spread = normalizedDist * breathe;

        // Warm hue at edges, cool hue at center — both from palette
        uint8_t warmHue = (uint8_t)(ctx.gHue + (uint8_t)(spread * 40.0f));
        uint8_t coolHue = (uint8_t)(ctx.gHue + 128 + (uint8_t)(spread * 40.0f));

        // Brightness: brighter at edges, softer at center (convergence zone)
        uint8_t edgeBright = (uint8_t)(153 + (uint8_t)(normalizedDist * 102.0f)); // 0.6*255=153, 0.4*255=102
        // Subtle shimmer at the convergence point
        float shimmer = 1.0f + 0.15f * sinf(m_phase * 2.0f + dist * 0.1f);
        uint8_t warmBright = scale8((uint8_t)(edgeBright * shimmer), ctx.brightness);
        uint8_t coolBright = scale8((uint8_t)(edgeBright * shimmer), ctx.brightness);

        CRGB warmColor = ctx.palette.getColor(warmHue, warmBright);
        CRGB coolColor = ctx.palette.getColor(coolHue, coolBright);

        // Strip 1: warm spectrum
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < STRIP_LENGTH) ctx.leds[left1] = warmColor;
        if (right1 < STRIP_LENGTH) ctx.leds[right1] = warmColor;

        // Strip 2: cool spectrum
        uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
        uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
        if (left2 < ctx.ledCount) ctx.leds[left2] = coolColor;
        if (right2 < ctx.ledCount) ctx.leds[right2] = coolColor;
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
