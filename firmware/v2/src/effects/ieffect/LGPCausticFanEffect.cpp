/**
 * @file LGPCausticFanEffect.cpp
 * @brief LGP Caustic Fan effect implementation
 */

#include "LGPCausticFanEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kCurvature = 1.5f;
constexpr float kSeparation = 1.5f;
constexpr float kGain = 12.0f;
constexpr float kPhaseStep = 0.25f;

const plugins::EffectParameter kParameters[] = {
    {"curvature", "Curvature", 0.4f, 3.0f, kCurvature, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
    {"separation", "Separation", 0.5f, 3.5f, kSeparation, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
    {"gain", "Gain", 2.0f, 24.0f, kGain, plugins::EffectParameterType::FLOAT, 0.5f, "blend", "", false},
    {"phase_step", "Phase Step", 0.1f, 1.2f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", true},
};
}

LGPCausticFanEffect::LGPCausticFanEffect()
    : m_time(0)
    , m_curvature(kCurvature)
    , m_separation(kSeparation)
    , m_gain(kGain)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPCausticFanEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_curvature = kCurvature;
    m_separation = kSeparation;
    m_gain = kGain;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPCausticFanEffect::render(plugins::EffectContext& ctx) {
    // Two virtual focusing fans creating drifting caustic envelopes
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));

    float intensityNorm = ctx.brightness / 255.0f;
    const float curvature = m_curvature;
    const float separation = m_separation;
    const float gain = m_gain;
    float animPhase = m_time / 256.0f;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)i - (float)CENTER_LEFT;

        float def1 = curvature * (x - separation) + sinf(animPhase);
        float def2 = -curvature * (x + separation) + sinf(animPhase * 1.21f);
        float diff = fabsf(def1 - def2);

        float caustic = 1.0f / (1.0f + diff * diff * gain);
        float envelope = 1.0f / (1.0f + fabsf(x) * 0.08f);
        float brightnessF = caustic * envelope * 255.0f * intensityNorm;

        brightnessF = constrain(brightnessF + (sin8((uint8_t)(i * 3 + (m_time >> 2))) >> 2), 0.0f, 255.0f);

        uint8_t brightness = (uint8_t)brightnessF;
        uint8_t hue = (uint8_t)(ctx.gHue + (uint8_t)(x * 1.5f) + (m_time >> 4));

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 96), brightU8);
        }
    }
}

void LGPCausticFanEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPCausticFanEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Caustic Fan",
        "Focused light caustics",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPCausticFanEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPCausticFanEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPCausticFanEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "curvature") == 0) {
        m_curvature = constrain(value, 0.4f, 3.0f);
        return true;
    }
    if (strcmp(name, "separation") == 0) {
        m_separation = constrain(value, 0.5f, 3.5f);
        return true;
    }
    if (strcmp(name, "gain") == 0) {
        m_gain = constrain(value, 2.0f, 24.0f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.1f, 1.2f);
        return true;
    }
    return false;
}

float LGPCausticFanEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "curvature") == 0) return m_curvature;
    if (strcmp(name, "separation") == 0) return m_separation;
    if (strcmp(name, "gain") == 0) return m_gain;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
