/**
 * @file LGPBassBreathEffect.cpp
 * @brief Organic breathing effect implementation
 */

#include "LGPBassBreathEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPBassBreathEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Initialize smoothing followers
    m_bassFollower.reset(0.0f);
    m_midFollower.reset(0.0f);
    m_trebleFollower.reset(0.0f);
    m_breathFollower.reset(0.0f);
    m_lastHopSeq = 0;
    m_targetBass = 0.0f;
    m_targetMid = 0.0f;
    m_targetTreble = 0.0f;
    
    m_breathLevel = 0.0f;
    m_hueShift = 0.0f;
    return true;
}

void LGPBassBreathEffect::render(plugins::EffectContext& ctx) {
    float bass, mid, treble;
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();

    if (ctx.audio.available) {
        // Hop-based updates: update targets only on new hops
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            m_targetBass = ctx.audio.bass();
            m_targetMid = ctx.audio.mid();
            m_targetTreble = ctx.audio.treble();
        }

        // Smooth toward targets every frame with MOOD-adjusted smoothing
        bass = m_bassFollower.updateWithMood(m_targetBass, dt, moodNorm);
        mid = m_midFollower.updateWithMood(m_targetMid, dt, moodNorm);
        treble = m_trebleFollower.updateWithMood(m_targetTreble, dt, moodNorm);

        // Time-based fallback when bass is low - ensures visibility without audio content
        float timeBreath = 0.4f + 0.2f * sinf((float)(ctx.totalTimeMs % 4000) / 4000.0f * 6.283f);
        float audioDrive = bass + mid * 0.5f;
        if (audioDrive < 0.25f) {
            // Blend in time-based breathing when audio is quiet
            float blendFactor = 1.0f - (audioDrive / 0.25f);
            bass = bass + timeBreath * blendFactor;
        }
    } else {
        // Fallback: slow sine wave breathing
        float phase = (float)(ctx.totalTimeMs % 3000) / 3000.0f;
        bass = 0.5f + 0.3f * sinf(phase * 6.283f);
        mid = 0.3f;
        treble = 0.2f;
    }

    // Breath dynamics: use AsymmetricFollower for natural attack/release
    float targetBreath = bass * 0.8f + mid * 0.2f;
    m_breathLevel = m_breathFollower.updateWithMood(targetBreath, dt, moodNorm);
    // Ensure minimum breath level for visibility
    m_breathLevel = fmaxf(0.25f, m_breathLevel);

    // Hue shifts with treble activity
    m_hueShift += treble * ctx.deltaTimeMs * 0.1f;
    if (m_hueShift > 255.0f) m_hueShift -= 255.0f;

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Render CENTER PAIR breathing
    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / HALF_LENGTH;

        // Breath expands from center
        float breathRadius = m_breathLevel;
        float brightness;

        if (normalizedDist < breathRadius) {
            // Inside breath: bright, fading toward edge of breath
            brightness = 0.25f + 0.75f * (1.0f - normalizedDist / breathRadius);
        } else {
            // Outside breath: dim ambient
            brightness = 0.03f;
        }

        // Apply master brightness and breath level
        uint8_t bright = (uint8_t)(brightness * m_breathLevel * ctx.brightness);

        // Color: base hue + accumulated shift + distance variation
        uint8_t hue = ctx.gHue + (uint8_t)m_hueShift + (uint8_t)(normalizedDist * 32);
        CRGB color = ctx.palette.getColor(hue, bright);

        SET_CENTER_PAIR(ctx, dist, color);
    }
}

void LGPBassBreathEffect::cleanup() {}

const plugins::EffectMetadata& LGPBassBreathEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Bass Breath",
        "Organic breathing driven by bass",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
