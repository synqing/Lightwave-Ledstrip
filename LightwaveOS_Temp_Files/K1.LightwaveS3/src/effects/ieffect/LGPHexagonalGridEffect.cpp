/**
 * @file LGPHexagonalGridEffect.cpp
 * @brief LGP Hexagonal Grid effect implementation
 */

#include "LGPHexagonalGridEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPHexagonalGridEffect::LGPHexagonalGridEffect()
    : m_phase(0.0f)
{
}

bool LGPHexagonalGridEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPHexagonalGridEffect::render(plugins::EffectContext& ctx) {
    // Three waves at 120 degrees create hexagonal patterns
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * 0.01f;

    const float hexSize = 10.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float pos = centerPairSignedPosition((uint16_t)i) / (float)HALF_LENGTH;

        // Three waves at 120 degree angles
        float wave1 = sinf(pos * hexSize * TWO_PI + m_phase);
        float wave2 = sinf(pos * hexSize * TWO_PI + m_phase + TWO_PI / 3.0f);
        float wave3 = sinf(pos * hexSize * TWO_PI + m_phase + 2.0f * TWO_PI / 3.0f);

        // Multiplicative creates cells
        float pattern = fabsf(wave1 * wave2 * wave3);
        pattern = powf(pattern, 0.3f);

        uint8_t brightness = (uint8_t)(pattern * 255.0f * intensityNorm);
        uint8_t hue = (uint8_t)(ctx.gHue + (uint8_t)(pattern * 60.0f) + (i >> 1));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 60), brightness);
        }
    }
}

void LGPHexagonalGridEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPHexagonalGridEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Hexagonal Grid",
        "Hexagonal cell structure",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
