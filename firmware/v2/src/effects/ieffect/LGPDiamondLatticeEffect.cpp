/**
 * @file LGPDiamondLatticeEffect.cpp
 * @brief LGP Diamond Lattice effect implementation
 */

#include "LGPDiamondLatticeEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.02f;
constexpr float kDiamondFreq = 6.0f;
constexpr float kRadialFalloff = 0.5f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.005f, 0.08f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.001f, "timing", "x", false},
    {"diamond_freq", "Diamond Frequency", 2.0f, 12.0f, kDiamondFreq, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"radial_falloff", "Radial Falloff", 0.0f, 1.2f, kRadialFalloff, plugins::EffectParameterType::FLOAT, 0.02f, "blend", "x", false},
};
}

LGPDiamondLatticeEffect::LGPDiamondLatticeEffect()
    : m_phase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_diamondFreq(kDiamondFreq)
    , m_radialFalloff(kRadialFalloff)
{
}

bool LGPDiamondLatticeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_diamondFreq = kDiamondFreq;
    m_radialFalloff = kRadialFalloff;
    return true;
}

void LGPDiamondLatticeEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Angled wave fronts create diamond patterns from center
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * m_phaseRate * 60.0f * dt;  // dt-corrected

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Create crossing diagonal waves from center
        float wave1 = sinf((normalizedDist + m_phase) * m_diamondFreq * TWO_PI);
        float wave2 = sinf((normalizedDist - m_phase) * m_diamondFreq * TWO_PI);

        // Interference creates diamond nodes
        float diamond = fabsf(wave1 * wave2);
        diamond = sqrtf(diamond);
        diamond *= (1.0f - normalizedDist * m_radialFalloff);
        if (diamond < 0.0f) diamond = 0.0f;

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

uint8_t LGPDiamondLatticeEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPDiamondLatticeEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPDiamondLatticeEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.005f, 0.08f);
        return true;
    }
    if (strcmp(name, "diamond_freq") == 0) {
        m_diamondFreq = constrain(value, 2.0f, 12.0f);
        return true;
    }
    if (strcmp(name, "radial_falloff") == 0) {
        m_radialFalloff = constrain(value, 0.0f, 1.2f);
        return true;
    }
    return false;
}

float LGPDiamondLatticeEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "diamond_freq") == 0) return m_diamondFreq;
    if (strcmp(name, "radial_falloff") == 0) return m_radialFalloff;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
