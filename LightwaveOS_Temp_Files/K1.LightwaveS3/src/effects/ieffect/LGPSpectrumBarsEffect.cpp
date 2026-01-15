/**
 * @file LGPSpectrumBarsEffect.cpp
 * @brief 8-band spectrum analyzer implementation
 */

#include "LGPSpectrumBarsEffect.h"
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

bool LGPSpectrumBarsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Initialize smoothing followers
    for (int i = 0; i < 8; i++) {
        m_bandFollowers[i].reset(0.0f);
        m_targetBands[i] = 0.0f;
        m_smoothedBands[i] = 0.0f;
    }
    m_lastHopSeq = 0;
    return true;
}

void LGPSpectrumBarsEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();
    
    // Hop-based updates: update targets only on new hops
    bool newHop = false;
    if (ctx.audio.available) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            for (int i = 0; i < 8; ++i) {
                m_targetBands[i] = ctx.audio.getBand(i);
            }
        }
    } else {
        // Fallback: use default values
        for (int i = 0; i < 8; ++i) {
            m_targetBands[i] = 0.3f;
        }
    }
    
    // Smooth toward targets every frame with MOOD-adjusted smoothing
    for (int i = 0; i < 8; ++i) {
        m_smoothedBands[i] = m_bandFollowers[i].updateWithMood(m_targetBands[i], dt, moodNorm);
    }

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Render CENTER PAIR: bands map from center (bass) to edges (treble)
    // Each band gets 10 LEDs (80 / 8 = 10)
    constexpr int LEDS_PER_BAND = 10;

    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        // Which band does this LED belong to?
        uint8_t bandIdx = (uint8_t)(dist / LEDS_PER_BAND);
        if (bandIdx > 7) bandIdx = 7;

        float bandEnergy = m_smoothedBands[bandIdx];

        // VISIBILITY FIX: Add minimum bar height floor to prevent invisible bars
        bandEnergy = fmaxf(0.1f, bandEnergy);

        // Position within band (0-1)
        int posInBand = dist % LEDS_PER_BAND;
        float normalizedPos = (float)posInBand / LEDS_PER_BAND;

        // Bar visualization: bright if energy > position in band
        float brightness;
        if (normalizedPos < bandEnergy) {
            brightness = 0.6f + bandEnergy * 0.4f;
        } else {
            brightness = 0.03f;  // Dim background
        }

        uint8_t bright = (uint8_t)(brightness * ctx.brightness);

        // Color: each band gets different hue (spread across palette)
        uint8_t hue = ctx.gHue + bandIdx * 28;  // ~224 degrees spread
        CRGB color = ctx.palette.getColor(hue, bright);

        SET_CENTER_PAIR(ctx, dist, color);
    }
}

void LGPSpectrumBarsEffect::cleanup() {}

const plugins::EffectMetadata& LGPSpectrumBarsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Spectrum Bars",
        "8-band spectrum from center to edge",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
