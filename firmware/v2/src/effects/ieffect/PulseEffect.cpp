/**
 * @file PulseEffect.cpp
 * @brief Pulse effect implementation
 */

#include "PulseEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:PulseEffect
namespace {
constexpr float kPulseEffectSpeedScale = 1.0f;
constexpr float kPulseEffectOutputGain = 1.0f;
constexpr float kPulseEffectCentreBias = 1.0f;

float gPulseEffectSpeedScale = kPulseEffectSpeedScale;
float gPulseEffectOutputGain = kPulseEffectOutputGain;
float gPulseEffectCentreBias = kPulseEffectCentreBias;

const lightwaveos::plugins::EffectParameter kPulseEffectParameters[] = {
    {"pulse_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kPulseEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"pulse_effect_output_gain", "Output Gain", 0.25f, 2.0f, kPulseEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"pulse_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kPulseEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:PulseEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {
bool  PulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:PulseEffect
    gPulseEffectSpeedScale = kPulseEffectSpeedScale;
    gPulseEffectOutputGain = kPulseEffectOutputGain;
    gPulseEffectCentreBias = kPulseEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:PulseEffect

    return true;
}

void PulseEffect::render(plugins::EffectContext& ctx) {
    // CENTER PAIR Pulse - Canonical pattern
    float phase = (ctx.frameNumber * ctx.speed / 60.0f);
    float pulsePos = fmodf(phase, (float)HALF_LENGTH);

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    for (int dist = 0; dist < HALF_LENGTH; dist++) {
        float delta = fabsf((float)dist - pulsePos);
        float intensity = (delta < 10.0f) ? (1.0f - delta / 10.0f) : 0.0f;

        if (intensity > 0.0f) {
            uint8_t brightness = (uint8_t)(intensity * 255.0f);
            uint8_t colorIndex = (uint8_t)(dist * 3);

            CRGB color = ctx.palette.getColor((uint8_t)(ctx.gHue + colorIndex), brightness);

            // Strip 1: center pair
            uint16_t left1 = CENTER_LEFT - dist;
            uint16_t right1 = CENTER_RIGHT + dist;

            if (left1 < STRIP_LENGTH) ctx.leds[left1] = color;
            if (right1 < STRIP_LENGTH) ctx.leds[right1] = color;

            // Strip 2: center pair
            uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
            uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;

            if (left2 < ctx.ledCount) ctx.leds[left2] = color;
            if (right2 < ctx.ledCount) ctx.leds[right2] = color;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:PulseEffect
uint8_t PulseEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kPulseEffectParameters) / sizeof(kPulseEffectParameters[0]));
}

const plugins::EffectParameter* PulseEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kPulseEffectParameters[index];
}

bool PulseEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "pulse_effect_speed_scale") == 0) {
        gPulseEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "pulse_effect_output_gain") == 0) {
        gPulseEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "pulse_effect_centre_bias") == 0) {
        gPulseEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float PulseEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "pulse_effect_speed_scale") == 0) return gPulseEffectSpeedScale;
    if (strcmp(name, "pulse_effect_output_gain") == 0) return gPulseEffectOutputGain;
    if (strcmp(name, "pulse_effect_centre_bias") == 0) return gPulseEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:PulseEffect

void PulseEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& PulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Pulse",
        "Sharp energy pulses",
        plugins::EffectCategory::SHOCKWAVE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
