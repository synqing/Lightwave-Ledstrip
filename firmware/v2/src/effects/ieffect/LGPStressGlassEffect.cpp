/**
 * @file LGPStressGlassEffect.cpp
 * @brief LGP Stress Glass effect implementation
 */

#include "LGPStressGlassEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPStressGlassEffect
namespace {
constexpr float kLGPStressGlassEffectSpeedScale = 1.0f;
constexpr float kLGPStressGlassEffectOutputGain = 1.0f;
constexpr float kLGPStressGlassEffectCentreBias = 1.0f;

float gLGPStressGlassEffectSpeedScale = kLGPStressGlassEffectSpeedScale;
float gLGPStressGlassEffectOutputGain = kLGPStressGlassEffectOutputGain;
float gLGPStressGlassEffectCentreBias = kLGPStressGlassEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPStressGlassEffectParameters[] = {
    {"lgpstress_glass_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPStressGlassEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpstress_glass_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPStressGlassEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpstress_glass_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPStressGlassEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPStressGlassEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPStressGlassEffect::LGPStressGlassEffect()
    : m_analyser(0.0f)
{
}

bool LGPStressGlassEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPStressGlassEffect
    gLGPStressGlassEffectSpeedScale = kLGPStressGlassEffectSpeedScale;
    gLGPStressGlassEffectOutputGain = kLGPStressGlassEffectOutputGain;
    gLGPStressGlassEffectCentreBias = kLGPStressGlassEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPStressGlassEffect

    m_analyser = 0.0f;
    return true;
}

void LGPStressGlassEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN STRESS GLASS - Photoelastic fringe field
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_analyser += 0.010f + 0.060f * speedNorm;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        float stress =
            expf(-dist * dist * 0.020f)
            + 0.65f * expf(-(dist - 6.0f) * (dist - 6.0f) * 0.030f)
            + 0.65f * expf(-(dist - 12.0f) * (dist - 12.0f) * 0.030f);

        stress = clamp01(stress);

        const float retard = (8.0f * stress) + m_analyser;
        const float fringe = sinf(retard);
        const float wave = clamp01(fringe * fringe);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(stress * 120.0f) + (int)(m_analyser * 12.0f));

        const float base = 0.08f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        ctx.leds[i] = ctx.palette.getColor(hueA, br);

        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            const uint8_t hueB = (uint8_t)(hueA + 24u);
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPStressGlassEffect
uint8_t LGPStressGlassEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPStressGlassEffectParameters) / sizeof(kLGPStressGlassEffectParameters[0]));
}

const plugins::EffectParameter* LGPStressGlassEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPStressGlassEffectParameters[index];
}

bool LGPStressGlassEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpstress_glass_effect_speed_scale") == 0) {
        gLGPStressGlassEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpstress_glass_effect_output_gain") == 0) {
        gLGPStressGlassEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpstress_glass_effect_centre_bias") == 0) {
        gLGPStressGlassEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPStressGlassEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpstress_glass_effect_speed_scale") == 0) return gLGPStressGlassEffectSpeedScale;
    if (strcmp(name, "lgpstress_glass_effect_output_gain") == 0) return gLGPStressGlassEffectOutputGain;
    if (strcmp(name, "lgpstress_glass_effect_centre_bias") == 0) return gLGPStressGlassEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPStressGlassEffect

void LGPStressGlassEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStressGlassEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Stress Glass",
        "Photoelastic birefringence fringes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
