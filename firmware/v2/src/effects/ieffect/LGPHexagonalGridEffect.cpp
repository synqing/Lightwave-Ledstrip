/**
 * @file LGPHexagonalGridEffect.cpp
 * @brief LGP Hexagonal Grid effect implementation
 */

#include "LGPHexagonalGridEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.01f;
constexpr float kHexSize = 10.0f;
constexpr float kPatternGamma = 0.3f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.002f, 0.04f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.001f, "timing", "x", false},
    {"hex_size", "Hex Size", 4.0f, 20.0f, kHexSize, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"pattern_gamma", "Pattern Gamma", 0.1f, 1.0f, kPatternGamma, plugins::EffectParameterType::FLOAT, 0.02f, "blend", "", false},
};
}

LGPHexagonalGridEffect::LGPHexagonalGridEffect()
    : m_phase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_hexSize(kHexSize)
    , m_patternGamma(kPatternGamma)
{
}

bool LGPHexagonalGridEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_hexSize = kHexSize;
    m_patternGamma = kPatternGamma;
    return true;
}

void LGPHexagonalGridEffect::render(plugins::EffectContext& ctx) {
    // Three waves at 120 degrees create hexagonal patterns
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * m_phaseRate * 60.0f * dt;  // dt-corrected

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float pos = centerPairSignedPosition((uint16_t)i) / (float)HALF_LENGTH;

        // Three waves at 120 degree angles
        float wave1 = sinf(pos * m_hexSize * TWO_PI + m_phase);
        float wave2 = sinf(pos * m_hexSize * TWO_PI + m_phase + TWO_PI / 3.0f);
        float wave3 = sinf(pos * m_hexSize * TWO_PI + m_phase + 2.0f * TWO_PI / 3.0f);

        // Multiplicative creates cells
        float pattern = fabsf(wave1 * wave2 * wave3);
        pattern = powf(pattern, m_patternGamma);

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

uint8_t LGPHexagonalGridEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPHexagonalGridEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPHexagonalGridEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.002f, 0.04f);
        return true;
    }
    if (strcmp(name, "hex_size") == 0) {
        m_hexSize = constrain(value, 4.0f, 20.0f);
        return true;
    }
    if (strcmp(name, "pattern_gamma") == 0) {
        m_patternGamma = constrain(value, 0.1f, 1.0f);
        return true;
    }
    return false;
}

float LGPHexagonalGridEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "hex_size") == 0) return m_hexSize;
    if (strcmp(name, "pattern_gamma") == 0) return m_patternGamma;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
