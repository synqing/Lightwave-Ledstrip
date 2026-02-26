/**
 * @file SbWaveform310RefEffect.cpp
 * @brief Sensory Bridge 3.1.0 reference waveform mode (centre-origin adaptation)
 */

#include "SbWaveform310RefEffect.h"

#include "../AudioReactivePolicy.h"
#include "../../CoreEffects.h"
#include "../../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

namespace {

const plugins::EffectParameter kParameters[] = {
    {"colour_tau", "Colour Tau", 0.050f, 1.200f, 0.325f,
     plugins::EffectParameterType::FLOAT, 0.005f, "colour", "s", false},
};

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
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbWaveform310Psram*>(
            heap_caps_malloc(sizeof(SbWaveform310Psram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    std::memset(m_ps, 0, sizeof(SbWaveform310Psram));
#endif
    for (int z = 0; z < kMaxZones; ++z) {
        m_lastHopSeq[z]           = 0;
        m_historyIndex[z]         = 0;
        m_historyPrimed[z]        = false;
        m_waveformPeakScaledLast[z] = 0.0f;
        m_waveformMaxFollower[z]  = 750.0f;
    }
    return true;
}

void SbWaveform310RefEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        // No audio: fade out to black.
        fadeToBlackBy(ctx.leds, ctx.ledCount, 32);
        return;
    }

    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    const float dt = AudioReactivePolicy::signalDt(ctx);

    // ---------------------------------------------------------------------
    // Waveform history (updated on hop)
    // ---------------------------------------------------------------------
    const bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq[z]);
    if (newHop) {
        m_lastHopSeq[z] = ctx.audio.controlBus.hop_seq;

        // Prefer SB waveform if present; otherwise use contract waveform.
        const int16_t* wf = ctx.audio.controlBus.sb_waveform;
        // If SB waveform is empty (ES adapter without sidecar), fall back.
        if (ctx.audio.controlBus.sb_waveform_peak_scaled < 0.0001f) {
            wf = ctx.audio.controlBus.waveform;
        }

        if (!m_historyPrimed[z]) {
            // First hop: seed all history frames to avoid startup zeros.
            for (uint8_t f = 0; f < HISTORY_FRAMES; ++f) {
                std::memcpy(m_ps->waveformHistory[z][f], wf, WAVEFORM_POINTS * sizeof(int16_t));
            }
            m_historyPrimed[z] = true;
            m_historyIndex[z] = 0;
        } else {
            std::memcpy(m_ps->waveformHistory[z][m_historyIndex[z]], wf, WAVEFORM_POINTS * sizeof(int16_t));
            m_historyIndex[z] = (uint8_t)((m_historyIndex[z] + 1) % HISTORY_FRAMES);
        }

        // Adaptive max follower for int16_t domain normalisation.
        float frameMaxAbs = 0.0f;
        for (uint8_t i = 0; i < WAVEFORM_POINTS; ++i) {
            const float a = fabsf(static_cast<float>(wf[i]));
            if (a > frameMaxAbs) frameMaxAbs = a;
        }
        if (frameMaxAbs > m_waveformMaxFollower[z]) {
            m_waveformMaxFollower[z] += (frameMaxAbs - m_waveformMaxFollower[z]) * 0.25f;
        } else {
            m_waveformMaxFollower[z] -= (m_waveformMaxFollower[z] - frameMaxAbs) * 0.005f;
        }
        if (m_waveformMaxFollower[z] < 100.0f) {
            m_waveformMaxFollower[z] = 100.0f;
        }
    }

    // ---------------------------------------------------------------------
    // Peak follower (SB uses waveform_peak_scaled_last = peakScaled*0.05 + last*0.95)
    // ---------------------------------------------------------------------
    float peakScaled = ctx.audio.controlBus.sb_waveform_peak_scaled;
    if (peakScaled < 0.0001f) {
        // Fallback when SB sidecar isn't populated: derive from RMS.
        peakScaled = clamp01(ctx.audio.rms() * 1.25f);
    }
    const float peakAlpha = 1.0f - powf(0.95f, dt * 60.0f);
    m_waveformPeakScaledLast[z] += (peakScaled - m_waveformPeakScaledLast[z]) * peakAlpha;

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

    // dt-corrected colour smoothing (one-pole filter, tau ~0.325s from SB 0.05/0.95 @ 60fps)
    const float colourAlpha = 1.0f - expf(-dt / m_colourTau);

    float sum_color_float[3] = { (float)sum_color.r, (float)sum_color.g, (float)sum_color.b };
    for (int c = 0; c < 3; ++c) {
        sum_color_float[c] = m_ps->sumColourLast[z][c] + colourAlpha * (sum_color_float[c] - m_ps->sumColourLast[z][c]);
        m_ps->sumColourLast[z][c] = sum_color_float[c];
    }

    // ---------------------------------------------------------------------
    // Waveform render (centre-origin resample of SB NATIVE_RESOLUTION=128)
    // ---------------------------------------------------------------------
    float moodNorm = ctx.getMoodNormalized();
    float smoothing = (0.1f + moodNorm * 0.9f) * 0.05f;
    smoothing = clamp(smoothing, 0.0005f, 0.25f);
    // Convert frame-based smoothing to dt-corrected alpha (~48 FPS reference).
    const float smoothingAlpha = 1.0f - powf(1.0f - smoothing, dt * 48.0f);

    float peak = m_waveformPeakScaledLast[z] * 4.0f;
    if (peak > 1.0f) peak = 1.0f;
    if (peak < 0.0f) peak = 0.0f;

    const float brightnessScale = (float)ctx.brightness / 255.0f;
    const float invFollower = 1.0f / m_waveformMaxFollower[z];

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        // Map dist 0..79 → waveform index 0..127 (SB NATIVE_RESOLUTION=128).
        uint16_t wfIndex = (uint16_t)((dist * (WAVEFORM_POINTS - 1) + (HALF_LENGTH - 1) / 2) / (HALF_LENGTH - 1));
        if (wfIndex >= WAVEFORM_POINTS) wfIndex = WAVEFORM_POINTS - 1;

        float waveform_sample = 0.0f;
        for (uint8_t s = 0; s < HISTORY_FRAMES; ++s) {
            waveform_sample += (float)m_ps->waveformHistory[z][s][wfIndex];
        }
        waveform_sample *= (1.0f / (float)HISTORY_FRAMES);

        // waveform_sample is int16_t-domain; normalise using adaptive follower.
        const float input_wave_sample = waveform_sample * invFollower;
        m_ps->waveformLast[z][dist] += (input_wave_sample - m_ps->waveformLast[z][dist]) * smoothingAlpha;

        float output_brightness = m_ps->waveformLast[z][dist];
        if (output_brightness > 1.0f) output_brightness = 1.0f;

        output_brightness = 0.5f + output_brightness * 0.5f;
        output_brightness = clamp(output_brightness, 0.0f, 1.0f);
        output_brightness *= peak;
        // Convert colour to final RGB with brightness and master brightness scaling.
        float r = m_ps->sumColourLast[z][0] * output_brightness * brightnessScale;
        float g = m_ps->sumColourLast[z][1] * output_brightness * brightnessScale;
        float b = m_ps->sumColourLast[z][2] * output_brightness * brightnessScale;

        CRGB c((uint8_t)fminf(r, 255.0f), (uint8_t)fminf(g, 255.0f), (uint8_t)fminf(b, 255.0f));
        SET_CENTER_PAIR(ctx, dist, c);
    }
#endif
}

void SbWaveform310RefEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

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

uint8_t SbWaveform310RefEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* SbWaveform310RefEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) {
        return nullptr;
    }
    return &kParameters[index];
}

bool SbWaveform310RefEffect::setParameter(const char* name, float value) {
    if (!name) {
        return false;
    }

    if (std::strcmp(name, "colour_tau") == 0) {
        if (value < 0.050f) value = 0.050f;
        if (value > 1.200f) value = 1.200f;
        m_colourTau = value;
        return true;
    }
    return false;
}

float SbWaveform310RefEffect::getParameter(const char* name) const {
    if (!name) {
        return 0.0f;
    }
    if (std::strcmp(name, "colour_tau") == 0) {
        return m_colourTau;
    }
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
