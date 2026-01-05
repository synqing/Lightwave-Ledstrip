/**
 * @file WaveformEffect.cpp
 * @brief Waveform effect implementation - 1:1 parity with Sensory Bridge 3.1.0
 *
 * Direct port of light_mode_waveform() from Sensory Bridge 3.1.0.
 * Maintains exact visual parity with reference implementation.
 */

#include "WaveformEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool WaveformEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    
    // Initialize state
    memset(m_waveformHistory, 0, sizeof(m_waveformHistory));
    m_historyIndex = 0;
    memset(m_waveformLast, 0, sizeof(m_waveformLast));
    m_waveformPeakScaled = 0.0f;
    m_waveformPeakScaledLast = 0.0f;
    m_sumColorLast[0] = 0.0f;
    m_sumColorLast[1] = 0.0f;
    m_sumColorLast[2] = 0.0f;
    m_lastHopSeq = 0;
    m_targetPeak = 0.0f;
    m_peakFollower.reset(0.0f);
    
    return true;
}

void WaveformEffect::render(plugins::EffectContext& ctx) {
    // Clear output buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

#if !FEATURE_AUDIO_SYNC
    // Fallback: render nothing when audio unavailable
    return;
#else
    if (!ctx.audio.available) {
        // Fallback: render nothing when audio unavailable
        return;
    }

    // ========================================================================
    // Stage 1: Waveform History & Peak Calculation
    // ========================================================================
    // Check for new audio hop (fresh data)
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        
        // Push current waveform into history ring buffer (matches Sensory Bridge)
        uint8_t waveformLen = ctx.audio.waveformSize();
        if (waveformLen > WAVEFORM_SIZE) waveformLen = WAVEFORM_SIZE;
        for (uint8_t i = 0; i < waveformLen; ++i) {
            int16_t sample = ctx.audio.getWaveformSample(i);
            m_waveformHistory[m_historyIndex][i] = sample;
        }
        m_historyIndex = (m_historyIndex + 1) % WAVEFORM_HISTORY_SIZE;
    }
    
    // Calculate waveform_peak_scaled from RMS (proxy for Sensory Bridge algorithm)
    // Sensory Bridge: waveform_peak_scaled = max_waveform_val / max_waveform_val_follower
    // v2: Use RMS as proxy (0.0-1.0 range)
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();
    
    // Update target on new hop
    if (newHop) {
        m_targetPeak = ctx.audio.rms();
    }
    
    // Smooth waveform_peak_scaled with mood-adjusted smoothing
    // Replaces manual 0.05/0.95 smoothing with AsymmetricFollower
    m_waveformPeakScaled = m_peakFollower.updateWithMood(m_targetPeak, dt, moodNorm);
    // Additional smoothing for waveform_peak_scaled_last (preserves original behavior)
    m_waveformPeakScaledLast = m_waveformPeakScaled * 0.05f + m_waveformPeakScaledLast * 0.95f;
    
    // ========================================================================
    // Stage 2: Chromagram Color Calculation (matches Sensory Bridge lines 160-218)
    // ========================================================================
    const float led_share = 255.0f / 12.0f;  // Per-bin brightness scaling
    
    // Use raw chromagram (NOT heavy_chroma) - matches Sensory Bridge note_chromagram
    const float* chroma = ctx.audio.controlBus.chroma;
    
    // Calculate chromagram_max_val (maximum value in chromagram for normalization)
    float chromagram_max_val = 0.0f;
    for (uint8_t c = 0; c < 12; c++) {
        if (chroma[c] > chromagram_max_val) {
            chromagram_max_val = chroma[c];
        }
    }
    // Avoid division by zero
    if (chromagram_max_val < 0.001f) chromagram_max_val = 0.001f;
    
    // Map complexity to SQUARE_ITER (0-3 range)
    uint8_t squareIter = (uint8_t)((ctx.complexity / 255.0f) * 3.0f);
    if (squareIter > 3) squareIter = 3;
    
    // Determine chromatic mode (saturation >= 128 = chromatic mode)
    const bool chromaticMode = (ctx.saturation >= 128);
    
    // Map gHue to chroma_val (0.0-1.0 range)
    float chroma_val = ctx.gHue / 255.0f;
    
    CRGB sum_color = CRGB(0, 0, 0);
    float brightness_sum = 0.0f;
    
    for (uint8_t c = 0; c < 12; c++) {
        float prog = c / 12.0f;
        // Normalize by chromagram_max_val (matches Sensory Bridge line 172)
        float bin = chroma[c] * (1.0f / chromagram_max_val);
        
        // Apply squaring iterations for contrast enhancement (matches Sensory Bridge lines 174-177)
        float bright = bin;
        for (uint8_t s = 0; s < squareIter; s++) {
            bright *= bright;
        }
        
        // Apply gain (matches Sensory Bridge line 178)
        bright *= 1.5f;
        if (bright > 1.0f) bright = 1.0f;
        
        // Scale by led_share (matches Sensory Bridge line 183)
        bright *= led_share;
        
        if (chromaticMode) {
            // Chromatic mode: use palette system (matches AudioWaveformEffect pattern)
            // Map chromagram bin to palette index: prog (0.0-0.916) maps to palette index
            uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
            uint8_t brightU8 = (uint8_t)bright;
            // Apply brightness scaling
            brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
            CRGB out_col = ctx.palette.getColor(paletteIdx, brightU8);
            sum_color += out_col;
        } else {
            // Single hue mode: accumulate brightness (matches Sensory Bridge line 194)
            brightness_sum += bright;
        }
    }
    
    // If not chromatic mode, use single hue with total brightness (matches Sensory Bridge lines 198-201)
    if (!chromaticMode) {
        uint8_t brightU8 = (uint8_t)brightness_sum;
        // Apply brightness scaling
        brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
        sum_color = ctx.palette.getColor(ctx.gHue, brightU8);
    }
    
    // Smooth color (0.05/0.95 ratio - matches Sensory Bridge lines 212-218)
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
    
    // ========================================================================
    // Stage 3: Waveform Sample Processing (matches Sensory Bridge lines 220-259)
    // ========================================================================
    // Calculate smoothing rate (MOOD-based - matches Sensory Bridge line 230)
    float moodNorm = ctx.getMoodNormalized();
    float smoothing = (0.1f + moodNorm * 0.9f) * 0.05f;
    
    // Calculate peak scaling (matches Sensory Bridge lines 234-237)
    float peak = m_waveformPeakScaledLast * 4.0f;
    if (peak > 1.0f) peak = 1.0f;
    
    // Process all waveform samples (LINEAR mapping - NO centre-origin)
    // For v2: ctx.ledCount / 2 gives single strip length (160)
    uint16_t stripLength = ctx.ledCount / 2;
    if (stripLength > STRIP_LENGTH) stripLength = STRIP_LENGTH;
    
    uint8_t waveformLen = ctx.audio.waveformSize();
    if (waveformLen > WAVEFORM_SIZE) waveformLen = WAVEFORM_SIZE;
    
    // Process each LED (linear mapping: LED i = waveform sample i)
    for (uint16_t i = 0; i < stripLength && i < waveformLen; i++) {
        // 1. Average 4-frame history (matches Sensory Bridge lines 221-225)
        float waveform_sample = 0.0f;
        for (uint8_t s = 0; s < WAVEFORM_HISTORY_SIZE; ++s) {
            waveform_sample += (float)m_waveformHistory[s][i];
        }
        waveform_sample /= (float)WAVEFORM_HISTORY_SIZE;
        
        // 2. Normalize: /128.0 (maps int16 to [-1,1] range - matches Sensory Bridge line 226)
        float input_wave_sample = waveform_sample / 128.0f;
        
        // 3. Apply smoothing (matches Sensory Bridge line 232)
        m_waveformLast[i] = input_wave_sample * smoothing + m_waveformLast[i] * (1.0f - smoothing);
        
        // 4. Calculate brightness (matches Sensory Bridge lines 239-252)
        float output_brightness = m_waveformLast[i];
        if (output_brightness > 1.0f) {
            output_brightness = 1.0f;
        }
        if (output_brightness < -1.0f) {
            output_brightness = -1.0f;
        }
        
        // Lift: maps [-1,1] to [0,1] (matches Sensory Bridge line 244)
        output_brightness = 0.5f + output_brightness * 0.5f;
        if (output_brightness > 1.0f) {
            output_brightness = 1.0f;
        } else if (output_brightness < 0.0f) {
            output_brightness = 0.0f;
        }
        
        // Scale by peak (matches Sensory Bridge line 252)
        output_brightness *= peak;
        
        // 5. Render to LED (LINEAR mapping - matches Sensory Bridge lines 254-258)
        ctx.leds[i] = CRGB(
            (uint8_t)(sum_color_float[0] * output_brightness),
            (uint8_t)(sum_color_float[1] * output_brightness),
            (uint8_t)(sum_color_float[2] * output_brightness)
        );
    }
    
    // Render to strip 2 (mirror for dual-strip display)
    for (uint16_t i = 0; i < stripLength; i++) {
        uint16_t strip2Idx = stripLength + i;
        if (strip2Idx < ctx.ledCount) {
            ctx.leds[strip2Idx] = ctx.leds[i];
        }
    }

#endif  // FEATURE_AUDIO_SYNC
}

void WaveformEffect::cleanup() {
    // No resources to free - all state is in instance members
}

const plugins::EffectMetadata& WaveformEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Waveform",
        "Direct waveform visualization matching Sensory Bridge 3.1.0",
        plugins::EffectCategory::AUDIO,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

