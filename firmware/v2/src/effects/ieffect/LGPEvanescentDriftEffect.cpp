/**
 * @file LGPEvanescentDriftEffect.cpp
 * @brief LGP Evanescent Drift effect implementation
 */

#include "LGPEvanescentDriftEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseStep = 1.0f;
constexpr int kDecayDepth = 8;
constexpr float kWaveScale = 4.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_step", "Phase Step", 0.25f, 4.0f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"decay_depth", "Decay Depth", 2.0f, 16.0f, (float)kDecayDepth, plugins::EffectParameterType::INT, 1.0f, "blend", "", false},
    {"wave_scale", "Wave Scale", 1.0f, 8.0f, kWaveScale, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "x", false},
};
}

LGPEvanescentDriftEffect::LGPEvanescentDriftEffect()
    : m_phase1(0)
    , m_phase2(32768)
    , m_phaseStep(kPhaseStep)
    , m_decayDepth(kDecayDepth)
    , m_waveScale(kWaveScale)
{
}

bool LGPEvanescentDriftEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0;
    m_phase2 = 32768;
    m_phaseStep = kPhaseStep;
    m_decayDepth = kDecayDepth;
    m_waveScale = kWaveScale;
    return true;
}

void LGPEvanescentDriftEffect::render(plugins::EffectContext& ctx) {
    // Exponentially fading waves from edges - subtle ambient effect
    uint16_t phaseStep = (uint16_t)(ctx.speed * m_phaseStep);
    m_phase1 = (uint16_t)(m_phase1 + phaseStep);
    m_phase2 = (uint16_t)(m_phase2 - phaseStep);

    uint8_t alpha = (uint8_t)(255 - ctx.brightness);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t distFromCenter = centerPairDistance(i);
        uint8_t dist8 = (uint8_t)distFromCenter;

        // Exponential decay approximation
        uint8_t decay = 255;
        for (uint16_t j = 0; j < distFromCenter && j < (uint16_t)m_decayDepth; j++) {
            decay = scale8(decay, alpha);
        }

        // Wave patterns
        uint8_t phase1 = (uint8_t)(dist8 * m_waveScale);
        uint8_t wave1 = sin8(phase1 + (m_phase1 >> 8));
        uint8_t wave2 = sin8(phase1 + (m_phase2 >> 8));

        // Apply decay
        wave1 = scale8(wave1, decay);
        wave2 = scale8(wave2, decay);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + dist8), wave1);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + dist8 + 85), wave2);
        }
    }
}

void LGPEvanescentDriftEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPEvanescentDriftEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Evanescent Drift",
        "Ghostly drifting particles",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

uint8_t LGPEvanescentDriftEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPEvanescentDriftEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPEvanescentDriftEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.25f, 4.0f);
        return true;
    }
    if (strcmp(name, "decay_depth") == 0) {
        m_decayDepth = (int)constrain(value, 2.0f, 16.0f);
        return true;
    }
    if (strcmp(name, "wave_scale") == 0) {
        m_waveScale = constrain(value, 1.0f, 8.0f);
        return true;
    }
    return false;
}

float LGPEvanescentDriftEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    if (strcmp(name, "decay_depth") == 0) return (float)m_decayDepth;
    if (strcmp(name, "wave_scale") == 0) return m_waveScale;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
