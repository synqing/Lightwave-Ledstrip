/**
 * @file LGPBassBreathEffect.cpp
 * @brief Organic breathing effect implementation
 */

#include "LGPBassBreathEffect.h"
#include "ChromaUtils.h"
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

static inline const float* selectChroma12(const plugins::AudioContext& audio) {
    return audio.chroma();
}

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

} // namespace

bool LGPBassBreathEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_breathEnvelope.reset(0.0f);
    m_chromaAngle = 0.0f;
    m_lastHopSeq = 0;
    m_lastBass = 0.0f;
    m_lastFastFlux = 0.0f;
    m_fluxKick = 0.0f;
    return true;
}

void LGPBassBreathEffect::render(plugins::EffectContext& ctx) {
    // Get dt early for all decay operations
    float dt = ctx.getSafeRawDeltaSeconds();

    float bass, mid, treble;
    float beatStrength = 0.0f;
    float beatPhase = 0.0f;
    // silentScale handled globally by RendererActor

    if (ctx.audio.available) {
        bass = ctx.audio.bass();
        mid = ctx.audio.mid();
        treble = ctx.audio.treble();
        beatStrength = ctx.audio.beatStrength();
        beatPhase = ctx.audio.beatPhase();
    } else {
        // Fallback: slow sine wave breathing
        float phase = (float)(ctx.totalTimeMs % 3000) / 3000.0f;
        bass = 0.5f + 0.3f * sinf(phase * 6.283f);
        mid = 0.3f;
        treble = 0.2f;
    }

    // Backend-agnostic transient accent: flux kick (helps ES backend where "snare" triggers may be neutral).
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        float flux = ctx.audio.fastFlux();
        float fluxDelta = flux - m_lastFastFlux;
        m_lastFastFlux = flux;

        // Attack on flux spikes, then decay quickly (keeps "breath" lively on transients).
        if (fluxDelta > 0.18f && flux > 0.20f) {
            m_fluxKick = 1.0f;
        } else {
            // Also allow slow following of sustained flux (but bounded).
            if (flux > m_fluxKick) m_fluxKick = flux;
            m_fluxKick *= powf(0.86f, dt * 60.0f);  // dt-corrected
        }
    } else {
        m_fluxKick *= powf(0.90f, dt * 60.0f);  // dt-corrected
    }
#else
    m_fluxKick *= powf(0.90f, dt * 60.0f);  // dt-corrected
#endif

    // Beat-shaped inhale (smooth, centre-origin): raised cosine pulse per beat.
    float beatInhale = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available && ctx.audio.tempoConfidence() > 0.35f) {
        beatInhale = 0.5f * (1.0f - cosf(beatPhase * 6.2831853f));
        beatInhale *= beatStrength;
    }
#endif

    // Breath dynamics via AsymmetricFollower: 50ms attack, 400ms release (organic breathing)
    float targetBreath = bass * 0.75f + mid * 0.15f + beatInhale * 0.35f + m_fluxKick * 0.20f;
    targetBreath = clamp01(targetBreath);
    float breathLevel = m_breathEnvelope.update(targetBreath, dt);

    // Musically anchored hue (non-rainbow): circular chroma mean, smoothed.
    uint8_t chromaHueOffset = 0;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        const float* chroma = selectChroma12(ctx.audio);
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            chroma, m_chromaAngle, dt, 0.35f);
    }
#endif

    // Fade previous frame for trail persistence (dynamic: loud = shorter trails)
    uint8_t fadeAmt = (uint8_t)(15 + 35 * (1.0f - breathLevel));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // Render CENTER PAIR breathing
    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / HALF_LENGTH;

        // Breath expands from center
        float breathRadius = (breathLevel < 0.02f) ? 0.02f : breathLevel;
        float brightness;

        if (normalizedDist < breathRadius) {
            // Inside breath: bright, fading toward edge of breath
            brightness = 0.25f + 0.75f * (1.0f - normalizedDist / breathRadius);
        } else {
            // Outside breath: dim ambient
            brightness = 0.03f;
        }

        // Apply master brightness and breath level
        uint8_t bright = (uint8_t)(brightness * breathLevel * ctx.brightness);

        // Colour: anchored to chroma (stable), with subtle treble lift (no cycling).
        uint8_t hue = (uint8_t)(chromaHueOffset + (uint8_t)(treble * 18.0f) + (uint8_t)(normalizedDist * 20.0f));
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
