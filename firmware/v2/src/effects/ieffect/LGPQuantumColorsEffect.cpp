/**
 * @file LGPQuantumColorsEffect.cpp
 * @brief LGP Quantum Colors effect implementation
 */

#include "LGPQuantumColorsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.001f;
constexpr int kNumStates = 4;
constexpr float kUncertaintyScale = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.0003f, 0.004f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.0002f, "timing", "x", false},
    {"num_states", "Num States", 2.0f, 8.0f, kNumStates, plugins::EffectParameterType::INT, 1.0f, "wave", "n", false},
    {"uncertainty_scale", "Uncertainty Scale", 0.5f, 2.0f, kUncertaintyScale, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
};
}

LGPQuantumColorsEffect::LGPQuantumColorsEffect()
    : m_waveFunction(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_numStates(kNumStates)
    , m_uncertaintyScale(kUncertaintyScale)
{
}

bool LGPQuantumColorsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_waveFunction = 0.0f;
    m_phaseRate = kPhaseRate;
    m_numStates = kNumStates;
    m_uncertaintyScale = kUncertaintyScale;
    return true;
}

void LGPQuantumColorsEffect::render(plugins::EffectContext& ctx) {
    // Colors exist in quantum states until "observed"
    float dt = ctx.getSafeDeltaSeconds();
    float intensity = ctx.brightness / 255.0f;

    m_waveFunction += ctx.speed * m_phaseRate * 60.0f * dt;  // dt-corrected

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float probability = sinf(m_waveFunction + normalizedDist * TWO_PI * m_numStates);
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

        uint8_t uncertainty = (uint8_t)(255.0f * (0.5f + 0.5f * sinf(distFromCenter * 0.2f * m_uncertaintyScale)));

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

uint8_t LGPQuantumColorsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPQuantumColorsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPQuantumColorsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.0003f, 0.004f);
        return true;
    }
    if (strcmp(name, "num_states") == 0) {
        m_numStates = constrain((int)lroundf(value), 2, 8);
        return true;
    }
    if (strcmp(name, "uncertainty_scale") == 0) {
        m_uncertaintyScale = constrain(value, 0.5f, 2.0f);
        return true;
    }
    return false;
}

float LGPQuantumColorsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "num_states") == 0) return m_numStates;
    if (strcmp(name, "uncertainty_scale") == 0) return m_uncertaintyScale;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
