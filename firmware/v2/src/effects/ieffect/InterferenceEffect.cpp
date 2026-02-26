/**
 * @file InterferenceEffect.cpp
 * @brief Interference effect implementation
 */

#include "InterferenceEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:InterferenceEffect
namespace {
constexpr float kInterferenceEffectSpeedScale = 1.0f;
constexpr float kInterferenceEffectOutputGain = 1.0f;
constexpr float kInterferenceEffectCentreBias = 1.0f;

float gInterferenceEffectSpeedScale = kInterferenceEffectSpeedScale;
float gInterferenceEffectOutputGain = kInterferenceEffectOutputGain;
float gInterferenceEffectCentreBias = kInterferenceEffectCentreBias;

const lightwaveos::plugins::EffectParameter kInterferenceEffectParameters[] = {
    {"interference_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kInterferenceEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"interference_effect_output_gain", "Output Gain", 0.25f, 2.0f, kInterferenceEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"interference_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kInterferenceEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:InterferenceEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {
InterferenceEffect::InterferenceEffect()
    : m_wave1Phase(0.0f)
    , m_wave2Phase(0.0f)
{
}

bool InterferenceEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:InterferenceEffect
    gInterferenceEffectSpeedScale = kInterferenceEffectSpeedScale;
    gInterferenceEffectOutputGain = kInterferenceEffectOutputGain;
    gInterferenceEffectCentreBias = kInterferenceEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:InterferenceEffect


    m_wave1Phase = 0.0f;
    m_wave2Phase = 0.0f;
    return true;
}

void InterferenceEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN INTERFERENCE - Two waves from center create patterns
    float dt = ctx.getSafeDeltaSeconds();
    m_wave1Phase += ctx.speed / 20.0f * 60.0f * dt;  // dt-corrected
    m_wave2Phase -= ctx.speed / 30.0f * 60.0f * dt;  // dt-corrected

    // Wrap phases to prevent unbounded growth (prevents hue cycling - no-rainbows rule)
    const float twoPi = 2.0f * PI;
    if (m_wave1Phase > twoPi) m_wave1Phase -= twoPi;
    if (m_wave1Phase < 0.0f) m_wave1Phase += twoPi;
    if (m_wave2Phase > twoPi) m_wave2Phase -= twoPi;
    if (m_wave2Phase < 0.0f) m_wave2Phase += twoPi;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Two waves emanating from center
        float wave1 = sinf(normalizedDist * PI * 4.0f + m_wave1Phase) * 127.0f + 128.0f;
        float wave2 = sinf(normalizedDist * PI * 6.0f + m_wave2Phase) * 127.0f + 128.0f;

        // Interference pattern
        uint8_t brightness = (uint8_t)((wave1 + wave2) / 2.0f);
        // Wrap hue calculation to prevent rainbow cycling (no-rainbows rule: < 60° range)
        uint8_t hue = (uint8_t)((m_wave1Phase * 20.0f) + (distFromCenter * 8.0f));

        CRGB color = ctx.palette.getColor((uint8_t)(ctx.gHue + hue), brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:InterferenceEffect
uint8_t InterferenceEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kInterferenceEffectParameters) / sizeof(kInterferenceEffectParameters[0]));
}

const plugins::EffectParameter* InterferenceEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kInterferenceEffectParameters[index];
}

bool InterferenceEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "interference_effect_speed_scale") == 0) {
        gInterferenceEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "interference_effect_output_gain") == 0) {
        gInterferenceEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "interference_effect_centre_bias") == 0) {
        gInterferenceEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float InterferenceEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "interference_effect_speed_scale") == 0) return gInterferenceEffectSpeedScale;
    if (strcmp(name, "interference_effect_output_gain") == 0) return gInterferenceEffectOutputGain;
    if (strcmp(name, "interference_effect_centre_bias") == 0) return gInterferenceEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:InterferenceEffect

void InterferenceEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& InterferenceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Interference",
        "Basic wave interference",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
