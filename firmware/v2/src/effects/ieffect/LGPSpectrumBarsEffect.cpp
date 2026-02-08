/**
 * @file LGPSpectrumBarsEffect.cpp
 * @brief 8-band spectrum analyzer implementation
 */

#include "LGPSpectrumBarsEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static inline const float* selectChroma12(const audio::ControlBusFrame& cb) {
    float esSum = 0.0f;
    float lwSum = 0.0f;
    for (uint8_t i = 0; i < audio::CONTROLBUS_NUM_CHROMA; ++i) {
        esSum += cb.es_chroma_raw[i];
        lwSum += cb.chroma[i];
    }
    return (esSum > (lwSum + 0.001f)) ? cb.es_chroma_raw : cb.chroma;
}

static inline uint8_t dominantChromaBin12(const float chroma[audio::CONTROLBUS_NUM_CHROMA]) {
    uint8_t best = 0;
    float bestV = chroma[0];
    for (uint8_t i = 1; i < audio::CONTROLBUS_NUM_CHROMA; ++i) {
        float v = chroma[i];
        if (v > bestV) {
            bestV = v;
            best = i;
        }
    }
    return best;
}

static inline uint8_t chromaBinToHue(uint8_t bin) {
    return (uint8_t)(bin * 21);
}

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

} // namespace

bool LGPSpectrumBarsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    memset(m_smoothedBands, 0, sizeof(m_smoothedBands));
    return true;
}

void LGPSpectrumBarsEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();

    // Update smoothed bands (fast attack, slow decay)
    for (int i = 0; i < 8; ++i) {
        float target = ctx.audio.available ? (ctx.audio.getBand(i) * ctx.audio.controlBus.silentScale) : 0.0f;
        if (target > m_smoothedBands[i]) {
            m_smoothedBands[i] = target;  // Instant attack
        } else {
            m_smoothedBands[i] *= powf(0.92f, dt * 60.0f);  // Slow decay (~200ms, dt-corrected)
        }
    }

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Musically anchored hue (non-rainbow): dominant chroma bin, stable per hop.
    uint8_t baseHue = ctx.gHue;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        const float* chroma = selectChroma12(ctx.audio.controlBus);
        baseHue = chromaBinToHue(dominantChromaBin12(chroma));
    }
#endif

    float beatAccent = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        beatAccent = 0.10f * ctx.audio.beatStrength();
    }
#endif

    // Render CENTER PAIR: bands map from center (bass) to edges (treble)
    // Each band gets 10 LEDs (80 / 8 = 10)
    constexpr int LEDS_PER_BAND = 10;

    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        // Which band does this LED belong to?
        uint8_t bandIdx = (uint8_t)(dist / LEDS_PER_BAND);
        if (bandIdx > 7) bandIdx = 7;

        // Mild perceptual lift makes quieter content visible without blowing peaks.
        float bandEnergy = powf(clamp01(m_smoothedBands[bandIdx]), 0.65f);

        // Position within band (0-1)
        int posInBand = dist % LEDS_PER_BAND;
        float normalizedPos = (float)posInBand / LEDS_PER_BAND;

        // Bar visualization: bright if energy > position in band
        float brightness;
        if (normalizedPos < bandEnergy) {
            brightness = 0.55f + bandEnergy * 0.45f + beatAccent;
        } else {
            brightness = 0.02f + 0.05f * beatAccent;  // Dim background with subtle beat lift
        }

        uint8_t bright = (uint8_t)(brightness * ctx.brightness);

        // Color: each band gets different hue (spread across palette)
        // Fixed offsets per band (no time-based hue sweep).
        uint8_t hue = (uint8_t)(baseHue + bandIdx * 10);
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
