/**
 * @file RippleEffect.cpp
 * @brief Ripple effect implementation
 */

#include "RippleEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

RippleEffect::RippleEffect()
{
    // Initialize all ripples as inactive
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0;
        m_ripples[i].speed = 0;
        m_ripples[i].hue = 0;
    }
}

bool RippleEffect::init(plugins::EffectContext& ctx) {
    // Reset all ripples
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0;
    }
    return true;
}

void RippleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN RIPPLE - Ripples expand from center
    //
    // STATEFUL EFFECT: This effect maintains ripple state (position, speed, hue) across frames
    // in the m_ripples[] array. Ripples spawn at center and expand outward. Identified
    // in PatternRegistry::isStatefulEffect().
    
    fadeToBlackBy(ctx.leds, ctx.ledCount, 20);

    // Spawn new ripples at center
    if (random8() < 30) {
        for (int i = 0; i < MAX_RIPPLES; i++) {
            if (!m_ripples[i].active) {
                m_ripples[i].radius = 0;
                m_ripples[i].speed = 0.5f + (random8() / 255.0f) * 2.0f;
                m_ripples[i].hue = random8();
                m_ripples[i].active = true;
                break;
            }
        }
    }

    // Update and render ripples
    for (int r = 0; r < MAX_RIPPLES; r++) {
        if (!m_ripples[r].active) continue;

        m_ripples[r].radius += m_ripples[r].speed * (ctx.speed / 10.0f);

        if (m_ripples[r].radius > HALF_LENGTH) {
            m_ripples[r].active = false;
            continue;
        }

        // Draw ripple
        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = (float)centerPairDistance((uint16_t)i);
            float wavePos = distFromCenter - m_ripples[r].radius;

            if (abs(wavePos) < 3.0f) {
                uint8_t brightness = 255 - (uint8_t)(abs(wavePos) * 85);
                brightness = (brightness * (uint8_t)(HALF_LENGTH - m_ripples[r].radius)) / HALF_LENGTH;

                CRGB color = ctx.palette.getColor(
                    m_ripples[r].hue + (uint8_t)distFromCenter,
                    brightness);
                ctx.leds[i] += color;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] += color;
                }
            }
        }
    }
}

void RippleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& RippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ripple",
        "Expanding water ripples",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

