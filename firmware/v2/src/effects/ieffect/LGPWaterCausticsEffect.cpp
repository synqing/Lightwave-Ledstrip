/**
 * @file LGPWaterCausticsEffect.cpp
 * @brief LGP Water Caustics effect implementation
 */

#include "LGPWaterCausticsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPWaterCausticsEffect
namespace {
constexpr float kLGPWaterCausticsEffectSpeedScale = 1.0f;
constexpr float kLGPWaterCausticsEffectOutputGain = 1.0f;
constexpr float kLGPWaterCausticsEffectCentreBias = 1.0f;

float gLGPWaterCausticsEffectSpeedScale = kLGPWaterCausticsEffectSpeedScale;
float gLGPWaterCausticsEffectOutputGain = kLGPWaterCausticsEffectOutputGain;
float gLGPWaterCausticsEffectCentreBias = kLGPWaterCausticsEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPWaterCausticsEffectParameters[] = {
    {"lgpwater_caustics_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPWaterCausticsEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpwater_caustics_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPWaterCausticsEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpwater_caustics_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPWaterCausticsEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPWaterCausticsEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPWaterCausticsEffect::LGPWaterCausticsEffect()
    : m_t1(0.0f)
    , m_t2(0.0f)
{
}

bool LGPWaterCausticsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPWaterCausticsEffect
    gLGPWaterCausticsEffectSpeedScale = kLGPWaterCausticsEffectSpeedScale;
    gLGPWaterCausticsEffectOutputGain = kLGPWaterCausticsEffectOutputGain;
    gLGPWaterCausticsEffectCentreBias = kLGPWaterCausticsEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPWaterCausticsEffect

    m_t1 = 0.0f;
    m_t2 = 0.0f;
    return true;
}

void LGPWaterCausticsEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN WATER CAUSTICS - Ray-envelope cusp filaments
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_t1 += 0.010f + 0.060f * speedNorm;
    m_t2 += 0.006f + 0.040f * speedNorm;

    const float A = 0.75f;
    const float k1 = 0.18f;
    const float B = 0.35f;
    const float k2 = 0.41f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        const float y =
            dist
            + A * sinf(dist * k1 + m_t1)
            + B * sinf(dist * k2 - m_t2 * 1.3f);

        const float dydx =
            1.0f
            + A * k1 * cosf(dist * k1 + m_t1)
            + B * k2 * cosf(dist * k2 - m_t2 * 1.3f);

        float density = 1.0f / (0.20f + fabsf(dydx));
        density = clamp01(density * 0.85f);

        const float sheet = 0.5f + 0.5f * sinf(y * 0.22f + m_t2);
        float wave = clamp01(0.72f * density + 0.28f * sheet);

        const float base = 0.08f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t brA = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(y * 10.0f) + (int)(density * 120.0f));

        const uint8_t hueB = (uint8_t)(hueA + 6u);
        const uint8_t brB = scale8_video(brA, 245);

        ctx.leds[i] = ctx.palette.getColor(hueA, brA);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPWaterCausticsEffect
uint8_t LGPWaterCausticsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPWaterCausticsEffectParameters) / sizeof(kLGPWaterCausticsEffectParameters[0]));
}

const plugins::EffectParameter* LGPWaterCausticsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPWaterCausticsEffectParameters[index];
}

bool LGPWaterCausticsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpwater_caustics_effect_speed_scale") == 0) {
        gLGPWaterCausticsEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpwater_caustics_effect_output_gain") == 0) {
        gLGPWaterCausticsEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpwater_caustics_effect_centre_bias") == 0) {
        gLGPWaterCausticsEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPWaterCausticsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpwater_caustics_effect_speed_scale") == 0) return gLGPWaterCausticsEffectSpeedScale;
    if (strcmp(name, "lgpwater_caustics_effect_output_gain") == 0) return gLGPWaterCausticsEffectOutputGain;
    if (strcmp(name, "lgpwater_caustics_effect_centre_bias") == 0) return gLGPWaterCausticsEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPWaterCausticsEffect

void LGPWaterCausticsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPWaterCausticsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Water Caustics",
        "Ray-envelope caustic filaments",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
