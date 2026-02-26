/**
 * @file WaveformParityEffect.cpp
 * @brief Sensory Bridge 3.1.0 waveform mode — true parity port
 *
 * Key differences from the failed SbWaveform310RefEffect:
 *  1) Intensity-only rendering — waveform amplitude is grayscale brightness,
 *     palette applied at output (no RGB corruption through accumulation).
 *  2) Dynamic normalisation — tracks per-zone max follower instead of
 *     fixed /128 divisor (wrong for int16_t range samples).
 *  3) dt-corrected smoothing — works at 120 FPS without becoming sluggish.
 *  4) Per-zone state — ZoneComposer can't corrupt cross-zone buffers.
 */

#include "WaveformParityEffect.h"
#include "AudioReactivePolicy.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include "../../utils/Log.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:WaveformParityEffect
namespace {
constexpr float kWaveformParityEffectSpeedScale = 1.0f;
constexpr float kWaveformParityEffectOutputGain = 1.0f;
constexpr float kWaveformParityEffectCentreBias = 1.0f;

float gWaveformParityEffectSpeedScale = kWaveformParityEffectSpeedScale;
float gWaveformParityEffectOutputGain = kWaveformParityEffectOutputGain;
float gWaveformParityEffectCentreBias = kWaveformParityEffectCentreBias;

const lightwaveos::plugins::EffectParameter kWaveformParityEffectParameters[] = {
    {"waveform_parity_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kWaveformParityEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"waveform_parity_effect_output_gain", "Output Gain", 0.25f, 2.0f, kWaveformParityEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"waveform_parity_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kWaveformParityEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:WaveformParityEffect

namespace lightwaveos::effects::ieffect {

// ============================================================================
// IEffect interface
// ============================================================================

bool WaveformParityEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:WaveformParityEffect
    gWaveformParityEffectSpeedScale = kWaveformParityEffectSpeedScale;
    gWaveformParityEffectOutputGain = kWaveformParityEffectOutputGain;
    gWaveformParityEffectCentreBias = kWaveformParityEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:WaveformParityEffect


    // Allocate large buffers in PSRAM (DRAM is too precious)
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("WaveformParityEffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }
    std::memset(m_ps, 0, sizeof(PsramData));

    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_lastHopSeq[z]    = 0;
        m_historyIndex[z]  = 0;
        m_historyPrimed[z] = false;
        m_maxFollower[z]   = 750.0f;   // SB minimum floor
        m_peakSmoothed[z]  = 0.0f;
    }
    return true;
}

void WaveformParityEffect::render(plugins::EffectContext& ctx) {
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!m_ps) return;

    if (!ctx.audio.available) {
        fadeToBlackBy(ctx.leds, ctx.ledCount, 32);
        return;
    }

    const uint8_t zone = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    const float dt = AudioReactivePolicy::signalDt(ctx);

    // ========================================================================
    // Step 1: Push waveform into 4-frame history ring (on new audio hop)
    // ========================================================================
    const bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq[zone]);
    if (newHop) {
        m_lastHopSeq[zone] = ctx.audio.controlBus.hop_seq;

        // Prefer SB parity waveform; fall back to contract waveform.
        const int16_t* wf = ctx.audio.controlBus.sb_waveform;
        if (ctx.audio.controlBus.sb_waveform_peak_scaled < 0.0001f) {
            wf = ctx.audio.controlBus.waveform;
        }

        if (!m_historyPrimed[zone]) {
            // Seed all history frames on first hop (avoid startup zeros).
            for (uint8_t f = 0; f < kHistoryFrames; ++f) {
                std::memcpy(m_ps->waveformHistory[zone][f], wf,
                            kWaveformPoints * sizeof(int16_t));
            }
            m_historyPrimed[zone] = true;
            m_historyIndex[zone]  = 0;
        } else {
            std::memcpy(m_ps->waveformHistory[zone][m_historyIndex[zone]], wf,
                        kWaveformPoints * sizeof(int16_t));
            m_historyIndex[zone] = (m_historyIndex[zone] + 1) % kHistoryFrames;
        }

        // ====================================================================
        // Dynamic max follower (replaces SB's fixed /128 divisor)
        // Matches SB i2s_audio.h: asymmetric attack 0.25, release 0.005
        // ====================================================================
        float frameMaxAbs = 0.0f;
        for (uint8_t i = 0; i < kWaveformPoints; ++i) {
            float a = fabsf(static_cast<float>(wf[i]));
            if (a > frameMaxAbs) frameMaxAbs = a;
        }

        if (frameMaxAbs > m_maxFollower[zone]) {
            m_maxFollower[zone] += (frameMaxAbs - m_maxFollower[zone]) * 0.25f;
        } else {
            m_maxFollower[zone] -= (m_maxFollower[zone] - frameMaxAbs) * 0.005f;
        }
        // Floor prevents division-by-near-zero in silence.
        if (m_maxFollower[zone] < 100.0f) m_maxFollower[zone] = 100.0f;
    }

    // ========================================================================
    // Step 2: Smooth peak follower (SB parity: 0.05/0.95, dt-corrected)
    // ========================================================================
    float peakScaled = ctx.audio.controlBus.sb_waveform_peak_scaled;
    if (peakScaled < 0.0001f) {
        // Fallback when SB sidecar isn't populated.
        peakScaled = clamp01(ctx.audio.rms() * 1.25f);
    }
    // dt-correct the 0.05/0.95 mix: alpha = 1 - pow(0.95, dt * 60)
    const float peakAlpha = 1.0f - powf(0.95f, dt * 60.0f);
    m_peakSmoothed[zone] += (peakScaled - m_peakSmoothed[zone]) * peakAlpha;

    // SB: peak = min(waveform_peak_scaled_last * 4.0, 1.0)
    float peak = m_peakSmoothed[zone] * 4.0f;
    if (peak > 1.0f) peak = 1.0f;
    if (peak < 0.0f) peak = 0.0f;

    // ========================================================================
    // Step 3: Per-LED waveform rendering (centre → edge)
    // ========================================================================
    // SB smoothing: (0.1 + MOOD * 0.9) * 0.05, dt-corrected.
    const float moodNorm = ctx.getMoodNormalized();
    const float smoothingRaw = (0.1f + moodNorm * 0.9f) * 0.05f;
    // Convert frame-based coefficient to dt-corrected:
    //   frame-based: new = old * (1-s) + input * s  (at ~48 FPS)
    //   dt-corrected: alpha = 1 - pow(1-s, dt * 48)
    const float smoothAlpha = 1.0f - powf(1.0f - smoothingRaw, dt * 48.0f);

    const float invMaxFollower = 1.0f / m_maxFollower[zone];

    // Palette parameters (BloomParity pattern)
    const float hueRotation = static_cast<float>(ctx.gHue);

    for (uint16_t dist = 0; dist < kHalfLength; ++dist) {
        // Map dist (0..79) → waveform index (0..127) with linear interpolation.
        const float wfPosFloat = static_cast<float>(dist) * 127.0f / 79.0f;
        const uint8_t wfIdx = static_cast<uint8_t>(wfPosFloat);
        const float frac = wfPosFloat - static_cast<float>(wfIdx);
        const uint8_t wfIdxNext = (wfIdx < kWaveformPoints - 1) ? (wfIdx + 1) : wfIdx;

        // Average 4 history frames with interpolation between adjacent samples.
        float waveformSample = 0.0f;
        for (uint8_t s = 0; s < kHistoryFrames; ++s) {
            const float a = static_cast<float>(m_ps->waveformHistory[zone][s][wfIdx]);
            const float b = static_cast<float>(m_ps->waveformHistory[zone][s][wfIdxNext]);
            waveformSample += a * (1.0f - frac) + b * frac;
        }
        waveformSample *= (1.0f / static_cast<float>(kHistoryFrames));

        // Dynamic normalisation: raw int16_t → [-1, +1] via max follower.
        const float normSample = waveformSample * invMaxFollower;

        // Temporal smooth per-LED (dt-corrected).
        m_ps->waveformLast[zone][dist] +=
            (normSample - m_ps->waveformLast[zone][dist]) * smoothAlpha;

        // SB brightness mapping: 0.5 + smoothed * 0.5 → [0, 1]
        // Zero-crossing at 50% brightness; peaks at 100%, troughs at 0%.
        float lum = 0.5f + m_ps->waveformLast[zone][dist] * 0.5f;
        lum = clamp01(lum);
        lum *= peak;

        // ====================================================================
        // Palette mapping at output (BloomParity pattern)
        // Spatial position drives palette index, brightness from waveform.
        // ====================================================================
        const float distNorm = static_cast<float>(dist) / static_cast<float>(kHalfLength);
        const float palFloat = distNorm * 128.0f + hueRotation;
        const uint8_t palIdx = static_cast<uint8_t>(static_cast<uint16_t>(palFloat) & 0xFFu);
        const uint8_t brightness = static_cast<uint8_t>(lum * 255.0f);

        CRGB c = ctx.palette.getColor(palIdx, brightness);
        SET_CENTER_PAIR(ctx, dist, c);
    }
#endif
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:WaveformParityEffect
uint8_t WaveformParityEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kWaveformParityEffectParameters) / sizeof(kWaveformParityEffectParameters[0]));
}

const plugins::EffectParameter* WaveformParityEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kWaveformParityEffectParameters[index];
}

bool WaveformParityEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "waveform_parity_effect_speed_scale") == 0) {
        gWaveformParityEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "waveform_parity_effect_output_gain") == 0) {
        gWaveformParityEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "waveform_parity_effect_centre_bias") == 0) {
        gWaveformParityEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float WaveformParityEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "waveform_parity_effect_speed_scale") == 0) return gWaveformParityEffectSpeedScale;
    if (strcmp(name, "waveform_parity_effect_output_gain") == 0) return gWaveformParityEffectOutputGain;
    if (strcmp(name, "waveform_parity_effect_centre_bias") == 0) return gWaveformParityEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:WaveformParityEffect

void WaveformParityEffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
}

const plugins::EffectMetadata& WaveformParityEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Waveform (Parity)",
        "Sensory Bridge 3.1.0 waveform mode (palette-clean, dt-corrected)",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect
