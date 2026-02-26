/**
 * @file LGPGratingScanBreakupEffect.cpp
 * @brief LGP Grating Scan (Breakup) effect implementation
 */

#include "LGPGratingScanBreakupEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstdint>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPGratingScanBreakupEffect
namespace {
constexpr float kLGPGratingScanBreakupEffectSpeedScale = 1.0f;
constexpr float kLGPGratingScanBreakupEffectOutputGain = 1.0f;
constexpr float kLGPGratingScanBreakupEffectCentreBias = 1.0f;

float gLGPGratingScanBreakupEffectSpeedScale = kLGPGratingScanBreakupEffectSpeedScale;
float gLGPGratingScanBreakupEffectOutputGain = kLGPGratingScanBreakupEffectOutputGain;
float gLGPGratingScanBreakupEffectCentreBias = kLGPGratingScanBreakupEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPGratingScanBreakupEffectParameters[] = {
    {"lgpgrating_scan_breakup_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPGratingScanBreakupEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpgrating_scan_breakup_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPGratingScanBreakupEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpgrating_scan_breakup_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPGratingScanBreakupEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPGratingScanBreakupEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

LGPGratingScanBreakupEffect::LGPGratingScanBreakupEffect()
    : m_pos(0.0f)
{
}

bool LGPGratingScanBreakupEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPGratingScanBreakupEffect
    gLGPGratingScanBreakupEffectSpeedScale = kLGPGratingScanBreakupEffectSpeedScale;
    gLGPGratingScanBreakupEffectOutputGain = kLGPGratingScanBreakupEffectOutputGain;
    gLGPGratingScanBreakupEffectCentreBias = kLGPGratingScanBreakupEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPGratingScanBreakupEffect

    m_pos = 0.0f;
    return true;
}

void LGPGratingScanBreakupEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN GRATING SCAN (BREAKUP) - Halo spatter decay
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

        // Breakup mask grows with distance from the scan core (halo spatter only).
        const float breakupAmt = clamp01(dx * 0.08f);
        uint32_t h = 2166136261u;
        h ^= (uint32_t)i;
        h *= 16777619u;
        h ^= (uint32_t)(m_pos * 1000.0f);
        h *= 16777619u;
        const float noise = ((h & 1023u) / 1023.0f);
        const float breakup = (noise > breakupAmt) ? 1.0f : 0.0f;

        const float angle = (dist - m_pos) * 0.08f;
        const float spec = 0.5f + 0.5f * tanhf(angle);

        const uint8_t hueA = (uint8_t)(ctx.gHue + (int)(spec * 96.0f));
        const uint8_t hueB = (uint8_t)(ctx.gHue + (int)((1.0f - spec) * 96.0f));

        const float base = 0.06f;
        const float wave = clamp01(0.15f * (halo * breakup) + 0.85f * core);
        const float out = clamp01(base + (1.0f - base) * wave) * master;
        const uint8_t br = (uint8_t)(255.0f * out);

        ctx.leds[i] = ctx.palette.getColor(hueA, br);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, br);
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPGratingScanBreakupEffect
uint8_t LGPGratingScanBreakupEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPGratingScanBreakupEffectParameters) / sizeof(kLGPGratingScanBreakupEffectParameters[0]));
}

const plugins::EffectParameter* LGPGratingScanBreakupEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPGratingScanBreakupEffectParameters[index];
}

bool LGPGratingScanBreakupEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpgrating_scan_breakup_effect_speed_scale") == 0) {
        gLGPGratingScanBreakupEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpgrating_scan_breakup_effect_output_gain") == 0) {
        gLGPGratingScanBreakupEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpgrating_scan_breakup_effect_centre_bias") == 0) {
        gLGPGratingScanBreakupEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPGratingScanBreakupEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpgrating_scan_breakup_effect_speed_scale") == 0) return gLGPGratingScanBreakupEffectSpeedScale;
    if (strcmp(name, "lgpgrating_scan_breakup_effect_output_gain") == 0) return gLGPGratingScanBreakupEffectOutputGain;
    if (strcmp(name, "lgpgrating_scan_breakup_effect_centre_bias") == 0) return gLGPGratingScanBreakupEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPGratingScanBreakupEffect

void LGPGratingScanBreakupEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGratingScanBreakupEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Grating Scan (Breakup)",
        "Diffraction scan with halo breakup",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
