/**
 * @file LGPMoireSilkEffect.cpp
 * @brief LGP Moire Silk effect implementation
 */

#include "LGPMoireSilkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPMoireSilkEffect
namespace {
constexpr float kLGPMoireSilkEffectSpeedScale = 1.0f;
constexpr float kLGPMoireSilkEffectOutputGain = 1.0f;
constexpr float kLGPMoireSilkEffectCentreBias = 1.0f;

float gLGPMoireSilkEffectSpeedScale = kLGPMoireSilkEffectSpeedScale;
float gLGPMoireSilkEffectOutputGain = kLGPMoireSilkEffectOutputGain;
float gLGPMoireSilkEffectCentreBias = kLGPMoireSilkEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPMoireSilkEffectParameters[] = {
    {"lgpmoire_silk_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPMoireSilkEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpmoire_silk_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPMoireSilkEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpmoire_silk_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPMoireSilkEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPMoireSilkEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPMoireSilkEffect::LGPMoireSilkEffect()
    : m_phaseA(0.0f)
    , m_phaseB(0.0f)
{
}

bool LGPMoireSilkEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPMoireSilkEffect
    gLGPMoireSilkEffectSpeedScale = kLGPMoireSilkEffectSpeedScale;
    gLGPMoireSilkEffectOutputGain = kLGPMoireSilkEffectOutputGain;
    gLGPMoireSilkEffectCentreBias = kLGPMoireSilkEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPMoireSilkEffect

    m_phaseA = 0.0f;
    m_phaseB = 0.0f;
    return true;
}

void LGPMoireSilkEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN MOIRE SILK - Two-lattice beat envelope
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_phaseA += 0.012f + 0.050f * speedNorm;
    m_phaseB += 0.010f + 0.041f * speedNorm;

    const float f1 = 0.180f;
    const float f2 = 0.198f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        const float g1 = sinf(dist * f1 + m_phaseA);
        const float g2 = sinf(dist * f2 + m_phaseB);

        float field = g1 * g2;
        field = 0.5f + 0.5f * tanhf(field * 2.2f);

        const float rib = 0.5f + 0.5f * sinf(dist * 0.70f - m_phaseA * 1.7f);
        field = clamp01(0.78f * field + 0.22f * rib);

        const float base = 0.10f;
        const float out = clamp01(base + (1.0f - base) * field) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(field * 80.0f));
        const uint8_t hueB = (uint8_t)(ctx.gHue + (int)((1.0f - field) * 80.0f));

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPMoireSilkEffect
uint8_t LGPMoireSilkEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPMoireSilkEffectParameters) / sizeof(kLGPMoireSilkEffectParameters[0]));
}

const plugins::EffectParameter* LGPMoireSilkEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPMoireSilkEffectParameters[index];
}

bool LGPMoireSilkEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpmoire_silk_effect_speed_scale") == 0) {
        gLGPMoireSilkEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpmoire_silk_effect_output_gain") == 0) {
        gLGPMoireSilkEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpmoire_silk_effect_centre_bias") == 0) {
        gLGPMoireSilkEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPMoireSilkEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpmoire_silk_effect_speed_scale") == 0) return gLGPMoireSilkEffectSpeedScale;
    if (strcmp(name, "lgpmoire_silk_effect_output_gain") == 0) return gLGPMoireSilkEffectOutputGain;
    if (strcmp(name, "lgpmoire_silk_effect_centre_bias") == 0) return gLGPMoireSilkEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPMoireSilkEffect

void LGPMoireSilkEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPMoireSilkEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Moire Silk",
        "Two-lattice moire beat pattern",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
