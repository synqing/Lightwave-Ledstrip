/**
 * @file LGPBeatPulseEffect.cpp
 * @brief Beat-synchronized radial pulse implementation
 */

#include "LGPBeatPulseEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPBeatPulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_pulsePosition = 0.0f;
    m_pulseIntensity = 0.0f;
    m_fallbackPhase = 0.0f;
    m_lastBeatPhase = 0.0f;
    return true;
}

void LGPBeatPulseEffect::render(plugins::EffectContext& ctx) {
    float beatPhase;
    float bassEnergy;
    bool onBeat = false;

    if (ctx.audio.available) {
        beatPhase = ctx.audio.beatPhase();
        bassEnergy = ctx.audio.bass();
        onBeat = ctx.audio.isOnBeat();
    } else {
        // Fallback: 120 BPM
        m_fallbackPhase += (ctx.deltaTimeMs / 500.0f);
        if (m_fallbackPhase >= 1.0f) m_fallbackPhase -= 1.0f;
        beatPhase = m_fallbackPhase;
        bassEnergy = 0.5f;

        // Detect beat crossing
        if (beatPhase < 0.05f && m_lastBeatPhase > 0.95f) {
            onBeat = true;
        }
    }
    m_lastBeatPhase = beatPhase;

    // Trigger new pulse on beat
    if (onBeat) {
        m_pulsePosition = 0.0f;
        m_pulseIntensity = 0.3f + bassEnergy * 0.7f;
    }

    // Expand pulse outward (~400ms to reach edge)
    m_pulsePosition += ctx.deltaTimeMs / 400.0f;
    if (m_pulsePosition > 1.0f) m_pulsePosition = 1.0f;

    // Decay intensity
    m_pulseIntensity *= 0.95f;
    if (m_pulseIntensity < 0.01f) m_pulseIntensity = 0.0f;

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Render CENTER PAIR outward
    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / HALF_LENGTH;

        // Pulse ring at current position
        float ringDist = fabsf(normalizedDist - m_pulsePosition);
        float ringWidth = 0.15f;
        float ringBright = 0.0f;

        if (ringDist < ringWidth) {
            ringBright = (1.0f - ringDist / ringWidth) * m_pulseIntensity;
        }

        // Background glow based on beat phase
        float bgBright = 0.08f + beatPhase * 0.12f;

        // Combine
        float totalBright = fmaxf(ringBright, bgBright);
        uint8_t brightness = (uint8_t)(totalBright * ctx.brightness);

        // Color from palette, hue shifts with beat phase
        uint8_t hue = ctx.gHue + (uint8_t)(beatPhase * 64);
        CRGB color = ctx.palette.getColor(hue, brightness);

        SET_CENTER_PAIR(ctx, dist, color);
    }
}

void LGPBeatPulseEffect::cleanup() {}

const plugins::EffectMetadata& LGPBeatPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse",
        "Radial pulse synchronized to beat",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
