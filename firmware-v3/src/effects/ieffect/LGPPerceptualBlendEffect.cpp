/**
 * @file LGPPerceptualBlendEffect.cpp
 * @brief LGP Perceptual Blend — gradient kernel multi-stop palette ramp
 *
 * Proves the gradient kernel with:
 * - 3-stop ramps with eased interpolation (centre→mid→edge)
 * - Dual-edge differentiation (strip 1 and strip 2 use different ramps)
 * - Centre-origin sampling via gradient::uCenter()
 * - Phase-animated colour rotation
 */

#include "LGPPerceptualBlendEffect.h"
#include "../CoreEffects.h"
#include "../gradient/GradientCoord.h"
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
    // 3-stop gradient ramp sampled across centre-distance.
    // Strip 1: hue1 (centre) → hue2 (mid) → hue3 (edge)
    // Strip 2: hue2 (centre) → hue3 (mid) → hue1 (edge) — rotated complement

    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;

    // Trail persistence — long fade for smooth ambient colour blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, 8);

    m_phase += speed * 0.01f * 60.0f * dt;

    // Three palette anchors spaced 120° apart
    uint8_t hue1 = ctx.gHue;
    uint8_t hue2 = (uint8_t)(ctx.gHue + 85);
    uint8_t hue3 = (uint8_t)(ctx.gHue + 170);

    // Phase-animated shimmer on brightness
    float shimmerBase = 0.85f + 0.15f * sinf(m_phase * 1.7f);
    uint8_t bright = scale8((uint8_t)(shimmerBase * 255.0f), ctx.brightness);

    // Resolve palette colours once per frame (not per LED)
    CRGB col1 = ctx.palette.getColor(hue1, bright);
    CRGB col2 = ctx.palette.getColor(hue2, bright);
    CRGB col3 = ctx.palette.getColor(hue3, bright);

    // Build 3-stop ramps — once per frame, not per LED
    // Strip 1: hue1 at centre, hue2 at midpoint, hue3 at edge
    m_rampA.clear();
    m_rampA.addStop(0,   col1);
    m_rampA.addStop(128, col2);
    m_rampA.addStop(255, col3);
    m_rampA.setInterpolationMode(gradient::InterpolationMode::EASED);

    // Strip 2: rotated complement — hue2 at centre, hue3 at midpoint, hue1 at edge
    m_rampB.clear();
    m_rampB.addStop(0,   col2);
    m_rampB.addStop(128, col3);
    m_rampB.addStop(255, col1);
    m_rampB.setInterpolationMode(gradient::InterpolationMode::EASED);

    // Sample ramps across the half-strip using centre-distance coordinates
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        // Animated position with phase offset per distance
        float uBase = (float)dist / (float)HALF_LENGTH;
        float phaseOffset = 0.15f * sinf(m_phase + dist * 0.1f);
        float u = uBase + phaseOffset;

        // Clamp to [0, 1] — ramp handles clamping internally too
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;

        CRGB strip1Colour = m_rampA.samplef(u);
        CRGB strip2Colour = m_rampB.samplef(u);

        // Write symmetric centre pair — both strips
        gradient::writeCentrePairDual(ctx.leds, (uint8_t)dist,
                                      strip1Colour, strip2Colour, ctx.ledCount);
    }
}

void LGPPerceptualBlendEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerceptualBlendEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perceptual Blend",
        "Lab colour space mixing",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
