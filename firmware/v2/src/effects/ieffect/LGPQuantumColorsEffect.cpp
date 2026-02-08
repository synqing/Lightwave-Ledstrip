/**
 * @file LGPQuantumColorsEffect.cpp
 * @brief LGP Quantum Colors effect implementation
 */

#include "LGPQuantumColorsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPQuantumColorsEffect::LGPQuantumColorsEffect()
    : m_waveFunction(0.0f)
{
}

bool LGPQuantumColorsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_waveFunction = 0.0f;
    return true;
}

void LGPQuantumColorsEffect::render(plugins::EffectContext& ctx) {
    // Colors exist in quantum states until "observed"
    float dt = ctx.getSafeDeltaSeconds();
    float intensity = ctx.brightness / 255.0f;

    m_waveFunction += ctx.speed * 0.001f * 60.0f * dt;  // dt-corrected

    const int numStates = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float probability = sinf(m_waveFunction + normalizedDist * TWO_PI * numStates);
        probability = probability * probability;

        uint8_t paletteOffset;
        if (probability < 0.25f) {
            paletteOffset = 0;
        } else if (probability < 0.5f) {
            paletteOffset = 10;
        } else if (probability < 0.75f) {
            paletteOffset = 20;
        } else {
            paletteOffset = 30;
        }

        uint8_t uncertainty = (uint8_t)(255.0f * (0.5f + 0.5f * sinf(distFromCenter * 0.2f)));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), (uint8_t)(uncertainty * intensity));
        if (i + STRIP_LENGTH < ctx.ledCount) {
            // Complementary color pairing: Strip 2 uses hue + 128 (180deg offset) for
            // intentional complementary color contrast, not rainbow cycling.
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(ctx.gHue + paletteOffset + 128),
                (uint8_t)((255 - uncertainty) * intensity));
        }
    }
}

void LGPQuantumColorsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPQuantumColorsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Quantum Colors",
        "Quantized energy levels",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
