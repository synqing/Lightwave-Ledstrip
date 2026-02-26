/**
 * @file LGPHolographicVortexEffect.cpp
 * @brief LGP Holographic Vortex effect implementation
 */

#include "LGPHolographicVortexEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr int kSpiralCount = 3;
constexpr float kTightnessScale = 1.0f;
constexpr float kPhaseStep = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"spiral_count", "Spiral Count", 1.0f, 8.0f, (float)kSpiralCount, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"tightness_scale", "Tightness", 0.5f, 2.5f, kTightnessScale, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
    {"phase_step", "Phase Step", 0.25f, 3.0f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
};
}

LGPHolographicVortexEffect::LGPHolographicVortexEffect()
    : m_time(0)
    , m_spiralCount(kSpiralCount)
    , m_tightnessScale(kTightnessScale)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPHolographicVortexEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_spiralCount = kSpiralCount;
    m_tightnessScale = kTightnessScale;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPHolographicVortexEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Spiral interference pattern with depth illusion
    m_time = (uint16_t)(m_time + (uint16_t)((ctx.speed << 1) * m_phaseStep));
    float tightnessF = (ctx.brightness >> 2) * m_tightnessScale;
    if (tightnessF > 255.0f) tightnessF = 255.0f;
    uint8_t tightness = (uint8_t)tightnessF;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i);
        float r = distFromCenter / (float)HALF_LENGTH;

        // Symmetric azimuthal angle
        uint16_t theta = (uint16_t)(distFromCenter * 410.0f);

        // Spiral phase
        uint16_t phase = (uint16_t)(m_spiralCount * theta + (uint16_t)(tightness * r * 65535.0f) - m_time);

        uint8_t brightness = sin8(phase >> 8);
        uint8_t paletteIndex = (uint8_t)(phase >> 10);

        // Add depth via brightness modulation
        brightness = scale8(brightness, (uint8_t)(255 - (uint8_t)(r * 127.0f)));
        brightness = scale8(brightness, ctx.brightness);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 128), brightness);
        }
    }
}

void LGPHolographicVortexEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPHolographicVortexEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Holographic Vortex",
        "Deep 3D vortex illusion",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPHolographicVortexEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPHolographicVortexEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPHolographicVortexEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "spiral_count") == 0) {
        m_spiralCount = (int)constrain(value, 1.0f, 8.0f);
        return true;
    }
    if (strcmp(name, "tightness_scale") == 0) {
        m_tightnessScale = constrain(value, 0.5f, 2.5f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.25f, 3.0f);
        return true;
    }
    return false;
}

float LGPHolographicVortexEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "spiral_count") == 0) return (float)m_spiralCount;
    if (strcmp(name, "tightness_scale") == 0) return m_tightnessScale;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
