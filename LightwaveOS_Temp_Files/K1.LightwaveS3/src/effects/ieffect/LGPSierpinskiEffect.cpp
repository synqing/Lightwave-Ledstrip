/**
 * @file LGPSierpinskiEffect.cpp
 * @brief LGP Sierpinski effect implementation
 */

#include "LGPSierpinskiEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPSierpinskiEffect::LGPSierpinskiEffect()
    : m_iteration(0)
{
}

bool LGPSierpinskiEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_iteration = 0;
    return true;
}

void LGPSierpinskiEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Fractal triangle patterns through recursive interference
    float intensityNorm = ctx.brightness / 255.0f;

    m_iteration = (uint16_t)(m_iteration + (ctx.speed >> 2));

    const int maxDepth = 5;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        uint16_t x = centerPairDistance((uint16_t)i);
        uint16_t y = m_iteration >> 4;

        // XOR creates Sierpinski triangle
        uint16_t pattern = x ^ y;

        // Count bits for fractal depth
        uint8_t bitCount = 0;
        for (int d = 0; d < maxDepth; d++) {
            if (pattern & (1 << d)) bitCount++;
        }

        float smooth = sinf(bitCount * PI / (float)maxDepth);

        uint8_t brightness = (uint8_t)(smooth * 255.0f * intensityNorm);
        uint8_t hue = (uint8_t)(ctx.gHue + (bitCount * 30));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 128), brightness);
        }
    }
}

void LGPSierpinskiEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSierpinskiEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Sierpinski",
        "Fractal triangle generation",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
