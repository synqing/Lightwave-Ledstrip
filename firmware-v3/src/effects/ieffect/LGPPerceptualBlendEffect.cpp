/**
 * @file LGPPerceptualBlendEffect.cpp
 * @brief LGP Perceptual Blend effect implementation
 */

#include "LGPPerceptualBlendEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerceptualBlendEffect::LGPPerceptualBlendEffect()
    : m_phase(0.0f)
{
}

bool LGPPerceptualBlendEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPPerceptualBlendEffect::render(plugins::EffectContext& ctx) {
    // Palette-driven perceptual blend — 3 palette colors interpolated with
    // Lab-inspired smooth weighting across radial distance.
    // Strip 1 blends hue1→hue2, Strip 2 blends hue2→hue3 (complementary pair).
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;

    // Trail persistence — long fade for smooth ambient colour blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, 8);

    m_phase += speed * 0.01f * 60.0f * dt;

    // Three palette anchor points spaced 120° apart
    uint8_t hue1 = ctx.gHue;
    uint8_t hue2 = (uint8_t)(ctx.gHue + 85);
    uint8_t hue3 = (uint8_t)(ctx.gHue + 170);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        float normalizedDist = (float)dist / (float)HALF_LENGTH;

        // Perceptual blend weights — sinusoidal crossfade with phase animation
        // This creates smooth, Lab-inspired transitions between palette anchors
        float crossfade = 0.5f + 0.5f * sinf(m_phase + normalizedDist * 3.14159f);
        float shimmer = 0.85f + 0.15f * sinf(m_phase * 1.7f + dist * 0.15f);

        uint8_t bright = scale8((uint8_t)(shimmer * 255.0f), ctx.brightness);

        // Strip 1: blend between hue1 and hue2
        CRGB colorA = ctx.palette.getColor(hue1, bright);
        CRGB colorB = ctx.palette.getColor(hue2, bright);
        CRGB strip1Color = colorA.lerp8(colorB, (uint8_t)(crossfade * 255.0f));

        // Strip 2: blend between hue2 and hue3 (complementary pair)
        CRGB colorC = ctx.palette.getColor(hue2, bright);
        CRGB colorD = ctx.palette.getColor(hue3, bright);
        CRGB strip2Color = colorC.lerp8(colorD, (uint8_t)(crossfade * 255.0f));

        // Write symmetric center pair — Strip 1
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < STRIP_LENGTH) ctx.leds[left1] = strip1Color;
        if (right1 < STRIP_LENGTH) ctx.leds[right1] = strip1Color;

        // Write symmetric center pair — Strip 2
        uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
        uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
        if (left2 < ctx.ledCount) ctx.leds[left2] = strip2Color;
        if (right2 < ctx.ledCount) ctx.leds[right2] = strip2Color;
    }
}

void LGPPerceptualBlendEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerceptualBlendEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perceptual Blend",
        "Lab color space mixing",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
