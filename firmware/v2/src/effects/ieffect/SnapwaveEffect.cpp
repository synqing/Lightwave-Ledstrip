/**
 * @file SnapwaveEffect.cpp
 * @brief Snapwave effect implementation
 *
 * Ported from original v1 light_mode_snapwave() function.
 * Preserves the "accidental genius" algorithm that creates time-based
 * oscillating motion with chromagram-driven color.
 */

#include "SnapwaveEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool SnapwaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    
    // Initialize state
    memset(m_waveformHistory, 0, sizeof(m_waveformHistory));
    m_historyIndex = 0;
    m_peakFollower.reset(0.0f);
    m_positionFollower.reset(0.0f);
    memset(m_chromaSmoothed, 0, sizeof(m_chromaSmoothed));
    m_lastColor = CRGB::Black;
    memset(m_scrollBuffer, 0, sizeof(m_scrollBuffer));
    m_lastHopSeq = 0;
    m_sumColorLast[0] = 0.0f;
    m_sumColorLast[1] = 0.0f;
    m_sumColorLast[2] = 0.0f;
    memset(m_waveformLast, 0, sizeof(m_waveformLast));
    m_waveformPeakScaledLast = 0.0f;
    
    return true;
}

void SnapwaveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Time-based oscillation with chromagram color
    // Visual Foundation: Time-based oscillation pattern
    // Audio Enhancement: Chromagram drives color, RMS modulates amplitude

    // Clear output buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

#if !FEATURE_AUDIO_SYNC
    // Fallback: Time-based oscillation with palette color
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    
    // Time-based oscillation (visual foundation)
    float oscillation = 0.0f;
    for (uint8_t i = 0; i < 12; i++) {
        float phaseMultiplier = 1.0f + i * 0.5f;
        oscillation += sinf(ctx.totalTimeMs * 0.001f * phaseMultiplier * speedNorm);
    }
    oscillation = tanhf(oscillation * 2.0f);  // Characteristic "snap"
    
    // Map to position - CENTER ORIGIN: use 79.5 as true centre (between CENTER_LEFT=79 and CENTER_RIGHT=80)
    float centerPos = 79.5f;  // True centre between LEDs 79 and 80
    float posF = centerPos + oscillation * 79.5f;  // Range: 0 (left edge) to 159 (right edge) when amp = ±1
    int bufferPos = (int)(posF + (posF >= 0 ? 0.5f : -0.5f));
    if (bufferPos < 0) bufferPos = 0;
    if (bufferPos >= STRIP_LENGTH) bufferPos = STRIP_LENGTH - 1;
    
    // Fade scroll buffer
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        m_scrollBuffer[i].r = (uint8_t)(m_scrollBuffer[i].r * 0.95f);
        m_scrollBuffer[i].g = (uint8_t)(m_scrollBuffer[i].g * 0.95f);
        m_scrollBuffer[i].b = (uint8_t)(m_scrollBuffer[i].b * 0.95f);
    }
    
    // Shift buffer
    for (int16_t i = STRIP_LENGTH - 1; i > 0; i--) {
        m_scrollBuffer[i] = m_scrollBuffer[i - 1];
    }
    m_scrollBuffer[0] = CRGB::Black;
    
    // Place dot with palette color
    CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(oscillation * 50.0f), ctx.brightness);
    m_scrollBuffer[bufferPos] = color;
    
    // Render to output
    for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; i++) {
        ctx.leds[i] = m_scrollBuffer[i];
    }
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t strip2Idx = STRIP_LENGTH + i;
        if (strip2Idx < ctx.ledCount) {
            ctx.leds[strip2Idx] = m_scrollBuffer[i];
        }
    }
    return;
#else
    if (!ctx.audio.available) {
        // Fallback when audio unavailable: Time-based oscillation with palette color
        float speedNorm = ctx.speed / 50.0f;
        float dt = ctx.getSafeDeltaSeconds();
        
        // Time-based oscillation (visual foundation)
        float oscillation = 0.0f;
        for (uint8_t i = 0; i < 12; i++) {
            float phaseMultiplier = 1.0f + i * 0.5f;
            oscillation += sinf(ctx.totalTimeMs * 0.001f * phaseMultiplier * speedNorm);
        }
        oscillation = tanhf(oscillation * 2.0f);  // Characteristic "snap"
        
        // Map to position - CENTER ORIGIN: use 79.5 as true centre (between CENTER_LEFT=79 and CENTER_RIGHT=80)
        float centerPos = 79.5f;  // True centre between LEDs 79 and 80
        float posF = centerPos + oscillation * 79.5f;  // Range: 0 (left edge) to 159 (right edge) when amp = ±1
        int bufferPos = (int)(posF + (posF >= 0 ? 0.5f : -0.5f));
        if (bufferPos < 0) bufferPos = 0;
        if (bufferPos >= STRIP_LENGTH) bufferPos = STRIP_LENGTH - 1;
        
        // Fade scroll buffer
        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            m_scrollBuffer[i].r = (uint8_t)(m_scrollBuffer[i].r * 0.95f);
            m_scrollBuffer[i].g = (uint8_t)(m_scrollBuffer[i].g * 0.95f);
            m_scrollBuffer[i].b = (uint8_t)(m_scrollBuffer[i].b * 0.95f);
        }
        
        // Shift buffer
        for (int16_t i = STRIP_LENGTH - 1; i > 0; i--) {
            m_scrollBuffer[i] = m_scrollBuffer[i - 1];
        }
        m_scrollBuffer[0] = CRGB::Black;
        
        // Place dot with palette color
        CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(oscillation * 50.0f), ctx.brightness);
        m_scrollBuffer[bufferPos] = color;
        
        // Render to output
        for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; i++) {
            ctx.leds[i] = m_scrollBuffer[i];
        }
        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            uint16_t strip2Idx = STRIP_LENGTH + i;
            if (strip2Idx < ctx.ledCount) {
                ctx.leds[strip2Idx] = m_scrollBuffer[i];
            }
        }
        return;
    }

    // ========================================================================
    // 1. Waveform History & Peak Smoothing (Enhanced)
    // ========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    
    // Check for new audio hop (fresh data)
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        
        // Push current waveform into history ring buffer (matches Sensory Bridge)
        // Use waveformSize() for safety, but WAVEFORM_SIZE should match
        uint8_t waveformLen = ctx.audio.waveformSize();
        if (waveformLen > WAVEFORM_SIZE) waveformLen = WAVEFORM_SIZE;
        for (uint8_t i = 0; i < waveformLen; ++i) {
            int16_t sample = ctx.audio.getWaveformSample(i);
            m_waveformHistory[m_historyIndex][i] = sample;
        }
        m_historyIndex = (m_historyIndex + 1) % WAVEFORM_HISTORY_SIZE;
    }
    
    // Calculate waveform peak from RMS (proxy for waveform_peak_scaled)
    float waveformPeakRaw = ctx.audio.rms();
    
    // Get mood-normalized value for mood-adjusted smoothing
    float moodNorm = ctx.getMoodNormalized();
    
    // Use AsymmetricFollower for peak tracking with mood-adjusted smoothing
    // This creates natural attack/release behavior matching Sensory Bridge
    // Low mood (reactive): faster attack/release
    // High mood (smooth): slower attack/release
    float waveformPeakScaled = m_peakFollower.updateWithMood(waveformPeakRaw, dt, moodNorm);
    
    // Preserve original smoothing ratio for compatibility (0.02/0.98)
    // But now with asymmetric behavior from follower
    float waveformPeakScaledLast = waveformPeakScaled;

    // ========================================================================
    // 2. Chromagram Color Synthesis (original lines 16-65)
    // ========================================================================
    CRGB currentSumColor = CRGB(0, 0, 0);
    float totalMagnitude = 0.0f;
    
    // Use heavy_chroma for smoothed chromagram (matches original chromagram_smooth)
    const float* chroma = ctx.audio.controlBus.heavy_chroma;
    
    // Determine chromatic mode (original uses chromatic_mode boolean)
    // Map saturation parameter: >= 128 = chromatic mode, < 128 = single hue
    const bool chromaticMode = (ctx.saturation >= 128);
    
    // Map complexity to SQUARE_ITER (original uses frame_config.SQUARE_ITER)
    // Complexity 0-255 maps to SQUARE_ITER 0-3 (approximate)
    float squareIter = (ctx.complexity / 255.0f) * 3.0f;
    
    for (uint8_t c = 0; c < 12; c++) {
        float prog = c / 12.0f;  // Maps 0-11 to 0.0-0.916... (12-tone chromatic scale)
        float bin = chroma[c];
        
        // Apply squaring iterations for contrast enhancement
        float bright = bin;
        for (uint8_t s = 0; s < (uint8_t)squareIter; s++) {
            bright *= bright;
        }
        
        // Fractional iteration support (original lines 28-32)
        float fractIter = squareIter - floorf(squareIter);
        if (fractIter > 0.01f) {
            float squared = bright * bright;
            bright = bright * (1.0f - fractIter) + squared * fractIter;
        }
        
        // Only add colors from active notes (threshold 0.05, original line 35)
        if (bright > 0.05f) {
            if (chromaticMode) {
                // Chromatic mode: use palette system (matches AudioWaveformEffect pattern)
                // Map chromagram bin to palette index: prog (0.0-0.916) maps to palette index
                uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
                uint8_t brightU8 = (uint8_t)(fminf(bright, 255.0f));
                // Apply brightness scaling (PHOTONS equivalent)
                brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
                CRGB noteCol = ctx.palette.getColor(paletteIdx, brightU8);
                currentSumColor.r += noteCol.r;
                currentSumColor.g += noteCol.g;
                currentSumColor.b += noteCol.b;
            }
            totalMagnitude += bright;
        }
    }
    
    // Normalize and scale color (original lines 44-56)
    if (chromaticMode && totalMagnitude > 0.01f) {
        // Normalize by total magnitude to get pure color (weighted average of active notes)
        // This preserves color balance while removing brightness scaling
        float rNorm = (float)currentSumColor.r / totalMagnitude;
        float gNorm = (float)currentSumColor.g / totalMagnitude;
        float bNorm = (float)currentSumColor.b / totalMagnitude;
        
        // Scale back up by total magnitude for proper brightness
        // This restores the overall brightness based on chromagram activity
        currentSumColor.r = (uint8_t)(rNorm * totalMagnitude);
        currentSumColor.g = (uint8_t)(gNorm * totalMagnitude);
        currentSumColor.b = (uint8_t)(bNorm * totalMagnitude);
        
        // Clamp to valid RGB range (0-255)
        if (currentSumColor.r > 255) currentSumColor.r = 255;
        if (currentSumColor.g > 255) currentSumColor.g = 255;
        if (currentSumColor.b > 255) currentSumColor.b = 255;
    } else if (!chromaticMode) {
        // Single hue mode: use palette system with ctx.gHue
        uint8_t brightU8 = (uint8_t)(fminf(totalMagnitude, 255.0f));
        // Apply brightness scaling (PHOTONS equivalent)
        brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
        currentSumColor = ctx.palette.getColor(ctx.gHue, brightU8);
    }
    
    // ========================================================================
    // Color Component Smoothing (Stage 3: RGB per-component smoothing)
    // ========================================================================
    // Smooth RGB components separately (0.05/0.95 like Sensory Bridge waveform)
    // This creates smooth color transitions without abrupt changes
    float sum_color_float[3] = {
        (float)currentSumColor.r,
        (float)currentSumColor.g,
        (float)currentSumColor.b
    };
    
    // Apply per-component smoothing (0.05/0.95 retention)
    sum_color_float[0] = sum_color_float[0] * 0.05f + m_sumColorLast[0] * 0.95f;
    sum_color_float[1] = sum_color_float[1] * 0.05f + m_sumColorLast[1] * 0.95f;
    sum_color_float[2] = sum_color_float[2] * 0.05f + m_sumColorLast[2] * 0.95f;
    
    // Update smoothing state
    m_sumColorLast[0] = sum_color_float[0];
    m_sumColorLast[1] = sum_color_float[1];
    m_sumColorLast[2] = sum_color_float[2];
    
    // Create smoothed color for rendering
    m_lastColor = CRGB(
        (uint8_t)sum_color_float[0],
        (uint8_t)sum_color_float[1],
        (uint8_t)sum_color_float[2]
    );

    // ========================================================================
    // 3. Dynamic Trail Fading (original lines 67-79)
    // ========================================================================
    float absAmp = fabsf(waveformPeakScaledLast);
    if (absAmp > 1.0f) absAmp = 1.0f;
    
    float maxFadeReduction = 0.10f;  // Original: 0.10
    float dynamicFadeAmount = 1.0f - (maxFadeReduction * absAmp);
    
    // Apply dynamic fade to scroll buffer
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        m_scrollBuffer[i].r = (uint8_t)(m_scrollBuffer[i].r * dynamicFadeAmount);
        m_scrollBuffer[i].g = (uint8_t)(m_scrollBuffer[i].g * dynamicFadeAmount);
        m_scrollBuffer[i].b = (uint8_t)(m_scrollBuffer[i].b * dynamicFadeAmount);
    }

    // ========================================================================
    // 4. Time-Based Oscillation (original lines 96-110)
    // ========================================================================
    // Use ctx.totalTimeMs instead of millis() (frame-based timing)
    // Original: sin(millis() * 0.001f * (1.0f + i * 0.5f))
    float oscillation = 0.0f;
    
    // MOOD-based oscillation speed modulation
    // Low mood (reactive): Faster oscillation
    // High mood (smooth): Slower oscillation
    float moodNorm = ctx.getMoodNormalized();
    float speedMultiplier = 1.0f + moodNorm * 0.5f;  // 1.0x to 1.5x range
    
    // Create oscillation from dominant chromagram notes
    for (uint8_t i = 0; i < 12; i++) {
        // Use heavy_chroma for smoothed values (matches chromagram_smooth)
        if (chroma[i] > 0.1f) {  // Original threshold: 0.1
            // Each note contributes to position with different phase
            // Original: sin(millis() * 0.001f * (1.0f + i * 0.5f))
            float phaseMultiplier = 1.0f + i * 0.5f;  // Preserve exact formula
            oscillation += chroma[i] * sinf(ctx.totalTimeMs * 0.001f * phaseMultiplier * speedMultiplier);
        }
    }
    
    // Normalize oscillation to -1 to +1 range (original line 107)
    // tanh(oscillation * 2.0f) creates the characteristic "snap"
    oscillation = tanhf(oscillation * 2.0f);
    
    // Mix oscillation with amplitude for more dynamic movement (original line 110)
    float amp = oscillation * waveformPeakScaledLast * 0.7f;  // Preserve 0.7f scale
    
    // Clamp amplitude (original lines 112-113)
    if (amp > 1.0f) amp = 1.0f;
    else if (amp < -1.0f) amp = -1.0f;

    // ========================================================================
    // 6. Waveform Peak Smoothing (0.05/0.95 ratio)
    // ========================================================================
    m_waveformPeakScaledLast = waveformPeakScaled * 0.05f + m_waveformPeakScaledLast * 0.95f;
    
    // Calculate peak scaling (original: peak = waveform_peak_scaled_last * 4.0)
    float peak = m_waveformPeakScaledLast * 4.0f;
    if (peak > 1.0f) peak = 1.0f;
    
    // ========================================================================
    // 5. Scrolling Buffer (original line 82: shift_leds_up)
    // ========================================================================
    // Scroll buffer up by 1 position BEFORE rendering new waveform
    // Shift from end to beginning to avoid overwriting
    for (int16_t i = STRIP_LENGTH - 1; i > 0; i--) {
        m_scrollBuffer[i] = m_scrollBuffer[i - 1];
    }
    // Clear new position at start (will be filled with new waveform)
    m_scrollBuffer[0] = CRGB::Black;

    // ========================================================================
    // 6. Full Waveform Rendering with Oscillation Modulation
    // ========================================================================
    // Process all waveform samples and render full waveform pattern
    // Use oscillation to modulate waveform position/shape (centre-origin)
    uint8_t waveformLen = ctx.audio.waveformSize();
    if (waveformLen > WAVEFORM_SIZE) waveformLen = WAVEFORM_SIZE;
    
    // MOOD-based smoothing rate (matches Sensory Bridge: (0.1 + MOOD * 0.9) * 0.05)
    float smoothing = (0.1f + moodNorm * 0.9f) * 0.05f;
    
    // CENTER ORIGIN: Map waveform samples to LED positions
    float centerPos = 79.5f;  // True centre between LEDs 79 and 80
    
    // Render waveform to position 0 in scroll buffer (most recent frame)
    // The oscillation modulates the waveform shape/position
    for (uint8_t i = 0; i < waveformLen; ++i) {
        // 1. Average 4-frame waveform history
        float waveform_sample = 0.0f;
        for (uint8_t s = 0; s < WAVEFORM_HISTORY_SIZE; ++s) {
            waveform_sample += (float)m_waveformHistory[s][i];
        }
        waveform_sample /= (float)WAVEFORM_HISTORY_SIZE;
        
        // 2. Normalize: /128.0 (maps int16 to [-1,1] range)
        float input_wave_sample = waveform_sample / 128.0f;
        
        // 3. Apply smoothing (mood-based)
        m_waveformLast[i] = input_wave_sample * smoothing + m_waveformLast[i] * (1.0f - smoothing);
        
        // 4. Apply oscillation modulation to waveform position
        // Map waveform sample index to normalized position (0.0-1.0)
        float waveformPos = (float)i / (float)(waveformLen - 1);
        // Modulate position by oscillation (creates "snappy" waveform motion)
        // Oscillation ranges from -1 to +1, so modulate by ±10% of strip length
        float modulatedPos = waveformPos + oscillation * 0.1f;
        if (modulatedPos < 0.0f) modulatedPos = 0.0f;
        if (modulatedPos > 1.0f) modulatedPos = 1.0f;
        
        // 5. Map to centre-origin LED position in scroll buffer
        // Convert normalized position to LED index (0-159)
        float ledPosF = modulatedPos * (float)(STRIP_LENGTH - 1);
        uint16_t ledPos = (uint16_t)(ledPosF + 0.5f);
        if (ledPos >= STRIP_LENGTH) ledPos = STRIP_LENGTH - 1;
        
        // Calculate brightness with lift and peak scaling
        float output_brightness = m_waveformLast[i];
        if (output_brightness > 1.0f) output_brightness = 1.0f;
        if (output_brightness < -1.0f) output_brightness = -1.0f;
        
        // Lift: maps [-1,1] to [0,1]
        output_brightness = 0.5f + output_brightness * 0.5f;
        if (output_brightness > 1.0f) output_brightness = 1.0f;
        if (output_brightness < 0.0f) output_brightness = 0.0f;
        
        // Scale by peak
        output_brightness *= peak;
        
        // 6. Render to scroll buffer at position 0 (most recent) with chromagram color
        // Map to centre-origin position in buffer
        float distFromCenter = fabsf((float)ledPos - centerPos);
        uint16_t dist = (uint16_t)distFromCenter;
        if (dist < STRIP_LENGTH) {
            CRGB color = m_lastColor;
            color.nscale8_video((uint8_t)(output_brightness * 255.0f));
            // Add to buffer (additive blending for overlapping samples)
            m_scrollBuffer[dist] += color;
        }
    }
    
    // ========================================================================
    // 7. Render Scroll Buffer to Output (with centre-origin mapping)
    // ========================================================================
    // Render scrolling buffer to output LEDs
    // Buffer[0] = most recent, buffer[159] = oldest
    // Map to centre-origin for display
    for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; i++) {
        // Map linear buffer position to centre-origin
        float distFromCenter = fabsf((float)i - centerPos);
        uint16_t dist = (uint16_t)distFromCenter;
        if (dist < STRIP_LENGTH) {
            SET_CENTER_PAIR(ctx, dist, m_scrollBuffer[i]);
        }
    }
    
    // For strip 2 (160-319): mirror the buffer for dual-strip display
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t strip2Idx = STRIP_LENGTH + i;
        if (strip2Idx < ctx.ledCount) {
            float distFromCenter = fabsf((float)i - centerPos);
            uint16_t dist = (uint16_t)distFromCenter;
            if (dist < STRIP_LENGTH) {
                SET_CENTER_PAIR(ctx, dist, m_scrollBuffer[i]);
            }
        }
    }

#endif  // FEATURE_AUDIO_SYNC
}

void SnapwaveEffect::cleanup() {
    // No resources to free - all state is in instance members
}

const plugins::EffectMetadata& SnapwaveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Snapwave",
        "Time-based oscillation with chromagram color and snappy motion",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

