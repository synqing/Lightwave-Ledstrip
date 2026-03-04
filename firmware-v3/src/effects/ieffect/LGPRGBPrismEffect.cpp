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
    // Palette-driven prism dispersion — 3 palette colors at different spatial frequencies
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;
    const float dispersion = 1.5f;

    // Trail persistence — long fade for smooth ambient colour blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, 10);

    m_prismAngle += speed * 0.02f * 60.0f * dt;

    // Three palette colors spaced 120° apart — the "spectral components"
    uint8_t hue1 = ctx.gHue;
    uint8_t hue2 = (uint8_t)(ctx.gHue + 85);
    uint8_t hue3 = (uint8_t)(ctx.gHue + 170);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        float normalizedDist = (float)dist / (float)HALF_LENGTH;

        // Each spectral component disperses at a slightly different rate
        float wave1 = sinf(normalizedDist * dispersion + m_prismAngle);
        float wave2 = sinf(normalizedDist * dispersion * 1.1f + m_prismAngle);
        float wave3 = sinf(normalizedDist * dispersion * 1.2f + m_prismAngle);

        // Convert sine to brightness envelope [0, 255], scaled by ctx.brightness via scale8
        uint8_t rawBright1 = (uint8_t)(128.0f + 127.0f * wave1);
        uint8_t rawBright2 = (uint8_t)(128.0f + 127.0f * wave2);
        uint8_t rawBright3 = (uint8_t)(128.0f + 127.0f * wave3);
        uint8_t bright1 = scale8(rawBright1, ctx.brightness);
        uint8_t bright2 = scale8(rawBright2, ctx.brightness);
        uint8_t bright3 = scale8(rawBright3, ctx.brightness);

        // Strip 1: first two spectral components dominate
        CRGB c1a = ctx.palette.getColor(hue1, bright1);
        CRGB c1b = ctx.palette.getColor(hue2, scale8(bright2, 128));
        CRGB strip1Color = c1a;
        strip1Color += c1b;

        // Strip 2: last two spectral components dominate
        CRGB c2a = ctx.palette.getColor(hue3, bright3);
        CRGB c2b = ctx.palette.getColor(hue2, scale8(bright2, 128));
        CRGB strip2Color = c2a;
        strip2Color += c2b;

        // Center convergence — all 3 components merge
        if (dist < 10) {
            uint8_t centerBoost = scale8((uint8_t)((10 - dist) * 6), ctx.brightness);
            CRGB centerColor = ctx.palette.getColor((uint8_t)(ctx.gHue + 42), centerBoost);
            strip1Color += centerColor;
            strip2Color += centerColor;
        }

        // Write symmetric center pair
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < STRIP_LENGTH) ctx.leds[left1] = strip1Color;
        if (right1 < STRIP_LENGTH) ctx.leds[right1] = strip1Color;

        uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
        uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
        if (left2 < ctx.ledCount) ctx.leds[left2] = strip2Color;
        if (right2 < ctx.ledCount) ctx.leds[right2] = strip2Color;
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
