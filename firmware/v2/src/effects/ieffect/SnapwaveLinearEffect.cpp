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
 * 4. Calculate dot distance from centre
 * 5. Push current position to HISTORY BUFFER
 * 6. Render ALL history entries as fading trail
 * 7. Mirror for CENTRE ORIGIN (strip 1 only)
 *
 * Per-zone: all temporal state is dimensioned [kMaxZones] and accessed via
 * const int z = ctx.zoneId. ZoneComposer reuses one instance across zones.
 */

#include "SnapwaveLinearEffect.h"
#include "AudioReactivePolicy.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool SnapwaveLinearEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_peakSmoothed[z] = 0.0f;
        m_historyIndex[z] = 0;
    }

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SnapwavePsram*>(
            heap_caps_malloc(sizeof(SnapwavePsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(SnapwavePsram));
#endif
    return true;
}

void SnapwaveLinearEffect::pushHistory(int z, uint8_t distance, CRGB color) {
    if (!m_ps) return;
    m_ps->distanceHistory[z][m_historyIndex[z]] = distance;
    m_ps->colorHistory[z][m_historyIndex[z]] = color;
    m_historyIndex[z] = (m_historyIndex[z] + 1) % HISTORY_SIZE;
}

void SnapwaveLinearEffect::renderHistoryToLeds(int z, plugins::EffectContext& ctx) {
    if (!m_ps) return;
    uint16_t stripLen = (ctx.ledCount > 160) ? 160 : ctx.ledCount;
    uint16_t halfStrip = stripLen / 2;

    memset(ctx.leds, 0, stripLen * sizeof(CRGB));

    for (uint16_t i = 0; i < HISTORY_SIZE; ++i) {
        uint16_t idx = (m_historyIndex[z] + i) % HISTORY_SIZE;

        uint8_t distance = m_ps->distanceHistory[z][idx];
        CRGB color = m_ps->colorHistory[z][idx];

        // Calculate faded brightness (oldest = dimmest, newest = brightest)
        // i=0 is oldest, i=HISTORY_SIZE-1 is newest
        float ageFactor = (float)(i + 1) / (float)HISTORY_SIZE;  // 0.025 to 1.0
        float fadedBrightness = ageFactor * ageFactor;  // Quadratic falloff for more contrast

        // Scale colour by faded brightness
        uint8_t scale = (uint8_t)(fadedBrightness * 255.0f);
        CRGB fadedColor = color;
        fadedColor.nscale8(scale);

        // Skip if too dim
        if (scale < 5) continue;

        // Map distance to LED positions (CENTRE ORIGIN)
        // distance 0 = centre (79/80), distance 79 = edge (0/159)
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
        float timeMs = static_cast<float>(ctx.rawTotalTimeMs);

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
    float timeMs = static_cast<float>(ctx.rawTotalTimeMs);
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
    if (!m_ps) return;

    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

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
        float breathPhase = sinf(static_cast<float>(ctx.rawTotalTimeMs) * 0.0005f);
        currentPeak = 0.3f + 0.2f * breathPhase;  // 0.1 to 0.5 range
    }
#else
    currentPeak = 0.4f;  // Compile-time no audio: constant demo
#endif
    const float dt = AudioReactivePolicy::signalDt(ctx);
    const float peakAlpha = 1.0f - powf(PEAK_DECAY, dt * 60.0f);  // 0.02-at-60fps equivalent
    m_peakSmoothed[z] += (currentPeak - m_peakSmoothed[z]) * peakAlpha;

    // =========================================
    // STEP 2: Compute oscillation
    // =========================================
    float oscillation = computeOscillation(ctx);

    // =========================================
    // STEP 3: Mix oscillation with amplitude
    // =========================================
    // CRITICAL: No forced minimum - silence = stillness (dot at centre)
    float amp = oscillation * m_peakSmoothed[z] * AMPLITUDE_MIX;
    if (amp > 1.0f) amp = 1.0f;
    if (amp < -1.0f) amp = -1.0f;

    // =========================================
    // STEP 4: Calculate dot distance from centre
    // =========================================
    // amp ranges from -1 to +1
    // Map to distance from centre: 0 to halfStrip-1
    float distance = fabsf(amp) * (halfStrip - 1);
    uint8_t distInt = (uint8_t)(distance + 0.5f);
    if (distInt >= halfStrip) distInt = halfStrip - 1;

    // =========================================
    // STEP 5: Compute colour
    // =========================================
    CRGB dotColor = computeChromaColor(ctx);

    // =========================================
    // STEP 6: Push to HISTORY BUFFER
    // =========================================
    pushHistory(z, distInt, dotColor);

    // =========================================
    // STEP 7: Render history as trail
    // =========================================
    renderHistoryToLeds(z, ctx);

    // =========================================
    // STEP 8: Render to strip 2 (centre-origin at LED 239/240)
    // =========================================
    if (ctx.ledCount > 160) {
        uint16_t strip2Start = 160;
        uint16_t strip2Center = 240;  // Centre of strip 2
        uint16_t strip2HalfLen = 80;

        // Clear strip 2 first
        memset(&ctx.leds[strip2Start], 0, strip2HalfLen * 2 * sizeof(CRGB));

        for (uint16_t i = 0; i < HISTORY_SIZE; ++i) {
            uint16_t idx = (m_historyIndex[z] + i) % HISTORY_SIZE;
            uint8_t distance = m_ps->distanceHistory[z][idx];
            CRGB color = m_ps->colorHistory[z][idx];

            // Faded brightness (oldest = dimmest)
            float ageFactor = (float)(i + 1) / (float)HISTORY_SIZE;
            float fadedBrightness = ageFactor * ageFactor;
            uint8_t scale = (uint8_t)(fadedBrightness * 255.0f);
            CRGB fadedColor = color;
            fadedColor.nscale8(scale);

            if (scale < 5) continue;

            // Map to strip 2 centre-origin
            if (distance < strip2HalfLen) {
                uint16_t leftPos = strip2Center - 1 - distance;
                uint16_t rightPos = strip2Center + distance;

                if (leftPos >= strip2Start && leftPos < ctx.ledCount) {
                    ctx.leds[leftPos] += fadedColor;
                }
                if (rightPos < ctx.ledCount) {
                    ctx.leds[rightPos] += fadedColor;
                }
            }
        }
    }
}

void SnapwaveLinearEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
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
