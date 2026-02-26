/**
 * @file LGPParallaxDepthEffect.cpp
 * @brief LGP Parallax Depth effect implementation
 */

#include "LGPParallaxDepthEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPParallaxDepthEffect
namespace {
constexpr float kLGPParallaxDepthEffectSpeedScale = 1.0f;
constexpr float kLGPParallaxDepthEffectOutputGain = 1.0f;
constexpr float kLGPParallaxDepthEffectCentreBias = 1.0f;

float gLGPParallaxDepthEffectSpeedScale = kLGPParallaxDepthEffectSpeedScale;
float gLGPParallaxDepthEffectOutputGain = kLGPParallaxDepthEffectOutputGain;
float gLGPParallaxDepthEffectCentreBias = kLGPParallaxDepthEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPParallaxDepthEffectParameters[] = {
    {"lgpparallax_depth_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPParallaxDepthEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpparallax_depth_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPParallaxDepthEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpparallax_depth_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPParallaxDepthEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPParallaxDepthEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPParallaxDepthEffect::LGPParallaxDepthEffect()
    : m_time(0.0f)
{
}

bool LGPParallaxDepthEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPParallaxDepthEffect
    gLGPParallaxDepthEffectSpeedScale = kLGPParallaxDepthEffectSpeedScale;
    gLGPParallaxDepthEffectOutputGain = kLGPParallaxDepthEffectOutputGain;
    gLGPParallaxDepthEffectCentreBias = kLGPParallaxDepthEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPParallaxDepthEffect

    m_time = 0.0f;
    return true;
}

void LGPParallaxDepthEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN PARALLAX DEPTH - Two-layer refractive field
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_time += 0.010f + 0.060f * speedNorm;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        float a =
            sinf(dist * 0.060f + m_time)
            + 0.7f * sinf(dist * 0.160f + m_time * 1.3f)
            + 0.4f * sinf(dist * 0.360f - m_time * 1.9f);
        a = 0.5f + 0.5f * tanhf(a / 2.0f);

        const float distB = dist + 0.8f * sinf(m_time * 0.7f);
        float b =
            sinf(distB * 0.058f + m_time * 1.05f + 0.9f)
            + 0.7f * sinf(distB * 0.150f + m_time * 1.35f + 1.7f)
            + 0.4f * sinf(distB * 0.330f - m_time * 2.05f + 2.6f);
        b = 0.5f + 0.5f * tanhf(b / 2.0f);

        const float wave = clamp01(0.5f * (a + b));
        const float base = 0.10f;
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(dist * 0.4f) + (int)(a * 50.0f));
        const uint8_t hueB = (uint8_t)((int)ctx.gHue + 96 - (int)(dist * 0.4f) + (int)(b * 50.0f));

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPParallaxDepthEffect
uint8_t LGPParallaxDepthEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPParallaxDepthEffectParameters) / sizeof(kLGPParallaxDepthEffectParameters[0]));
}

const plugins::EffectParameter* LGPParallaxDepthEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPParallaxDepthEffectParameters[index];
}

bool LGPParallaxDepthEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpparallax_depth_effect_speed_scale") == 0) {
        gLGPParallaxDepthEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpparallax_depth_effect_output_gain") == 0) {
        gLGPParallaxDepthEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpparallax_depth_effect_centre_bias") == 0) {
        gLGPParallaxDepthEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPParallaxDepthEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpparallax_depth_effect_speed_scale") == 0) return gLGPParallaxDepthEffectSpeedScale;
    if (strcmp(name, "lgpparallax_depth_effect_output_gain") == 0) return gLGPParallaxDepthEffectOutputGain;
    if (strcmp(name, "lgpparallax_depth_effect_centre_bias") == 0) return gLGPParallaxDepthEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPParallaxDepthEffect

void LGPParallaxDepthEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPParallaxDepthEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Parallax Depth",
        "Two-layer chromatic parallax",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
