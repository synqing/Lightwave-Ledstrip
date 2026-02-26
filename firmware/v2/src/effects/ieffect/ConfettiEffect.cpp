/**
 * @file ConfettiEffect.cpp
 * @brief Confetti effect implementation
 */

#include "ConfettiEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:ConfettiEffect
namespace {
constexpr float kConfettiEffectSpeedScale = 1.0f;
constexpr float kConfettiEffectOutputGain = 1.0f;
constexpr float kConfettiEffectCentreBias = 1.0f;

float gConfettiEffectSpeedScale = kConfettiEffectSpeedScale;
float gConfettiEffectOutputGain = kConfettiEffectOutputGain;
float gConfettiEffectCentreBias = kConfettiEffectCentreBias;

const lightwaveos::plugins::EffectParameter kConfettiEffectParameters[] = {
    {"confetti_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kConfettiEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"confetti_effect_output_gain", "Output Gain", 0.25f, 2.0f, kConfettiEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"confetti_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kConfettiEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:ConfettiEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {
ConfettiEffect::ConfettiEffect()
{
}

bool ConfettiEffect::init(plugins::EffectContext& ctx) {
    // AUTO_TUNABLES_BULK_RESET_BEGIN:ConfettiEffect
    gConfettiEffectSpeedScale = kConfettiEffectSpeedScale;
    gConfettiEffectOutputGain = kConfettiEffectOutputGain;
    gConfettiEffectCentreBias = kConfettiEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:ConfettiEffect


    // Clear buffer for stateful effect
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));
    return true;
}

void ConfettiEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN CONFETTI - Sparks spawn at center 79/80 and fade outward
    //
    // BUFFER-FEEDBACK EFFECT: This effect reads from ctx.leds[i+1] and ctx.leds[i-1]
    // to propagate confetti particles outward. This relies on the previous frame's
    // LED buffer state, making it a stateful effect. Identified in PatternRegistry::isStatefulEffect().

    // Fade all LEDs
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Spawn new confetti at CENTER PAIR
    if (random8() < 80) {
        int centerPos = CENTER_LEFT + random8(2);  // 79 or 80
        CRGB color = ctx.palette.getColor(ctx.gHue + random8(64), 255);

        ctx.leds[centerPos] = color;
        if (centerPos + STRIP_LENGTH < ctx.ledCount) {
            int mirrorPos = centerPos + STRIP_LENGTH;
            ctx.leds[mirrorPos] = color;
        }
    }

    // Propagate confetti outward (buffer feedback)
    // Left side propagation
    for (int i = CENTER_LEFT - 1; i >= 0; i--) {
        if (ctx.leds[i + 1]) {
            ctx.leds[i] = ctx.leds[i + 1];
            ctx.leds[i].fadeToBlackBy(25);

            // Mirror to strip 2
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.leds[i];
            }
        }
    }

    // Right side propagation
    for (int i = CENTER_RIGHT + 1; i < STRIP_LENGTH; i++) {
        if (ctx.leds[i - 1]) {
            ctx.leds[i] = ctx.leds[i - 1];
            ctx.leds[i].fadeToBlackBy(25);

            // Mirror to strip 2
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.leds[i];
            }
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:ConfettiEffect
uint8_t ConfettiEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kConfettiEffectParameters) / sizeof(kConfettiEffectParameters[0]));
}

const plugins::EffectParameter* ConfettiEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kConfettiEffectParameters[index];
}

bool ConfettiEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "confetti_effect_speed_scale") == 0) {
        gConfettiEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "confetti_effect_output_gain") == 0) {
        gConfettiEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "confetti_effect_centre_bias") == 0) {
        gConfettiEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float ConfettiEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "confetti_effect_speed_scale") == 0) return gConfettiEffectSpeedScale;
    if (strcmp(name, "confetti_effect_output_gain") == 0) return gConfettiEffectOutputGain;
    if (strcmp(name, "confetti_effect_centre_bias") == 0) return gConfettiEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:ConfettiEffect

void ConfettiEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ConfettiEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Confetti",
        "Random colored speckles fading",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

