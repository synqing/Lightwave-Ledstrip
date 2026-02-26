/**
 * @file HeartbeatEffect.cpp
 * @brief Heartbeat effect implementation
 */

#include "HeartbeatEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <Arduino.h>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:HeartbeatEffect
namespace {
constexpr float kHeartbeatEffectSpeedScale = 1.0f;
constexpr float kHeartbeatEffectOutputGain = 1.0f;
constexpr float kHeartbeatEffectCentreBias = 1.0f;

float gHeartbeatEffectSpeedScale = kHeartbeatEffectSpeedScale;
float gHeartbeatEffectOutputGain = kHeartbeatEffectOutputGain;
float gHeartbeatEffectCentreBias = kHeartbeatEffectCentreBias;

const lightwaveos::plugins::EffectParameter kHeartbeatEffectParameters[] = {
    {"heartbeat_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kHeartbeatEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"heartbeat_effect_output_gain", "Output Gain", 0.25f, 2.0f, kHeartbeatEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"heartbeat_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kHeartbeatEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:HeartbeatEffect

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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:HeartbeatEffect
    gHeartbeatEffectSpeedScale = kHeartbeatEffectSpeedScale;
    gHeartbeatEffectOutputGain = kHeartbeatEffectOutputGain;
    gHeartbeatEffectCentreBias = kHeartbeatEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:HeartbeatEffect


    m_lastBeatTime = millis();
    m_beatState = 0;
    m_pulseRadius = 0.0f;
    return true;
}

void HeartbeatEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN HEARTBEAT - Pulses like a heart from center
    // Uses a "lub-dub" pattern: two quick beats then pause
    float dt = ctx.getSafeDeltaSeconds();

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

        // Expand pulse outward (dt-corrected)
        m_pulseRadius += ctx.speed / 8.0f * 60.0f * dt;
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:HeartbeatEffect
uint8_t HeartbeatEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kHeartbeatEffectParameters) / sizeof(kHeartbeatEffectParameters[0]));
}

const plugins::EffectParameter* HeartbeatEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kHeartbeatEffectParameters[index];
}

bool HeartbeatEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "heartbeat_effect_speed_scale") == 0) {
        gHeartbeatEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "heartbeat_effect_output_gain") == 0) {
        gHeartbeatEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "heartbeat_effect_centre_bias") == 0) {
        gHeartbeatEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float HeartbeatEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "heartbeat_effect_speed_scale") == 0) return gHeartbeatEffectSpeedScale;
    if (strcmp(name, "heartbeat_effect_output_gain") == 0) return gHeartbeatEffectOutputGain;
    if (strcmp(name, "heartbeat_effect_centre_bias") == 0) return gHeartbeatEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:HeartbeatEffect

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

