/**
 * @file LGPSpiralVortexEffect.cpp
 * @brief LGP Spiral Vortex effect implementation
 */

#include "LGPSpiralVortexEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.05f;
constexpr int kSpiralArms = 4;
constexpr float kRadialFade = 0.5f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.01f, 0.15f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.001f, "timing", "x", false},
    {"spiral_arms", "Spiral Arms", 2.0f, 8.0f, (float)kSpiralArms, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"radial_fade", "Radial Fade", 0.0f, 1.0f, kRadialFade, plugins::EffectParameterType::FLOAT, 0.02f, "blend", "x", false},
};
}

LGPSpiralVortexEffect::LGPSpiralVortexEffect()
    : m_phase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_spiralArms(kSpiralArms)
    , m_radialFade(kRadialFade)
{
}

bool LGPSpiralVortexEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_spiralArms = kSpiralArms;
    m_radialFade = kRadialFade;
    return true;
}

void LGPSpiralVortexEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Creates rotating spiral patterns from center
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * m_phaseRate * 60.0f * dt;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Spiral equation
        float spiralAngle = normalizedDist * m_spiralArms * TWO_PI + m_phase;
        float spiral = sinf(spiralAngle);

        // Radial fade
        spiral *= (1.0f - normalizedDist * m_radialFade);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * spiral * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(spiralAngle * 255.0f / TWO_PI);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 128), brightness);
        }
    }
}

void LGPSpiralVortexEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSpiralVortexEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Spiral Vortex",
        "Rotating spiral arms",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

uint8_t LGPSpiralVortexEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPSpiralVortexEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPSpiralVortexEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.01f, 0.15f);
        return true;
    }
    if (strcmp(name, "spiral_arms") == 0) {
        m_spiralArms = (int)constrain(value, 2.0f, 8.0f);
        return true;
    }
    if (strcmp(name, "radial_fade") == 0) {
        m_radialFade = constrain(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float LGPSpiralVortexEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "spiral_arms") == 0) return (float)m_spiralArms;
    if (strcmp(name, "radial_fade") == 0) return m_radialFade;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
