/**
 * @file LGPStressGlassMeltEffect.cpp
 * @brief LGP Stress Glass (Melt) effect implementation
 */

#include "LGPStressGlassMeltEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPStressGlassMeltEffect
namespace {
constexpr float kLGPStressGlassMeltEffectSpeedScale = 1.0f;
constexpr float kLGPStressGlassMeltEffectOutputGain = 1.0f;
constexpr float kLGPStressGlassMeltEffectCentreBias = 1.0f;

float gLGPStressGlassMeltEffectSpeedScale = kLGPStressGlassMeltEffectSpeedScale;
float gLGPStressGlassMeltEffectOutputGain = kLGPStressGlassMeltEffectOutputGain;
float gLGPStressGlassMeltEffectCentreBias = kLGPStressGlassMeltEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPStressGlassMeltEffectParameters[] = {
    {"lgpstress_glass_melt_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPStressGlassMeltEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpstress_glass_melt_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPStressGlassMeltEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpstress_glass_melt_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPStressGlassMeltEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPStressGlassMeltEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPStressGlassMeltEffect::LGPStressGlassMeltEffect()
    : m_analyser(0.0f)
{
}

bool LGPStressGlassMeltEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPStressGlassMeltEffect
    gLGPStressGlassMeltEffectSpeedScale = kLGPStressGlassMeltEffectSpeedScale;
    gLGPStressGlassMeltEffectOutputGain = kLGPStressGlassMeltEffectOutputGain;
    gLGPStressGlassMeltEffectCentreBias = kLGPStressGlassMeltEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPStressGlassMeltEffect

    m_analyser = 0.0f;
    return true;
}

void LGPStressGlassMeltEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN STRESS GLASS (MELT) - Phase-locked wings
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
            // Lock wings together near the melt zone: subtle shift at edges only.
            const float shiftF = (1.0f - stress);
            const uint8_t hueShift = (uint8_t)(shiftF * 10.0f);
            const uint8_t hueB = (uint8_t)(hueA + hueShift);

            // Slight dim on B to avoid perceived dominance ping-pong.
            const uint8_t brB = scale8_video(br, 245);
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPStressGlassMeltEffect
uint8_t LGPStressGlassMeltEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPStressGlassMeltEffectParameters) / sizeof(kLGPStressGlassMeltEffectParameters[0]));
}

const plugins::EffectParameter* LGPStressGlassMeltEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPStressGlassMeltEffectParameters[index];
}

bool LGPStressGlassMeltEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpstress_glass_melt_effect_speed_scale") == 0) {
        gLGPStressGlassMeltEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpstress_glass_melt_effect_output_gain") == 0) {
        gLGPStressGlassMeltEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpstress_glass_melt_effect_centre_bias") == 0) {
        gLGPStressGlassMeltEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPStressGlassMeltEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpstress_glass_melt_effect_speed_scale") == 0) return gLGPStressGlassMeltEffectSpeedScale;
    if (strcmp(name, "lgpstress_glass_melt_effect_output_gain") == 0) return gLGPStressGlassMeltEffectOutputGain;
    if (strcmp(name, "lgpstress_glass_melt_effect_centre_bias") == 0) return gLGPStressGlassMeltEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPStressGlassMeltEffect

void LGPStressGlassMeltEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStressGlassMeltEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Stress Glass (Melt)",
        "Photoelastic fringes with phase-locked wings",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
