/**
 * @file SinelonEffect.cpp
 * @brief Sinelon effect implementation
 */

#include "SinelonEffect.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include <FastLED.h>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:SinelonEffect
namespace {
constexpr float kSinelonEffectSpeedScale = 1.0f;
constexpr float kSinelonEffectOutputGain = 1.0f;
constexpr float kSinelonEffectCentreBias = 1.0f;

float gSinelonEffectSpeedScale = kSinelonEffectSpeedScale;
float gSinelonEffectOutputGain = kSinelonEffectOutputGain;
float gSinelonEffectCentreBias = kSinelonEffectCentreBias;

const lightwaveos::plugins::EffectParameter kSinelonEffectParameters[] = {
    {"sinelon_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kSinelonEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"sinelon_effect_output_gain", "Output Gain", 0.25f, 2.0f, kSinelonEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"sinelon_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kSinelonEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:SinelonEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {
SinelonEffect::SinelonEffect()
{
}

bool SinelonEffect::init(plugins::EffectContext& ctx) {
    // AUTO_TUNABLES_BULK_RESET_BEGIN:SinelonEffect
    gSinelonEffectSpeedScale = kSinelonEffectSpeedScale;
    gSinelonEffectOutputGain = kSinelonEffectOutputGain;
    gSinelonEffectCentreBias = kSinelonEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:SinelonEffect


    return true;
}

void SinelonEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN SINELON - Oscillates outward from center
    using namespace utils;
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Oscillate from center outward using utility function
    int distFromCenter = fastled_beatsin16(13, 0, HALF_LENGTH);

    // Set both sides of center
    int pos1 = CENTER_RIGHT + distFromCenter;  // Right side
    int pos2 = CENTER_LEFT - distFromCenter;   // Left side

    CRGB color1 = ctx.palette.getColor(ctx.gHue, 192);
    CRGB color2 = ctx.palette.getColor(ctx.gHue + 128, 192);

    if (pos1 < STRIP_LENGTH) {
        ctx.leds[pos1] = color1;
        if (pos1 + STRIP_LENGTH < ctx.ledCount) {
            int mirrorPos1 = pos1 + STRIP_LENGTH;
            ctx.leds[mirrorPos1] = color1;
        }
    }
    if (pos2 >= 0) {
        ctx.leds[pos2] = color2;
        if (pos2 + STRIP_LENGTH < ctx.ledCount) {
            int mirrorPos2 = pos2 + STRIP_LENGTH;
            ctx.leds[mirrorPos2] = color2;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:SinelonEffect
uint8_t SinelonEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kSinelonEffectParameters) / sizeof(kSinelonEffectParameters[0]));
}

const plugins::EffectParameter* SinelonEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kSinelonEffectParameters[index];
}

bool SinelonEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "sinelon_effect_speed_scale") == 0) {
        gSinelonEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "sinelon_effect_output_gain") == 0) {
        gSinelonEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "sinelon_effect_centre_bias") == 0) {
        gSinelonEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float SinelonEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "sinelon_effect_speed_scale") == 0) return gSinelonEffectSpeedScale;
    if (strcmp(name, "sinelon_effect_output_gain") == 0) return gSinelonEffectOutputGain;
    if (strcmp(name, "sinelon_effect_centre_bias") == 0) return gSinelonEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:SinelonEffect

void SinelonEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& SinelonEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Sinelon",
        "Bouncing particle with palette trails",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

