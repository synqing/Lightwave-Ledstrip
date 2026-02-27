/**
 * @file LGPDiamondLatticeEffect.cpp
 * @brief LGP Diamond Lattice effect implementation
 */

#include "LGPDiamondLatticeEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPDiamondLatticeEffect::LGPDiamondLatticeEffect()
    : m_phase(0.0f)
{
}

bool LGPDiamondLatticeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPDiamondLatticeEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Angled wave fronts create diamond patterns from center
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * 0.02f;

    const float diamondFreq = 6.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Create crossing diagonal waves from center
        float wave1 = sinf((normalizedDist + m_phase) * diamondFreq * TWO_PI);
        float wave2 = sinf((normalizedDist - m_phase) * diamondFreq * TWO_PI);

        // Interference creates diamond nodes
        float diamond = fabsf(wave1 * wave2);
        diamond = powf(diamond, 0.5f);

        uint8_t brightness = (uint8_t)(diamond * 255.0f * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2.0f);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 128), brightness);
        }
    }
}

void LGPDiamondLatticeEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPDiamondLatticeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Diamond Lattice",
        "Interwoven diamond patterns",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
