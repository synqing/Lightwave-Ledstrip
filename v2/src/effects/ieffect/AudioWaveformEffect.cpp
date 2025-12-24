/**
 * @file AudioWaveformEffect.cpp
 * @brief Sensory Bridge-style waveform implementation
 */

#include "AudioWaveformEffect.h"
#include "../CoreEffects.h"

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
    m_waveformPeakScaledLast = 0.0f;
    m_sumColorLast[0] = 0.0f;
    m_sumColorLast[1] = 0.0f;
    m_sumColorLast[2] = 0.0f;
    m_historyIndex = 0;
    m_lastHopSeq = 0;
    return true;
}

void AudioWaveformEffect::render(plugins::EffectContext& ctx) {
    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // If audio not available, show subtle idle animation
    if (!ctx.audio.available) {
        float phase = ctx.getPhase(0.5f);
        float idleBrightness = 0.1f + 0.05f * sinf(phase * 2.0f * 3.14159265f);
        uint8_t bright = (uint8_t)(idleBrightness * ctx.brightness);
        CRGB idleColor = ctx.palette.getColor(ctx.gHue, bright);
        
        if (ctx.centerPoint < ctx.ledCount) {
            ctx.leds[ctx.centerPoint] = idleColor;
        }
        if (ctx.centerPoint > 0) {
            ctx.leds[ctx.centerPoint - 1] = idleColor;
        }
        return;
    }

    // Check if we have a new hop
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        
        // Push current waveform into history ring buffer
        for (uint8_t i = 0; i < WAVEFORM_SIZE; ++i) {
            m_waveformHistory[m_historyIndex][i] = ctx.audio.getWaveformSample(i);
        }
        m_historyIndex = (m_historyIndex + 1) % WAVEFORM_HISTORY_SIZE;

        // Update waveform_peak_scaled_last (0.05/0.95 smoothing)
        float waveformPeakScaled = ctx.audio.rms();  // Use RMS as peak proxy
        m_waveformPeakScaledLast = waveformPeakScaled * 0.05f + m_waveformPeakScaledLast * 0.95f;

        // Compute sum_color from chromagram (matching Sensory Bridge light_mode_waveform)
        const float led_share = 255.0f / 12.0f;
        CRGB sum_color = CRGB(0, 0, 0);
        float brightness_sum = 0.0f;

        for (uint8_t c = 0; c < 12; ++c) {
            float prog = c / 12.0f;
            float bin = ctx.audio.controlBus.chroma[c];

            // Apply squaring and gain
            float bright = bin;
            bright = bright * bright;  // Square once
            bright *= 1.5f;
            if (bright > 1.0f) bright = 1.0f;

            bright *= led_share;

            // Use palette for colour (no hue wheel)
            uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue) % 256;
            CRGB out_col = ctx.palette.getColor(paletteIdx, (uint8_t)(bright * ctx.brightness));
            sum_color += out_col;
        }

        // Smooth sum_color (0.05/0.95 matching Sensory Bridge)
        float sum_color_float[3] = {
            (float)sum_color.r,
            (float)sum_color.g,
            (float)sum_color.b
        };

        sum_color_float[0] = sum_color_float[0] * 0.05f + m_sumColorLast[0] * 0.95f;
        sum_color_float[1] = sum_color_float[1] * 0.05f + m_sumColorLast[1] * 0.95f;
        sum_color_float[2] = sum_color_float[2] * 0.05f + m_sumColorLast[2] * 0.95f;

        m_sumColorLast[0] = sum_color_float[0];
        m_sumColorLast[1] = sum_color_float[1];
        m_sumColorLast[2] = sum_color_float[2];
    }

    // Render waveform (matching Sensory Bridge algorithm)
    // Map ctx.speed to smoothing rate (MOOD equivalent)
    // Speed 1-50 maps to smoothing 0.1-1.0 range
    float speedNorm = ctx.speed / 50.0f;  // 0.02 to 1.0
    float smoothing = (0.1f + speedNorm * 0.9f) * 0.05f;  // 0.005 to 0.05

    // Compute peak scaling
    float peak = m_waveformPeakScaledLast * 4.0f;
    if (peak > 1.0f) peak = 1.0f;

    for (uint8_t i = 0; i < WAVEFORM_SIZE; ++i) {
        // Average waveform history (4 frames)
        float waveform_sample = 0.0f;
        for (uint8_t s = 0; s < WAVEFORM_HISTORY_SIZE; ++s) {
            waveform_sample += (float)m_waveformHistory[s][i];
        }
        waveform_sample /= (float)WAVEFORM_HISTORY_SIZE;

        // Normalize to [-1, 1] range (matching Sensory Bridge: / 128.0)
        // Note: Sensory Bridge stores samples in a normalized range, so /128.0 works
        // Our int16_t samples need /32768.0, but we'll match the legacy behavior
        float input_wave_sample = waveform_sample / 32768.0f;

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
        float distF = ((float)i / (float)(WAVEFORM_SIZE - 1)) * (float)(HALF_LENGTH - 1);
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
