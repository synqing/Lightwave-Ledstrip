/**
 * @file SbWaveform310RefEffect.cpp
 * @brief Sensory Bridge 3.1.0 reference waveform mode (centre-origin adaptation)
 */

#include "SbWaveform310RefEffect.h"

#include "../../CoreEffects.h"
#include "../../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

namespace {

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline float clamp(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline const float* selectNoteChroma12(const audio::ControlBusFrame& cb, float* outMaxVal) {
    // Prefer SB 3.1.0 note chromagram if populated; fall back to LWLS chroma.
    float sbMax = cb.sb_chromagram_max_val;
    if (sbMax > 0.0001f) {
        if (outMaxVal) *outMaxVal = sbMax;
        return cb.sb_note_chromagram;
    }

    // LWLS/ES chroma fallback: use max for normalisation.
    float maxV = 0.0001f;
    for (uint8_t i = 0; i < audio::CONTROLBUS_NUM_CHROMA; ++i) {
        if (cb.chroma[i] > maxV) maxV = cb.chroma[i];
    }
    if (outMaxVal) *outMaxVal = maxV;
    return cb.chroma;
}

} // namespace

bool SbWaveform310RefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_lastHopSeq = 0;
    m_historyIndex = 0;
    m_historyPrimed = false;
    std::memset(m_waveformHistory, 0, sizeof(m_waveformHistory));
    std::memset(m_waveformLast, 0, sizeof(m_waveformLast));
    m_waveformPeakScaledLast = 0.0f;
    m_sumColorLast[0] = 0.0f;
    m_sumColorLast[1] = 0.0f;
    m_sumColorLast[2] = 0.0f;
    return true;
}

void SbWaveform310RefEffect::render(plugins::EffectContext& ctx) {
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        // No audio: fade out to black.
        fadeToBlackBy(ctx.leds, ctx.ledCount, 32);
        return;
    }

    // ---------------------------------------------------------------------
    // Waveform history (updated on hop)
    // ---------------------------------------------------------------------
    const bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;

        // Prefer SB waveform if present; otherwise use contract waveform.
        const int16_t* wf = ctx.audio.controlBus.sb_waveform;
        // If SB waveform is empty (ES adapter without sidecar), fall back.
        if (ctx.audio.controlBus.sb_waveform_peak_scaled < 0.0001f) {
            wf = ctx.audio.controlBus.waveform;
        }

        if (!m_historyPrimed) {
            // First hop: seed all history frames to avoid startup zeros.
            for (uint8_t f = 0; f < HISTORY_FRAMES; ++f) {
                std::memcpy(m_waveformHistory[f], wf, WAVEFORM_POINTS * sizeof(int16_t));
            }
            m_historyPrimed = true;
            m_historyIndex = 0;
        } else {
            std::memcpy(m_waveformHistory[m_historyIndex], wf, WAVEFORM_POINTS * sizeof(int16_t));
            m_historyIndex = (uint8_t)((m_historyIndex + 1) % HISTORY_FRAMES);
        }
    }

    // ---------------------------------------------------------------------
    // Peak follower (SB uses waveform_peak_scaled_last = peakScaled*0.05 + last*0.95)
    // ---------------------------------------------------------------------
    float peakScaled = ctx.audio.controlBus.sb_waveform_peak_scaled;
    if (peakScaled < 0.0001f) {
        // Fallback when SB sidecar isn’t populated: derive from RMS.
        peakScaled = clamp01(ctx.audio.rms() * 1.25f);
    }
    m_waveformPeakScaledLast = peakScaled * 0.05f + m_waveformPeakScaledLast * 0.95f;

    // ---------------------------------------------------------------------
    // Colour synthesis (SB 3.1.0: note chromagram → sum_color, smoothed 5/95)
    // ---------------------------------------------------------------------
    const float led_share = 255.0f / 12.0f;
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB sum_color = CRGB(0, 0, 0);
    float brightness_sum = 0.0f;

    float chromaMaxVal = 1.0f;
    const float* noteChroma = selectNoteChroma12(ctx.audio.controlBus, &chromaMaxVal);
    if (chromaMaxVal < 0.0001f) chromaMaxVal = 0.0001f;
    const float chromaInv = 1.0f / chromaMaxVal;

    constexpr uint8_t SQUARE_ITER = 1;  // SB default

    for (uint8_t c = 0; c < 12; ++c) {
        float prog = c / 12.0f;
        float bin = clamp01(noteChroma[c] * chromaInv);

        float bright = bin;
        for (uint8_t s = 0; s < SQUARE_ITER; ++s) {
            bright *= bright;
        }
        bright *= 1.5f;
        bright = clamp01(bright);
        bright *= led_share;  // 0..255-ish

        if (chromaticMode) {
            CRGB out_col;
            hsv2rgb_spectrum(CHSV((uint8_t)(255.0f * prog), 255, (uint8_t)bright), out_col);
            sum_color += out_col;
        } else {
            brightness_sum += bright;
        }
    }

    if (!chromaticMode) {
        // Non-chromatic: single hue from ctx.gHue (SB uses chroma_val knob).
        hsv2rgb_spectrum(CHSV(ctx.gHue, 255, (uint8_t)brightness_sum), sum_color);
    }

    float sum_color_float[3] = { (float)sum_color.r, (float)sum_color.g, (float)sum_color.b };
    sum_color_float[0] = sum_color_float[0] * 0.05f + m_sumColorLast[0] * 0.95f;
    sum_color_float[1] = sum_color_float[1] * 0.05f + m_sumColorLast[1] * 0.95f;
    sum_color_float[2] = sum_color_float[2] * 0.05f + m_sumColorLast[2] * 0.95f;
    m_sumColorLast[0] = sum_color_float[0];
    m_sumColorLast[1] = sum_color_float[1];
    m_sumColorLast[2] = sum_color_float[2];

    // ---------------------------------------------------------------------
    // Waveform render (centre-origin resample of SB NATIVE_RESOLUTION=128)
    // ---------------------------------------------------------------------
    float moodNorm = ctx.getMoodNormalized();
    float smoothing = (0.1f + moodNorm * 0.9f) * 0.05f;
    smoothing = clamp(smoothing, 0.0005f, 0.25f);

    float peak = m_waveformPeakScaledLast * 4.0f;
    if (peak > 1.0f) peak = 1.0f;
    if (peak < 0.0f) peak = 0.0f;

    const float brightnessScale = (float)ctx.brightness / 255.0f;
    const float silentScale = ctx.audio.controlBus.silentScale;

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        // Map dist 0..79 → waveform index 0..127 (SB NATIVE_RESOLUTION=128).
        uint16_t wfIndex = (uint16_t)((dist * (WAVEFORM_POINTS - 1) + (HALF_LENGTH - 1) / 2) / (HALF_LENGTH - 1));
        if (wfIndex >= WAVEFORM_POINTS) wfIndex = WAVEFORM_POINTS - 1;

        float waveform_sample = 0.0f;
        for (uint8_t s = 0; s < HISTORY_FRAMES; ++s) {
            waveform_sample += (float)m_waveformHistory[s][wfIndex];
        }
        waveform_sample *= (1.0f / (float)HISTORY_FRAMES);

        float input_wave_sample = waveform_sample / 128.0f;
        m_waveformLast[dist] = input_wave_sample * smoothing + m_waveformLast[dist] * (1.0f - smoothing);

        float output_brightness = m_waveformLast[dist];
        if (output_brightness > 1.0f) output_brightness = 1.0f;

        output_brightness = 0.5f + output_brightness * 0.5f;
        output_brightness = clamp(output_brightness, 0.0f, 1.0f);
        output_brightness *= peak;
        output_brightness *= silentScale;

        // Convert colour to final RGB with brightness and master brightness scaling.
        float r = m_sumColorLast[0] * output_brightness * brightnessScale;
        float g = m_sumColorLast[1] * output_brightness * brightnessScale;
        float b = m_sumColorLast[2] * output_brightness * brightnessScale;

        CRGB c((uint8_t)fminf(r, 255.0f), (uint8_t)fminf(g, 255.0f), (uint8_t)fminf(b, 255.0f));
        SET_CENTER_PAIR(ctx, dist, c);
    }
#endif
}

void SbWaveform310RefEffect::cleanup() {}

const plugins::EffectMetadata& SbWaveform310RefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Waveform (Ref)",
        "Sensory Bridge 3.1.0 waveform mode (centre-origin parity)",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference

