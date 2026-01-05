/**
 * @file AudioWaveformEffect.cpp
 * @brief Sensory Bridge-style waveform implementation
 */

#include "AudioWaveformEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifdef FEATURE_EFFECT_VALIDATION
#include "../../validation/EffectValidationMacros.h"
#endif

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool AudioWaveformEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    memset(m_waveformHistory, 0, sizeof(m_waveformHistory));
    memset(m_waveformLast, 0, sizeof(m_waveformLast));
    m_maxWaveformValFollower = SWEET_SPOT_MIN_LEVEL;
    m_waveformPeakScaled = 0.0f;
    m_waveformPeakScaledLast = 0.0f;
    m_sumColorLast[0] = 0.0f;
    m_sumColorLast[1] = 0.0f;
    m_sumColorLast[2] = 0.0f;
    m_historyIndex = 0;
    m_lastHopSeq = 0;
    m_beatSyncMode = false;
    m_beatSyncPhase = 0.0f;
    m_waveformPeakScaledTarget = 0.0f;
    m_peakScaledFollower.reset(0.0f);
    return true;
}

void AudioWaveformEffect::render(plugins::EffectContext& ctx) {
    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        return;
    }

    // Check if we have a new hop
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;

        float maxWaveformValRaw = 0.0f;

        // Push current waveform into history ring buffer
        for (uint8_t i = 0; i < WAVEFORM_SIZE; ++i) {
            int16_t sample = ctx.audio.getWaveformSample(i);
            m_waveformHistory[m_historyIndex][i] = sample;
            int32_t absSample = (sample < 0) ? -sample : sample;
            if (absSample > maxWaveformValRaw) {
                maxWaveformValRaw = (float)absSample;
            }
        }
        m_historyIndex = (m_historyIndex + 1) % WAVEFORM_HISTORY_SIZE;

        float maxWaveformVal = maxWaveformValRaw - SWEET_SPOT_MIN_LEVEL;
        if (maxWaveformVal > m_maxWaveformValFollower) {
            float delta = maxWaveformVal - m_maxWaveformValFollower;
            m_maxWaveformValFollower += delta * PEAK_FOLLOW_ATTACK;
        } else if (maxWaveformVal < m_maxWaveformValFollower) {
            float delta = m_maxWaveformValFollower - maxWaveformVal;
            m_maxWaveformValFollower -= delta * PEAK_FOLLOW_RELEASE;
            if (m_maxWaveformValFollower < SWEET_SPOT_MIN_LEVEL) {
                m_maxWaveformValFollower = SWEET_SPOT_MIN_LEVEL;
            }
        }

        float waveformPeakScaledRaw = 0.0f;
        if (m_maxWaveformValFollower > 0.0f) {
            waveformPeakScaledRaw = maxWaveformVal / m_maxWaveformValFollower;
        }
        
        // Update target for mood-based smoothing (will be smoothed every frame)
        m_waveformPeakScaledTarget = waveformPeakScaledRaw;
    }

    // Smooth waveform_peak_scaled with mood-adjusted smoothing
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();
    m_waveformPeakScaled = m_peakScaledFollower.updateWithMood(m_waveformPeakScaledTarget, dt, moodNorm);
    
    // Additional smoothing for waveform_peak_scaled_last (0.03/0.97 tuned for LGP)
    m_waveformPeakScaledLast = m_waveformPeakScaled * 0.03f + m_waveformPeakScaledLast * 0.97f;

    // Compute sum_colour from chromagram (matching Sensory Bridge light_mode_waveform)
    float chromaMax = 0.0f;
    for (uint8_t c = 0; c < 12; ++c) {
        float v = ctx.audio.controlBus.chroma[c];
        if (v > chromaMax) chromaMax = v;
    }

    const float chromaNorm = (chromaMax > 0.0f) ? (1.0f / chromaMax) : 0.0f;
    const float led_share = 255.0f / 12.0f;
    CRGB sum_color = CRGB(0, 0, 0);
    float brightness_sum = 0.0f;
    const bool chromaticMode = (ctx.saturation >= 128);

    for (uint8_t c = 0; c < 12; ++c) {
        float prog = c / 12.0f;
        float bin = ctx.audio.controlBus.chroma[c] * chromaNorm;

        // Apply squaring and gain
        float bright = bin;
        bright = bright * bright;  // Square once
        bright *= 1.5f;
        if (bright > 1.0f) bright = 1.0f;

        bright *= led_share;

        if (chromaticMode) {
            uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
            uint8_t brightU8 = (uint8_t)bright;
            brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
            CRGB out_col = ctx.palette.getColor(paletteIdx, brightU8);
            sum_color += out_col;
        } else {
            brightness_sum += bright;
        }
    }

    if (!chromaticMode) {
        uint8_t brightU8 = (uint8_t)brightness_sum;
        brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
        sum_color = ctx.palette.getColor(ctx.gHue, brightU8);
    }

    // Smooth sum_colour (0.04/0.96 tuned for LGP)
    float sum_color_float[3] = {
        (float)sum_color.r,
        (float)sum_color.g,
        (float)sum_color.b
    };

    sum_color_float[0] = sum_color_float[0] * 0.04f + m_sumColorLast[0] * 0.96f;
    sum_color_float[1] = sum_color_float[1] * 0.04f + m_sumColorLast[1] * 0.96f;
    sum_color_float[2] = sum_color_float[2] * 0.04f + m_sumColorLast[2] * 0.96f;

    m_sumColorLast[0] = sum_color_float[0];
    m_sumColorLast[1] = sum_color_float[1];
    m_sumColorLast[2] = sum_color_float[2];

    // Beat-sync mode: use beat phase to time-stretch waveform display
    // When enabled, waveform scrolls in sync with beat tempo
    m_beatSyncMode = (ctx.speed > 75);  // Enable beat-sync when speed > 75
    if (m_beatSyncMode) {
        m_beatSyncPhase = ctx.audio.beatPhase();
    } else {
        m_beatSyncPhase = 0.0f;
    }

    // Render waveform (matching Sensory Bridge algorithm)
    // Map ctx.speed to smoothing rate (MOOD equivalent)
    // Speed 1-50 maps to smoothing 0.1-1.0 range
    float speedNorm = ctx.speed / 50.0f;  // 0.02 to 1.0
    float smoothing = (0.1f + speedNorm * 0.9f) * 0.035f;  // 0.0035 to 0.035 (tuned for LGP)

    // Compute peak scaling
    float peak = m_waveformPeakScaledLast * 4.0f;
    if (peak > 1.0f) peak = 1.0f;

#ifdef FEATURE_EFFECT_VALIDATION
    VALIDATION_INIT(20);  // Effect ID for AudioWaveform
    VALIDATION_PHASE(0.0f, 0.0f);  // No phase for waveform effect
    VALIDATION_SPEED(0.0f, 0.0f);  // No speed scaling
    VALIDATION_AUDIO(0.0f, m_waveformPeakScaledLast, 0.0f);  // Use peakScaled as energy proxy
    VALIDATION_SUBMIT(::lightwaveos::validation::g_validationRing);
#endif

    for (uint8_t i = 0; i < WAVEFORM_SIZE; ++i) {
        // Average waveform history (4 frames)
        float waveform_sample = 0.0f;
        for (uint8_t s = 0; s < WAVEFORM_HISTORY_SIZE; ++s) {
            waveform_sample += (float)m_waveformHistory[s][i];
        }
        waveform_sample /= (float)WAVEFORM_HISTORY_SIZE;

        // Normalise to Sensory Bridge scale
        float input_wave_sample = waveform_sample / 128.0f;

        // Apply follower smoothing (matching Sensory Bridge)
        m_waveformLast[i] = input_wave_sample * smoothing + m_waveformLast[i] * (1.0f - smoothing);

        // Apply brightness shaping (matching Sensory Bridge)
        float output_brightness = m_waveformLast[i];
        if (output_brightness > 1.0f) output_brightness = 1.0f;
        if (output_brightness < -1.0f) output_brightness = -1.0f;

        // Lift: 0.5 + x * 0.5 (maps [-1,1] to [0,1])
        output_brightness = 0.5f + output_brightness * 0.5f;
        if (output_brightness > 1.0f) output_brightness = 1.0f;
        if (output_brightness < 0.0f) output_brightness = 0.0f;

        // Scale by peak
        output_brightness *= peak;

        // Map waveform index to radial distance (centre-origin)
        // Waveform index 0 = centre, index 127 = edge
        // Apply beat-sync phase offset if enabled (time-stretches display)
        float waveformPos = (float)i / (float)(WAVEFORM_SIZE - 1);
        if (m_beatSyncMode) {
            // Time-stretch: shift waveform position based on beat phase
            // Creates smooth scrolling effect synchronized to tempo
            waveformPos = fmodf(waveformPos + m_beatSyncPhase * 0.3f, 1.0f);
        }
        float distF = waveformPos * (float)(HALF_LENGTH - 1);
        uint16_t dist = (uint16_t)distF;

        // Compute final colour (sum_color * brightness)
        CRGB color = CRGB(
            (uint8_t)(m_sumColorLast[0] * output_brightness),
            (uint8_t)(m_sumColorLast[1] * output_brightness),
            (uint8_t)(m_sumColorLast[2] * output_brightness)
        );

        // Render symmetric about centre pair
        SET_CENTER_PAIR(ctx, dist, color);
    }
#endif  // FEATURE_AUDIO_SYNC
}

void AudioWaveformEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& AudioWaveformEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Audio Waveform",
        "Time-domain waveform with history smoothing and chromagram colour, centre-origin mirrored",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
