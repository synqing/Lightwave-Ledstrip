/**
 * @file RippleEsTunedEffect.cpp
 * @brief Ripple (ES tuned) effect implementation
 */

#include "RippleEsTunedEffect.h"
#include "ChromaUtils.h"

#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>


// AUTO_TUNABLES_BULK_BEGIN:RippleEsTunedEffect
namespace {
constexpr float kRippleEsTunedEffectSpeedScale = 1.0f;
constexpr float kRippleEsTunedEffectOutputGain = 1.0f;
constexpr float kRippleEsTunedEffectCentreBias = 1.0f;

float gRippleEsTunedEffectSpeedScale = kRippleEsTunedEffectSpeedScale;
float gRippleEsTunedEffectOutputGain = kRippleEsTunedEffectOutputGain;
float gRippleEsTunedEffectCentreBias = kRippleEsTunedEffectCentreBias;

const lightwaveos::plugins::EffectParameter kRippleEsTunedEffectParameters[] = {
    {"ripple_es_tuned_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kRippleEsTunedEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"ripple_es_tuned_effect_output_gain", "Output Gain", 0.25f, 2.0f, kRippleEsTunedEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"ripple_es_tuned_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kRippleEsTunedEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:RippleEsTunedEffect
#endif

namespace lightwaveos::effects::ieffect {

RippleEsTunedEffect::RippleEsTunedEffect()
    : m_ps(nullptr)
{
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i] = {};
    }
}

bool RippleEsTunedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:RippleEsTunedEffect
    gRippleEsTunedEffectSpeedScale = kRippleEsTunedEffectSpeedScale;
    gRippleEsTunedEffectOutputGain = kRippleEsTunedEffectOutputGain;
    gRippleEsTunedEffectCentreBias = kRippleEsTunedEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:RippleEsTunedEffect

    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i] = {};
    }
    m_lastHopSeq = 0;
    m_spawnCooldown = 0;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<RippleEsTunedPsram*>(
            heap_caps_malloc(sizeof(RippleEsTunedPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(RippleEsTunedPsram));
#endif
    m_subBass = 0.0f;
    m_treble = 0.0f;
    m_fluxEnv = 0.0f;
    m_chromaAngle = 0.0f;
    m_baseHue = 0;
    return true;
}

void RippleEsTunedEffect::spawnRipple(uint8_t hue, uint8_t intensity, float speed) {
    for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
        if (!m_ripples[r].active) {
            m_ripples[r].radius = 0.0f;
            m_ripples[r].speed = speed;
            m_ripples[r].hue = hue;
            m_ripples[r].intensity = intensity;
            m_ripples[r].active = true;
            return;
        }
    }
}

void RippleEsTunedEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    const float rawDtEarly = ctx.getSafeRawDeltaSeconds();
    const bool hasAudio = ctx.audio.available;
    const bool tempoOk = hasAudio && (ctx.audio.tempoConfidence() >= 0.30f);

    // Decay trails. Slightly louder music keeps more trail.
    uint8_t fade = 42;
    if (hasAudio) {
        float rms = ctx.audio.rms();
        if (rms < 0.0f) rms = 0.0f;
        if (rms > 1.0f) rms = 1.0f;
        float f = 52.0f - (18.0f * rms);
        if (f < 28.0f) f = 28.0f;
        if (f > 58.0f) f = 58.0f;
        fade = static_cast<uint8_t>(f);
    }
    fadeToBlackBy(m_ps->radial, HALF_LENGTH, fade);

    bool newHop = false;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // FFT energy summaries (prefer adaptive bins when present).
            // ES backend populates bins64 + bins64Adaptive; use adaptive for stability.
            float subBassSum = 0.0f;
            for (uint8_t i = 0; i < 6; i++) {
                subBassSum += ctx.audio.binAdaptive(i);
            }
            float subBass = subBassSum * (1.0f / 6.0f);
            if (subBass > 1.0f) subBass = 1.0f;
            if (subBass < 0.0f) subBass = 0.0f;
            // Fast attack / medium decay to feel punchy but stable.
            if (subBass > m_subBass) {
                m_subBass = subBass;
            } else {
                m_subBass = (m_subBass * 0.86f) + (subBass * 0.14f);
            }

            float trebleSum = 0.0f;
            for (uint8_t i = 48; i < 64; i++) {
                trebleSum += ctx.audio.binAdaptive(i);
            }
            float treble = trebleSum * (1.0f / 16.0f);
            if (treble > 1.0f) treble = 1.0f;
            if (treble < 0.0f) treble = 0.0f;
            m_treble = (m_treble * 0.80f) + (treble * 0.20f);

            float flux = ctx.audio.fastFlux();
            if (flux < 0.0f) flux = 0.0f;
            if (flux > 1.0f) flux = 1.0f;
            // Transient envelope: instant-ish attack, fast-ish decay.
            if (flux > m_fluxEnv) {
                m_fluxEnv = flux;
            } else {
                m_fluxEnv = effects::chroma::dtDecay(m_fluxEnv, 0.82f, rawDtEarly);
            }

        }

        // Circular chroma hue (prevents argmax discontinuities and wrapping artefacts).
        // Runs every frame for smooth tracking, not just on hops.
        float rawDt = ctx.getSafeRawDeltaSeconds();
        const float* chroma = ctx.audio.controlBus.chroma;
        m_baseHue = effects::chroma::circularChromaHueSmoothed(
            chroma, m_chromaAngle, rawDt, 0.20f);
    }
#endif

    if (m_spawnCooldown > 0) {
        m_spawnCooldown--;
    }

    // Unified speed scaling from slider: 0..50 → ~0.6..2.4
    float speedScale = 0.6f + (1.8f * (ctx.speed / 50.0f));
    if (speedScale < 0.25f) speedScale = 0.25f;

    // Spawn logic:
    // - Beat-locked spawns when tempo is reliable.
    // - Kick + snare can force spawns.
    // - Flux can add extra micro-spawns on sharp transients.
    if (hasAudio && m_spawnCooldown == 0) {
        const float beatStrength = tempoOk ? ctx.audio.beatStrength() : 0.0f;
        const bool beatTick = tempoOk && ctx.audio.isOnBeat();

        // Base intensity driven by sub-bass + flux. Beat strength boosts when tempo locked.
        float intensity01 = 0.25f + (0.55f * m_subBass) + (0.45f * m_fluxEnv);
        if (tempoOk) {
            intensity01 *= 0.75f + (0.60f * beatStrength);
        }
        if (intensity01 > 1.0f) intensity01 = 1.0f;
        if (intensity01 < 0.0f) intensity01 = 0.0f;
        uint8_t intensity = static_cast<uint8_t>(intensity01 * 255.0f);

        // Kick detection: lower threshold than legacy Ripple, tuned for ES adaptive bins.
        const bool kick = (m_subBass > 0.35f);
        const bool snare = ctx.audio.isSnareHit();

        // Beat spawns: predictable pulse when locked.
        if (beatTick && (beatStrength > 0.18f)) {
            float spd = speedScale * (0.85f + (0.40f * m_subBass));
            spawnRipple(static_cast<uint8_t>(m_baseHue + ctx.gHue), intensity, spd);
            m_spawnCooldown = 1;
        }

        // Force spawns for kick/snare (even if tempo is poor).
        if (kick) {
            float spd = speedScale * (1.00f + (0.50f * m_subBass));
            uint8_t hue = static_cast<uint8_t>(ctx.gHue + m_baseHue + (uint8_t)(m_subBass * 30.0f));
            spawnRipple(hue, 255, spd);
            m_spawnCooldown = 2;
        } else if (snare) {
            float spd = speedScale * 1.15f;
            uint8_t hue = static_cast<uint8_t>(m_baseHue + 64);
            spawnRipple(hue, 230, spd);
            m_spawnCooldown = 1;
        } else if (m_fluxEnv > 0.55f && (!tempoOk || !beatTick)) {
            // Flux accent spawns (only when not already beat-spawning) to avoid overload.
            float spd = speedScale * (0.75f + (0.35f * m_fluxEnv));
            spawnRipple(static_cast<uint8_t>(m_baseHue + ctx.gHue), (uint8_t)(180 + (m_fluxEnv * 60.0f)), spd);
            m_spawnCooldown = 1;
        }
    }

    // Update and render ripples into radial history buffer.
    const float beatStrengthNow = (tempoOk ? ctx.audio.beatStrength() : 0.0f);
    const float thickness = 2.0f + (4.0f * m_treble);  // treble = thicker, brighter edge

    for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
        if (!m_ripples[r].active) continue;

        // Growth rate responds to beat strength + sub-bass.
        float growth = m_ripples[r].speed * (0.85f + (0.35f * beatStrengthNow) + (0.25f * m_subBass));
        m_ripples[r].radius += growth;

        if (m_ripples[r].radius > HALF_LENGTH) {
            m_ripples[r].active = false;
            continue;
        }

        for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
            float wavePos = static_cast<float>(dist) - m_ripples[r].radius;
            float waveAbs = fabsf(wavePos);
            if (waveAbs < thickness) {
                float front01 = 1.0f - (waveAbs / thickness);
                if (front01 < 0.0f) front01 = 0.0f;
                if (front01 > 1.0f) front01 = 1.0f;

                uint8_t b = static_cast<uint8_t>(front01 * 255.0f);

                // Edge fade to keep the centre clean.
                uint8_t edgeFade = (uint8_t)((HALF_LENGTH - m_ripples[r].radius) * 255.0f / HALF_LENGTH);
                b = scale8(b, edgeFade);
                b = scale8(b, m_ripples[r].intensity);

                // Treble shimmer: add sparkle to the leading edge.
                if (m_treble > 0.08f) {
                    uint8_t shimmerBoost = (uint8_t)(m_treble * front01 * 70.0f);
                    b = qadd8(b, shimmerBoost);
                }

                CRGB color = ctx.palette.getColor((uint8_t)(m_ripples[r].hue + dist), b);
                // Pre-scale so multiple overlapping ripples stay in range (colour corruption fix)
                constexpr uint8_t RIPPLE_PRE_SCALE = 85;  // ~3 overlapping ripples sum to 255
                color = color.nscale8(RIPPLE_PRE_SCALE);
                m_ps->radial[dist].r = qadd8(m_ps->radial[dist].r, color.r);
                m_ps->radial[dist].g = qadd8(m_ps->radial[dist].g, color.g);
                m_ps->radial[dist].b = qadd8(m_ps->radial[dist].b, color.b);
            }
        }
    }

    memcpy(m_ps->radialAux, m_ps->radial, sizeof(m_ps->radial));

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        SET_CENTER_PAIR(ctx, dist, m_ps->radialAux[dist]);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:RippleEsTunedEffect
uint8_t RippleEsTunedEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kRippleEsTunedEffectParameters) / sizeof(kRippleEsTunedEffectParameters[0]));
}

const plugins::EffectParameter* RippleEsTunedEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kRippleEsTunedEffectParameters[index];
}

bool RippleEsTunedEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "ripple_es_tuned_effect_speed_scale") == 0) {
        gRippleEsTunedEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "ripple_es_tuned_effect_output_gain") == 0) {
        gRippleEsTunedEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "ripple_es_tuned_effect_centre_bias") == 0) {
        gRippleEsTunedEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float RippleEsTunedEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "ripple_es_tuned_effect_speed_scale") == 0) return gRippleEsTunedEffectSpeedScale;
    if (strcmp(name, "ripple_es_tuned_effect_output_gain") == 0) return gRippleEsTunedEffectOutputGain;
    if (strcmp(name, "ripple_es_tuned_effect_centre_bias") == 0) return gRippleEsTunedEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:RippleEsTunedEffect

void RippleEsTunedEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& RippleEsTunedEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ripple (ES tuned)",
        "Beat-locked ripples tuned for ES v1.1 audio backend",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect

