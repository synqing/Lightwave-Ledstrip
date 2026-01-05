/**
 * @file PulseEffect.cpp
 * @brief Pulse effect implementation
 */

#include "PulseEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool PulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void PulseEffect::render(plugins::EffectContext& ctx) {
    // CENTER PAIR Pulse - Canonical pattern
    // Mood modulates speed: low mood (reactive) = faster, high mood (smooth) = slower
    float moodNorm = ctx.getMoodNormalized();
    float speedMultiplier = 1.0f + moodNorm * 0.5f;  // 1.0x to 1.5x range
    float phase = (ctx.frameNumber * ctx.speed * speedMultiplier / 60.0f);
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
