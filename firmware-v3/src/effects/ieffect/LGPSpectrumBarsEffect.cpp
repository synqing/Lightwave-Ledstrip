/**
 * @file LGPSpectrumBarsEffect.cpp
 * @brief 8-band spectrum analyzer implementation
 */

#include "LGPSpectrumBarsEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static inline const float* selectChroma12(const plugins::AudioContext& audio) {
    return audio.chroma();
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
    m_chromaAngle = 0.0f;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SpectrumBarsPsram*>(
            heap_caps_malloc(sizeof(SpectrumBarsPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(SpectrumBarsPsram));
#endif
    return true;
}

void LGPSpectrumBarsEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();

    // Update smoothed bands (fast attack, slow decay)
    for (int i = 0; i < 8; ++i) {
        float target;
        if (ctx.audio.available) {
            // silentScale handled globally by RendererActor
            target = ctx.audio.getBand(i);
        } else {
            // Fallback: gentle sine wave animation when no audio
            float phase = ctx.totalTimeMs * 0.001f + i * 0.4f;
            target = 0.3f + 0.2f * sinf(phase);
        }
        if (target > m_smoothedBands[i]) {
            m_smoothedBands[i] = target;  // Instant attack
        } else {
            m_smoothedBands[i] *= powf(0.92f, dt * 60.0f);  // Slow decay (~200ms, dt-corrected)
        }
    }

    // Audio-reactive trail decay: loud = faster fade (punchy), quiet = slower (ambient)
#ifndef NATIVE_BUILD
    if (m_ps) {
        float rmsNow = ctx.audio.available ? ctx.audio.rms() : 0.0f;
        static constexpr float kDecayBase  = 1.5f;  // decays/sec at silence
        static constexpr float kDecayScale = 7.0f;  // extra decays/sec per unit RMS
        float decayRate = kDecayBase + kDecayScale * rmsNow;
        uint8_t trailFade = static_cast<uint8_t>(decayRate * dt * 255.0f);
        if (trailFade < 1) trailFade = 1;
        if (trailFade > 200) trailFade = 200;
        fadeToBlackBy(m_ps->trailBuffer, 160, trailFade);
    }
#endif

    // Musically anchored hue (non-rainbow): circular chroma mean, smoothed.
    uint8_t baseHue = ctx.gHue;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        const float* chroma = selectChroma12(ctx.audio);
        baseHue = effects::chroma::circularChromaHueSmoothed(
            chroma, m_chromaAngle, dt, 0.20f);
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

#ifndef NATIVE_BUILD
    if (m_ps) {
        // Blend current frame into trail buffer (audio-coupled):
        // loud → high blend amount → frame dominates → short trail
        // quiet → low blend amount → trail dominates → long ambient trail
        float rmsNow2 = ctx.audio.available ? ctx.audio.rms() : 0.0f;
        uint8_t blendAmt = static_cast<uint8_t>(30.0f + 200.0f * rmsNow2);
        uint16_t stripLen = ctx.ledCount < 160 ? ctx.ledCount : 160;
        for (uint16_t i = 0; i < stripLen; ++i) {
            m_ps->trailBuffer[i] = blend(m_ps->trailBuffer[i], ctx.leds[i], blendAmt);
        }
        // Output trail to strip 1 and mirror to strip 2
        for (uint16_t i = 0; i < stripLen; ++i) {
            ctx.leds[i] = m_ps->trailBuffer[i];
        }
        for (uint16_t i = 0; i < stripLen && (160u + i) < ctx.ledCount; ++i) {
            ctx.leds[160u + i] = m_ps->trailBuffer[i];
        }
    }
#endif
}

void LGPSpectrumBarsEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

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
