// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HeartbeatEffect.cpp
 * @brief Heartbeat effect implementation
 */

#include "HeartbeatEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <Arduino.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

HeartbeatEffect::HeartbeatEffect()
    : m_lastBeatTime(0)
    , m_beatState(0)
    , m_pulseRadius(0.0f)
{
}

bool HeartbeatEffect::init(plugins::EffectContext& ctx) {
    m_lastBeatTime = millis();
    m_beatState = 0;
    m_pulseRadius = 0.0f;
    return true;
}

void HeartbeatEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN HEARTBEAT - Pulses like a heart from center
    // Uses a "lub-dub" pattern: two quick beats then pause

    uint32_t now = millis();

    // Heartbeat timing (lub-dub pattern)
    // First beat, short pause, second beat, long pause
    const uint32_t BEAT1_DELAY = 0;
    const uint32_t BEAT2_DELAY = 200;   // 200ms after first beat
    const uint32_t CYCLE_TIME = 800;    // Full cycle ~75 BPM

    uint32_t cyclePos = (now - m_lastBeatTime);

    // Trigger beats
    if (cyclePos >= CYCLE_TIME) {
        m_lastBeatTime = now;
        m_beatState = 1;
        m_pulseRadius = 0;
    } else if (cyclePos >= BEAT2_DELAY && m_beatState == 1) {
        m_beatState = 2;
        m_pulseRadius = 0;
    }

    // Fade existing
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Expand pulse from center
    if (m_beatState > 0 && m_pulseRadius < HALF_LENGTH) {
        // Draw expanding ring
        for (int dist = 0; dist < HALF_LENGTH; dist++) {
            float delta = fabs(dist - m_pulseRadius);

            if (delta < 8) {
                float intensity = 1.0f - (delta / 8.0f);
                // Fade as it expands
                intensity *= 1.0f - (m_pulseRadius / HALF_LENGTH) * 0.7f;

                uint8_t brightness = (uint8_t)(255 * intensity);
                CRGB color = ctx.palette.getColor(
                    ctx.gHue + (uint8_t)(dist * 2),
                    brightness);

                // Strip 1: center pair
                uint16_t left1 = CENTER_LEFT - dist;
                uint16_t right1 = CENTER_RIGHT + dist;

                if (left1 < STRIP_LENGTH) {
                    ctx.leds[left1] = color;
                }
                if (right1 < STRIP_LENGTH) {
                    ctx.leds[right1] = color;
                }

                // Strip 2: center pair
                uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
                uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;

                if (left2 < ctx.ledCount) {
                    ctx.leds[left2] = color;
                }
                if (right2 < ctx.ledCount) {
                    ctx.leds[right2] = color;
                }
            }
        }

        // Expand pulse outward
        m_pulseRadius += ctx.speed / 8.0f;
    }
}

void HeartbeatEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& HeartbeatEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Heartbeat",
        "Rhythmic cardiac pulsing",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

