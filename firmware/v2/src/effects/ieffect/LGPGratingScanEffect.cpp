/**
 * @file LGPGratingScanEffect.cpp
 * @brief LGP Grating Scan effect implementation
 */

#include "LGPGratingScanEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPGratingScanEffect
namespace {
constexpr float kLGPGratingScanEffectSpeedScale = 1.0f;
constexpr float kLGPGratingScanEffectOutputGain = 1.0f;
constexpr float kLGPGratingScanEffectCentreBias = 1.0f;

float gLGPGratingScanEffectSpeedScale = kLGPGratingScanEffectSpeedScale;
float gLGPGratingScanEffectOutputGain = kLGPGratingScanEffectOutputGain;
float gLGPGratingScanEffectCentreBias = kLGPGratingScanEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPGratingScanEffectParameters[] = {
    {"lgpgrating_scan_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPGratingScanEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpgrating_scan_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPGratingScanEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpgrating_scan_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPGratingScanEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPGratingScanEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPGratingScanEffect::LGPGratingScanEffect()
    : m_pos(0.0f)
{
}

bool LGPGratingScanEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPGratingScanEffect
    gLGPGratingScanEffectSpeedScale = kLGPGratingScanEffectSpeedScale;
    gLGPGratingScanEffectOutputGain = kLGPGratingScanEffectOutputGain;
    gLGPGratingScanEffectCentreBias = kLGPGratingScanEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPGratingScanEffect

    m_pos = 0.0f;
    return true;
}

void LGPGratingScanEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN GRATING SCAN - Spectral scan highlight
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_pos += (0.6f + 2.2f * speedNorm);
    if (m_pos > (float)STRIP_LENGTH) {
        m_pos -= (float)STRIP_LENGTH;
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);

        const float dx = fabsf(dist - m_pos);
        const float core = expf(-dx * dx * 0.020f);
        const float halo = expf(-dx * dx * 0.006f);

        const float angle = (dist - m_pos) * 0.08f;
        const float spec = 0.5f + 0.5f * tanhf(angle);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(spec * 96.0f));
        const uint8_t hueB = (uint8_t)(ctx.gHue + (int)((1.0f - spec) * 96.0f));

        const float base = 0.06f;
        const float wave = clamp01(0.2f * halo + 0.8f * core);
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPGratingScanEffect
uint8_t LGPGratingScanEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPGratingScanEffectParameters) / sizeof(kLGPGratingScanEffectParameters[0]));
}

const plugins::EffectParameter* LGPGratingScanEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPGratingScanEffectParameters[index];
}

bool LGPGratingScanEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpgrating_scan_effect_speed_scale") == 0) {
        gLGPGratingScanEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpgrating_scan_effect_output_gain") == 0) {
        gLGPGratingScanEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpgrating_scan_effect_centre_bias") == 0) {
        gLGPGratingScanEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPGratingScanEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpgrating_scan_effect_speed_scale") == 0) return gLGPGratingScanEffectSpeedScale;
    if (strcmp(name, "lgpgrating_scan_effect_output_gain") == 0) return gLGPGratingScanEffectOutputGain;
    if (strcmp(name, "lgpgrating_scan_effect_centre_bias") == 0) return gLGPGratingScanEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPGratingScanEffect

void LGPGratingScanEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGratingScanEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Grating Scan",
        "Diffraction scan highlight",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
