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


// AUTO_TUNABLES_BULK_BEGIN:LGPBassBreathEffect
namespace {
constexpr float kLGPBassBreathEffectSpeedScale = 1.0f;
constexpr float kLGPBassBreathEffectOutputGain = 1.0f;
constexpr float kLGPBassBreathEffectCentreBias = 1.0f;

float gLGPBassBreathEffectSpeedScale = kLGPBassBreathEffectSpeedScale;
float gLGPBassBreathEffectOutputGain = kLGPBassBreathEffectOutputGain;
float gLGPBassBreathEffectCentreBias = kLGPBassBreathEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPBassBreathEffectParameters[] = {
    {"lgpbass_breath_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPBassBreathEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpbass_breath_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPBassBreathEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpbass_breath_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPBassBreathEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPBassBreathEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static inline const float* selectChroma12(const audio::ControlBusFrame& cb) {
    // Both backends now produce normalised chroma via Stage A/B pipeline.
    return cb.chroma;
}

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

} // namespace

bool LGPBassBreathEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPBassBreathEffect
    gLGPBassBreathEffectSpeedScale = kLGPBassBreathEffectSpeedScale;
    gLGPBassBreathEffectOutputGain = kLGPBassBreathEffectOutputGain;
    gLGPBassBreathEffectCentreBias = kLGPBassBreathEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPBassBreathEffect

    m_breathLevel = 0.0f;
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

    // Breath dynamics: fast attack, slow decay (bass-led, with optional beat inhale).
    float targetBreath = bass * 0.75f + mid * 0.15f + beatInhale * 0.35f + m_fluxKick * 0.20f;
    targetBreath = clamp01(targetBreath);
    if (targetBreath > m_breathLevel) {
        m_breathLevel = targetBreath;  // Instant attack
    } else {
        m_breathLevel *= powf(0.97f, dt * 60.0f);  // Slow exhale (dt-corrected)
    }

    // Musically anchored hue (non-rainbow): circular chroma mean, smoothed.
    uint8_t chromaHueOffset = 0;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        const float* chroma = selectChroma12(ctx.audio.controlBus);
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            chroma, m_chromaAngle, dt, 0.35f);
    }
#endif

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Render CENTER PAIR breathing
    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / HALF_LENGTH;

        // Breath expands from center
        float breathRadius = (m_breathLevel < 0.02f) ? 0.02f : m_breathLevel;
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

        // Colour: anchored to chroma (stable), with subtle treble lift (no cycling).
        uint8_t hue = (uint8_t)(chromaHueOffset + (uint8_t)(treble * 18.0f) + (uint8_t)(normalizedDist * 20.0f));
        CRGB color = ctx.palette.getColor(hue, bright);

        SET_CENTER_PAIR(ctx, dist, color);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPBassBreathEffect
uint8_t LGPBassBreathEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPBassBreathEffectParameters) / sizeof(kLGPBassBreathEffectParameters[0]));
}

const plugins::EffectParameter* LGPBassBreathEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPBassBreathEffectParameters[index];
}

bool LGPBassBreathEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpbass_breath_effect_speed_scale") == 0) {
        gLGPBassBreathEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpbass_breath_effect_output_gain") == 0) {
        gLGPBassBreathEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpbass_breath_effect_centre_bias") == 0) {
        gLGPBassBreathEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPBassBreathEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpbass_breath_effect_speed_scale") == 0) return gLGPBassBreathEffectSpeedScale;
    if (strcmp(name, "lgpbass_breath_effect_output_gain") == 0) return gLGPBassBreathEffectOutputGain;
    if (strcmp(name, "lgpbass_breath_effect_centre_bias") == 0) return gLGPBassBreathEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPBassBreathEffect

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
