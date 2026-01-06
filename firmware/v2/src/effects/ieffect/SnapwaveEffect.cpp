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
#include <Arduino.h>
#endif

#include <cmath>
#include <cstring>
#include <cstdio>

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
    
    // #region agent log
    #ifndef NATIVE_BUILD
    static uint32_t frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {  // Log every 60 frames (~0.5s at 120fps)
        Serial.printf("{\"id\":\"snap_pipeline\",\"location\":\"SnapwaveEffect.cpp:49\",\"message\":\"Render entry\",\"data\":{\"frame\":%u,\"audioAvailable\":%d,\"ledCount\":%u,\"brightness\":%u,\"gHue\":%u},\"timestamp\":%lu}\n", 
            frameCount, ctx.audio.available ? 1 : 0, ctx.ledCount, ctx.brightness, ctx.gHue, millis());
    }
    #endif
    // #endregion

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
    
    // #region agent log
    #ifndef NATIVE_BUILD
    static uint32_t chromaLogCount = 0;
    chromaLogCount++;
    if (chromaLogCount % 60 == 0) {
        float chromaSum = 0.0f;
        float chromaMax = 0.0f;
        for (uint8_t c = 0; c < 12; c++) {
            chromaSum += chroma[c];
            if (chroma[c] > chromaMax) chromaMax = chroma[c];
        }
        Serial.printf("{\"id\":\"snap_chroma\",\"location\":\"SnapwaveEffect.cpp:200\",\"message\":\"Chromagram values\",\"data\":{\"sum\":%.3f,\"max\":%.3f,\"rms\":%.3f},\"timestamp\":%lu}\n",
            chromaSum, chromaMax, ctx.audio.rms(), millis());
    }
    #endif
    // #endregion
    
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
    
    // #region agent log
    #ifndef NATIVE_BUILD
    static uint32_t colorLogCount = 0;
    colorLogCount++;
    if (colorLogCount % 60 == 0) {
        Serial.printf("{\"id\":\"snap_color\",\"location\":\"SnapwaveEffect.cpp:293\",\"message\":\"Final color\",\"data\":{\"r\":%u,\"g\":%u,\"b\":%u,\"totalMagnitude\":%.3f,\"chromaticMode\":%d},\"timestamp\":%lu}\n",
            m_lastColor.r, m_lastColor.g, m_lastColor.b, totalMagnitude, chromaticMode ? 1 : 0, millis());
    }
    #endif
    // #endregion

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
    // Reuse moodNorm from above (line 179)
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
    
    // #region agent log
    #ifndef NATIVE_BUILD
    static uint32_t oscLogCount = 0;
    oscLogCount++;
    if (oscLogCount % 60 == 0) {
        Serial.printf("{\"id\":\"snap_osc\",\"location\":\"SnapwaveEffect.cpp:341\",\"message\":\"Oscillation calculation\",\"data\":{\"oscRaw\":%.3f,\"oscTanh\":%.3f,\"peakScaled\":%.3f,\"amp\":%.3f},\"timestamp\":%lu}\n",
            oscillation / tanhf(1.0f), oscillation, waveformPeakScaledLast, amp, millis());
    }
    #endif
    // #endregion

    // ========================================================================
    // 4. Waveform Peak Smoothing (0.02/0.98 ratio - original line 13)
    // ========================================================================
    m_waveformPeakScaledLast = waveformPeakScaled * 0.02f + m_waveformPeakScaledLast * 0.98f;
    
    // Apply threshold to ignore tiny movements (original lines 88-91)
    float threshold = 0.05f;
    if (fabsf(amp) < threshold) {
        amp = 0.0f;
    }
    
    // ========================================================================
    // 5. Scrolling Buffer (original line 82: shift_leds_up)
    // ========================================================================
    // Scroll buffer up by 1 position (shift LEDs up)
    for (int16_t i = STRIP_LENGTH - 1; i > 0; i--) {
        m_scrollBuffer[i] = m_scrollBuffer[i - 1];
    }
    // Clear new position at start (will be filled with new dot)
    m_scrollBuffer[0] = CRGB::Black;
    
    // ========================================================================
    // 6. Single Dot Placement (original lines 115-122)
    // ========================================================================
    // Calculate position: pos = center + amp * half_length
    // Original: int center = NATIVE_RESOLUTION / 2;
    //          float pos_f = center + amp * (NATIVE_RESOLUTION / 2.0f);
    float centerPos = 79.5f;  // True centre between LEDs 79 and 80
    float posF = centerPos + amp * 79.5f;  // Range: 0 to 159 when amp = ±1
    int pos = (int)(posF + (posF >= 0 ? 0.5f : -0.5f));
    if (pos < 0) pos = 0;
    if (pos >= STRIP_LENGTH) pos = STRIP_LENGTH - 1;
    
    // Place ONE dot at calculated position with chromagram color (original line 122)
    m_scrollBuffer[pos] = m_lastColor;
    
    // #region agent log
    #ifndef NATIVE_BUILD
    static uint32_t posLogCount = 0;
    posLogCount++;
    if (posLogCount % 60 == 0) {
        uint16_t nonZeroPixels = 0;
        uint16_t maxBright = 0;
        for (uint16_t j = 0; j < STRIP_LENGTH; j++) {
            uint16_t bright = m_scrollBuffer[j].r + m_scrollBuffer[j].g + m_scrollBuffer[j].b;
            if (bright > 0) nonZeroPixels++;
            if (bright > maxBright) maxBright = bright;
        }
        Serial.printf("{\"id\":\"snap_render\",\"location\":\"SnapwaveEffect.cpp:384\",\"message\":\"Dot placement and buffer\",\"data\":{\"pos\":%d,\"posF\":%.2f,\"amp\":%.3f,\"bufferNonZero\":%u,\"maxBright\":%u,\"lastColorR\":%u},\"timestamp\":%lu}\n",
            pos, posF, amp, nonZeroPixels, maxBright, m_lastColor.r, millis());
    }
    #endif
    // #endregion
    
    // ========================================================================
    // 7. Render Scroll Buffer to Output (LINEAR mapping - no centre-origin)
    // ========================================================================
    // Render scrolling buffer LINEAR to output LEDs (original renders directly)
    // Buffer[0] = most recent, buffer[159] = oldest
    for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; i++) {
        ctx.leds[i] = m_scrollBuffer[i];
    }
    
    // Mirror to strip 2 (original line 124-126: mirror_image_downwards)
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t strip2Idx = STRIP_LENGTH + i;
        if (strip2Idx < ctx.ledCount) {
            ctx.leds[strip2Idx] = m_scrollBuffer[i];
        }
    }
    
    // #region agent log
    #ifndef NATIVE_BUILD
    static uint32_t outputLogCount = 0;
    outputLogCount++;
    if (outputLogCount % 60 == 0) {
        uint16_t outputNonZero = 0;
        uint16_t outputMaxBright = 0;
        for (uint16_t j = 0; j < ctx.ledCount && j < 20; j++) {  // Check first 20 LEDs
            uint16_t bright = ctx.leds[j].r + ctx.leds[j].g + ctx.leds[j].b;
            if (bright > 0) outputNonZero++;
            if (bright > outputMaxBright) outputMaxBright = bright;
        }
        Serial.printf("{\"id\":\"snap_output\",\"location\":\"SnapwaveEffect.cpp:399\",\"message\":\"Output buffer state\",\"data\":{\"outputNonZero\":%u,\"outputMaxBright\":%u,\"led0R\":%u,\"led79R\":%u},\"timestamp\":%lu}\n",
            outputNonZero, outputMaxBright, ctx.leds[0].r, ctx.leds[79].r, millis());
    }
    #endif
    // #endregion

#endif  // FEATURE_AUDIO_SYNC
}

void SnapwaveEffect::cleanup() {
    // No resources to free - all state is in instance members
}

const plugins::EffectMetadata& SnapwaveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Snapwave",
        "Single oscillating dot with scrolling history trail, chromagram-driven color, time-based snappy motion",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

