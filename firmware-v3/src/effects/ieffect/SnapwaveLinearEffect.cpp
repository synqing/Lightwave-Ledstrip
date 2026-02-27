// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file SnapwaveLinearEffect.cpp
 * @brief Scrolling waveform with time-based oscillation and HISTORY BUFFER trails
 *
 * ORIGINAL SNAPWAVE ALGORITHM - Restored from SensoryBridge light_mode_snapwave()
 *
 * Algorithm per frame:
 * 1. Smooth peak (2% new, 98% old)
 * 2. Compute time-based oscillation from chromagram + sin(millis())
 * 3. Apply tanh() for "snap" characteristic
 * 4. Calculate dot distance from center
 * 5. Push current position to HISTORY BUFFER
 * 6. Render ALL history entries as fading trail
 * 7. Mirror for CENTER ORIGIN (strip 1 only)
 */

#include "SnapwaveLinearEffect.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool SnapwaveLinearEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_peakSmoothed = 0.0f;
    m_historyIndex = 0;

    // Clear history buffers
    memset(m_distanceHistory, 0, sizeof(m_distanceHistory));
    memset(m_colorHistory, 0, sizeof(m_colorHistory));

    return true;
}

void SnapwaveLinearEffect::pushHistory(uint8_t distance, CRGB color) {
    // Push new entry to ring buffer
    m_distanceHistory[m_historyIndex] = distance;
    m_colorHistory[m_historyIndex] = color;

    // Advance ring buffer index
    m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE;
}

void SnapwaveLinearEffect::renderHistoryToLeds(plugins::EffectContext& ctx) {
    uint16_t stripLen = (ctx.ledCount > 160) ? 160 : ctx.ledCount;
    uint16_t halfStrip = stripLen / 2;  // 80

    // Clear strip 1 first
    memset(ctx.leds, 0, stripLen * sizeof(CRGB));

    // Render history from oldest to newest (so newest overwrites oldest)
    // Oldest entry is at m_historyIndex, newest is at m_historyIndex - 1
    float brightness = 1.0f;

    for (uint16_t i = 0; i < HISTORY_SIZE; ++i) {
        // Calculate actual index in ring buffer (oldest first)
        uint16_t idx = (m_historyIndex + i) % HISTORY_SIZE;

        uint8_t distance = m_distanceHistory[idx];
        CRGB color = m_colorHistory[idx];

        // Calculate faded brightness (oldest = dimmest, newest = brightest)
        // i=0 is oldest, i=HISTORY_SIZE-1 is newest
        float ageFactor = (float)(i + 1) / (float)HISTORY_SIZE;  // 0.025 to 1.0
        float fadedBrightness = ageFactor * ageFactor;  // Quadratic falloff for more contrast

        // Scale color by faded brightness
        uint8_t scale = (uint8_t)(fadedBrightness * 255.0f);
        CRGB fadedColor = color;
        fadedColor.nscale8(scale);

        // Skip if too dim
        if (scale < 5) continue;

        // Map distance to LED positions (CENTER ORIGIN)
        // distance 0 = center (79/80), distance 79 = edge (0/159)
        if (distance < halfStrip) {
            uint16_t leftPos = (halfStrip - 1) - distance;   // 79 - dist
            uint16_t rightPos = halfStrip + distance;         // 80 + dist

            // Additive blend for overlapping trails
            ctx.leds[leftPos] += fadedColor;
            ctx.leds[rightPos] += fadedColor;
        }
    }
}

float SnapwaveLinearEffect::computeOscillation(const plugins::EffectContext& ctx) {
    float oscillation = 0.0f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // CRITICAL FIX: Gate by overall audio energy
        // If RMS is below threshold, return 0 immediately (dot stays at center)
        float rms = ctx.audio.rms();
        if (rms < ENERGY_GATE_THRESHOLD) {
            return 0.0f;  // Silence = stillness
        }

        // Time-based oscillation from chromagram
        // Each active note contributes with different phase offset
        // Formula: sum(chromagram[i] * sin(millis() * 0.001 * (1.0 + i * 0.5)))
        float timeMs = (float)ctx.totalTimeMs;

        for (uint8_t i = 0; i < 12; ++i) {
            float chromaVal = ctx.audio.controlBus.chroma[i];
            if (chromaVal > NOTE_THRESHOLD) {
                float freqMult = 1.0f + PHASE_SPREAD * i;
                oscillation += chromaVal * sinf(timeMs * BASE_FREQUENCY * freqMult);
            }
        }
    }
#else
    // Without audio, use time-based oscillation for demo
    float timeMs = (float)ctx.totalTimeMs;
    oscillation = sinf(timeMs * 0.002f);  // Slow oscillation for visual demo
#endif

    // Normalize with tanh() for "snap" characteristic
    // tanh(x * 2.0) provides natural limiting at ±1
    oscillation = tanhf(oscillation * TANH_SCALE);

    return oscillation;
}

CRGB SnapwaveLinearEffect::computeChromaColor(const plugins::EffectContext& ctx) {
    float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
    float totalMagnitude = 0.0f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        for (uint8_t c = 0; c < 12; ++c) {
            float prog = c / 12.0f;
            float bin = ctx.audio.controlBus.chroma[c];

            // Square for contrast (SQUARE_ITER = 1)
            float bright = bin * bin;

            if (bright > COLOR_THRESHOLD) {
                uint8_t hue = (uint8_t)(prog * 255.0f + ctx.gHue);
                CRGB noteCol = ctx.palette.getColor(hue, 255);

                sumR += noteCol.r * bright;
                sumG += noteCol.g * bright;
                sumB += noteCol.b * bright;
                totalMagnitude += bright;
            }
        }

        // Normalize and scale
        if (totalMagnitude > 0.01f) {
            sumR /= totalMagnitude;
            sumG /= totalMagnitude;
            sumB /= totalMagnitude;
            sumR *= fminf(totalMagnitude, 1.0f);
            sumG *= fminf(totalMagnitude, 1.0f);
            sumB *= fminf(totalMagnitude, 1.0f);
        }
    }
#endif

    // Fallback color when no audio
    if (totalMagnitude < 0.01f) {
        CRGB fallback = ctx.palette.getColor(ctx.gHue, 255);
        sumR = fallback.r;
        sumG = fallback.g;
        sumB = fallback.b;
    }

    // Brightness scaling
    float brightnessScale = ctx.brightness / 255.0f;
    sumR *= brightnessScale;
    sumG *= brightnessScale;
    sumB *= brightnessScale;

    return CRGB(
        (uint8_t)fminf(sumR, 255.0f),
        (uint8_t)fminf(sumG, 255.0f),
        (uint8_t)fminf(sumB, 255.0f)
    );
}

void SnapwaveLinearEffect::render(plugins::EffectContext& ctx) {
    uint16_t stripLen = (ctx.ledCount > 160) ? 160 : ctx.ledCount;
    uint16_t halfStrip = stripLen / 2;  // 80

    // =========================================
    // STEP 1: Smooth the peak (2% new, 98% old)
    // =========================================
    float currentPeak = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        currentPeak = ctx.audio.rms();
    } else {
        // No audio available: slow breathing demo (obviously NOT musical)
        // 2-second cycle so it's clearly not responding to audio
        float breathPhase = sinf(ctx.totalTimeMs * 0.0005f);
        currentPeak = 0.3f + 0.2f * breathPhase;  // 0.1 to 0.5 range
    }
#else
    currentPeak = 0.4f;  // Compile-time no audio: constant demo
#endif
    m_peakSmoothed = currentPeak * PEAK_ATTACK + m_peakSmoothed * PEAK_DECAY;

    // =========================================
    // STEP 2: Compute oscillation
    // =========================================
    float oscillation = computeOscillation(ctx);

    // =========================================
    // STEP 3: Mix oscillation with amplitude
    // =========================================
    // CRITICAL: No forced minimum - silence = stillness (dot at center)
    float amp = oscillation * m_peakSmoothed * AMPLITUDE_MIX;
    if (amp > 1.0f) amp = 1.0f;
    if (amp < -1.0f) amp = -1.0f;

    // =========================================
    // STEP 4: Calculate dot distance from center
    // =========================================
    // amp ranges from -1 to +1
    // Map to distance from center: 0 to halfStrip-1
    float distance = fabsf(amp) * (halfStrip - 1);
    uint8_t distInt = (uint8_t)(distance + 0.5f);
    if (distInt >= halfStrip) distInt = halfStrip - 1;

    // =========================================
    // STEP 5: Compute color
    // =========================================
    CRGB dotColor = computeChromaColor(ctx);

    // =========================================
    // STEP 6: Push to HISTORY BUFFER
    // =========================================
    pushHistory(distInt, dotColor);

    // =========================================
    // STEP 7: Render history as trail
    // =========================================
    renderHistoryToLeds(ctx);

    // =========================================
    // STEP 8: Keep strip 2 black
    // =========================================
    if (ctx.ledCount > 160) {
        memset(&ctx.leds[160], 0, (ctx.ledCount - 160) * sizeof(CRGB));
    }
}

void SnapwaveLinearEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& SnapwaveLinearEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Snapwave",
        "Bouncing dot with history trail - time-based oscillation with snap",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
