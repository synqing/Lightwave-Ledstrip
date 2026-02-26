/**
 * @file LGPSchlierenFlowEffect.cpp
 * @brief LGP Schlieren Flow effect implementation
 */

#include "LGPSchlierenFlowEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPSchlierenFlowEffect
namespace {
constexpr float kLGPSchlierenFlowEffectSpeedScale = 1.0f;
constexpr float kLGPSchlierenFlowEffectOutputGain = 1.0f;
constexpr float kLGPSchlierenFlowEffectCentreBias = 1.0f;

float gLGPSchlierenFlowEffectSpeedScale = kLGPSchlierenFlowEffectSpeedScale;
float gLGPSchlierenFlowEffectOutputGain = kLGPSchlierenFlowEffectOutputGain;
float gLGPSchlierenFlowEffectCentreBias = kLGPSchlierenFlowEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPSchlierenFlowEffectParameters[] = {
    {"lgpschlieren_flow_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPSchlierenFlowEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpschlieren_flow_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPSchlierenFlowEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpschlieren_flow_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPSchlierenFlowEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPSchlierenFlowEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPSchlierenFlowEffect::LGPSchlierenFlowEffect()
    : m_t(0.0f)
{
}

bool LGPSchlierenFlowEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPSchlierenFlowEffect
    gLGPSchlierenFlowEffectSpeedScale = kLGPSchlierenFlowEffectSpeedScale;
    gLGPSchlierenFlowEffectOutputGain = kLGPSchlierenFlowEffectOutputGain;
    gLGPSchlierenFlowEffectCentreBias = kLGPSchlierenFlowEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPSchlierenFlowEffect

    m_t = 0.0f;
    return true;
}

void LGPSchlierenFlowEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN SCHLIEREN - Knife-edge gradient flow
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_t += 0.012f + 0.070f * speedNorm;

    const float f1 = 0.060f;
    const float f2 = 0.145f;
    const float f3 = 0.310f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = (float)i;
        const float dist = (float)centerPairDistance((uint16_t)i);

        float rho =
            sinf(x * f1 + m_t)
            + 0.7f * sinf(x * f2 - m_t * 1.2f)
            + 0.3f * sinf(x * f3 + m_t * 2.1f);

        float grad =
            f1 * cosf(x * f1 + m_t)
            + 0.7f * f2 * cosf(x * f2 - m_t * 1.2f)
            + 0.3f * f3 * cosf(x * f3 + m_t * 2.1f);

        float edge = 0.5f + 0.5f * tanhf(grad * 6.0f);

        const float mid = (STRIP_LENGTH - 1) * 0.5f;
        const float dmid = (x - mid);
        const float melt = expf(-(dmid * dmid) * 0.0028f);

        float wave = clamp01(0.65f * (edge * melt + 0.25f * melt) + 0.35f * (0.5f + 0.5f * sinf(rho)));

        const float base = 0.08f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t brA = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(dist * 0.7f) + (int)(edge * 40.0f));

        const uint8_t hueB = (uint8_t)(hueA + 5u);
        const uint8_t brB = scale8_video(brA, 245);

        ctx.leds[i] = ctx.palette.getColor(hueA, brA);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPSchlierenFlowEffect
uint8_t LGPSchlierenFlowEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPSchlierenFlowEffectParameters) / sizeof(kLGPSchlierenFlowEffectParameters[0]));
}

const plugins::EffectParameter* LGPSchlierenFlowEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPSchlierenFlowEffectParameters[index];
}

bool LGPSchlierenFlowEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpschlieren_flow_effect_speed_scale") == 0) {
        gLGPSchlierenFlowEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpschlieren_flow_effect_output_gain") == 0) {
        gLGPSchlierenFlowEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpschlieren_flow_effect_centre_bias") == 0) {
        gLGPSchlierenFlowEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPSchlierenFlowEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpschlieren_flow_effect_speed_scale") == 0) return gLGPSchlierenFlowEffectSpeedScale;
    if (strcmp(name, "lgpschlieren_flow_effect_output_gain") == 0) return gLGPSchlierenFlowEffectOutputGain;
    if (strcmp(name, "lgpschlieren_flow_effect_centre_bias") == 0) return gLGPSchlierenFlowEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPSchlierenFlowEffect

void LGPSchlierenFlowEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSchlierenFlowEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Schlieren Flow",
        "Knife-edge gradient flow",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
