/**
 * @file LGPHolographicEffect.cpp
 * @brief LGP Holographic effect implementation
 */

#include "LGPHolographicEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kLayer2Weight = 0.7f;
constexpr float kLayer3Weight = 0.5f;
constexpr float kShimmerWeight = 0.3f;

const plugins::EffectParameter kParameters[] = {
    {"layer2_weight", "Layer 2", 0.1f, 1.5f, kLayer2Weight, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"layer3_weight", "Layer 3", 0.1f, 1.5f, kLayer3Weight, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"shimmer_weight", "Shimmer", 0.0f, 1.0f, kShimmerWeight, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
};
}

LGPHolographicEffect::LGPHolographicEffect()
    : m_phase1(0.0f)
    , m_phase2(0.0f)
    , m_phase3(0.0f)
    , m_layer2Weight(kLayer2Weight)
    , m_layer3Weight(kLayer3Weight)
    , m_shimmerWeight(kShimmerWeight)
{
}

bool LGPHolographicEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_phase3 = 0.0f;
    m_layer2Weight = kLayer2Weight;
    m_layer3Weight = kLayer3Weight;
    m_shimmerWeight = kShimmerWeight;
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
        layerSum += sinf(dist * 0.15f + m_phase2) * m_layer2Weight;

        // Layer 3 - Fast, tight pattern
        layerSum += sinf(dist * 0.3f + m_phase3) * m_layer3Weight;

        // Layer 4 - Shimmer (3.0x creates sharp interference bands)
        layerSum += sinf(dist * 0.6f - m_phase1 * 3.0f) * m_shimmerWeight;

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

uint8_t LGPHolographicEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPHolographicEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPHolographicEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "layer2_weight") == 0) {
        m_layer2Weight = constrain(value, 0.1f, 1.5f);
        return true;
    }
    if (strcmp(name, "layer3_weight") == 0) {
        m_layer3Weight = constrain(value, 0.1f, 1.5f);
        return true;
    }
    if (strcmp(name, "shimmer_weight") == 0) {
        m_shimmerWeight = constrain(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float LGPHolographicEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "layer2_weight") == 0) return m_layer2Weight;
    if (strcmp(name, "layer3_weight") == 0) return m_layer3Weight;
    if (strcmp(name, "shimmer_weight") == 0) return m_shimmerWeight;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
