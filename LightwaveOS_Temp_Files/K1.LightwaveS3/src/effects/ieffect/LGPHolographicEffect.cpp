/**
 * @file LGPHolographicEffect.cpp
 * @brief LGP Holographic effect implementation
 */

#include "LGPHolographicEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPHolographicEffect::LGPHolographicEffect()
    : m_phase1(0.0f)
    , m_phase2(0.0f)
    , m_phase3(0.0f)
{
}

bool LGPHolographicEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_phase3 = 0.0f;
    return true;
}

void LGPHolographicEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN HOLOGRAPHIC - Creates depth illusion through multi-layer interference
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase1 += speedNorm * 0.02f;
    m_phase2 += speedNorm * 0.03f;
    m_phase3 += speedNorm * 0.05f;

    const int numLayers = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);

        float layerSum = 0.0f;

        // Layer 1 - Slow, wide pattern
        layerSum += sinf(dist * 0.05f + m_phase1);

        // Layer 2 - Medium pattern
        layerSum += sinf(dist * 0.15f + m_phase2) * 0.7f;

        // Layer 3 - Fast, tight pattern
        layerSum += sinf(dist * 0.3f + m_phase3) * 0.5f;

        // Layer 4 - Very fast shimmer
        layerSum += sinf(dist * 0.6f - m_phase1 * 3.0f) * 0.3f;

        // Normalize
        layerSum = layerSum / (float)numLayers;
        layerSum = tanhf(layerSum);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * layerSum * intensityNorm);

        // Chromatic dispersion effect
        uint8_t paletteIndex1 = (uint8_t)((dist * 0.5f) + (layerSum * 20.0f));
        int paletteIndex2 = (int)(128.0f - (dist * 0.5f) - (layerSum * 20.0f));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex1), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + (uint8_t)paletteIndex2), brightness);
        }
    }
}

void LGPHolographicEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPHolographicEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Holographic",
        "Holographic interference depth layers",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
