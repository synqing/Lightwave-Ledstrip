/**
 * @file SbReconstructedWaveformEffect.cpp
 * @brief SB Dynamic Arc — energy-driven fill/retreat arc effect
 *
 * Visualises audio energy as a crescendo/release arc. Energy builds over
 * seconds: the strip progressively fills outward from centre. When energy
 * releases, the light retreats inward. The bright element is a narrow
 * 3-pixel edge at the fill boundary; the interior is very dim.
 *
 * Algorithm:
 *   1. Decay trail buffer (rate adapts: building = slow, releasing = fast)
 *   2. Sample smoothed RMS into 32-slot history every ~125ms, compute trend
 *   3. Integrate trend into fill level with inertia + RMS nudge
 *   4. Draw 3-pixel edge at fill boundary + dim interior fill
 *   5. Centre-origin mirror + strip copy
 *
 * Performance: O(80) per frame — well within the 2.0ms budget.
 */

#include "SbReconstructedWaveformEffect.h"

#include "../../CoreEffects.h"
#include "../../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool SbReconstructedWaveformEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (audio pipeline)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbReconWfPsram*>(
            heap_caps_malloc(sizeof(SbReconWfPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbReconWfPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero all PSRAM state
    memset(m_ps, 0, sizeof(SbReconWfPsram));

    (void)ctx;
    return true;
}

void SbReconstructedWaveformEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#else
    delete m_ps;
    m_ps = nullptr;
#endif
    baseCleanup();
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void SbReconstructedWaveformEffect::renderEffect(plugins::EffectContext& ctx) {
    if (!m_ps) return;

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    // No audio: decay trail buffer toward black
    if (!ctx.audio.available) {
        fadeToBlackBy(m_ps->trailBuffer, kStripLength, 20);
        for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
            ctx.leds[i] = m_ps->trailBuffer[i];
        }
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    const float dt = m_dt;

    // =================================================================
    // Stage 1: Decay trail buffer
    // Gentle persistence — releasing = faster decay, building = slower.
    // =================================================================
    {
        float decayRate = 1.5f + 5.0f * fmaxf(0.0f, -m_ps->rmsTrend * 5.0f);
        uint8_t fadeAmt = static_cast<uint8_t>(fminf(decayRate * dt * 255.0f, 100.0f));
        if (fadeAmt < 1) fadeAmt = 1;
        fadeToBlackBy(m_ps->trailBuffer, kStripLength, fadeAmt);
    }

    // =================================================================
    // Stage 2: Update RMS trend
    // Smooth RMS with EMA, sample into circular history every ~125ms,
    // compute trend as difference between recent and older averages.
    // =================================================================
    {
        float rms = ctx.audio.rms();
        float aRms = 1.0f - expf(-dt / 0.1f);
        m_ps->smoothedRms += (rms - m_ps->smoothedRms) * aRms;

        uint32_t now = millis();
        if (now - m_ps->lastTrendUpdate >= 125) {
            m_ps->lastTrendUpdate = now;
            m_ps->rmsHistory[m_ps->rmsHistIdx] = m_ps->smoothedRms;
            m_ps->rmsHistIdx = (m_ps->rmsHistIdx + 1) % kRmsHistSize;

            // Trend: compare recent average (last 8 samples ~1s)
            // versus older average (samples 16-24 ~2-3s ago)
            float recent = 0.0f;
            float older  = 0.0f;
            for (int i = 0; i < 8; ++i) {
                recent += m_ps->rmsHistory[(m_ps->rmsHistIdx - 1 - i + kRmsHistSize) % kRmsHistSize];
                older  += m_ps->rmsHistory[(m_ps->rmsHistIdx - 17 - i + kRmsHistSize) % kRmsHistSize];
            }
            recent *= 0.125f;  // /8
            older  *= 0.125f;
            m_ps->rmsTrend = recent - older;  // Positive = building, negative = releasing
        }
    }

    // =================================================================
    // Stage 3: Update fill level with inertia
    // Fill level integrates the trend with clamping. A small nudge
    // toward current RMS prevents divergence over long periods.
    // =================================================================
    {
        static constexpr float kFillSpeed = 0.15f;
        m_ps->fillLevel += m_ps->rmsTrend * kFillSpeed * dt * 10.0f;
        // Nudge toward current RMS (prevents long-term drift)
        m_ps->fillLevel += (m_ps->smoothedRms - m_ps->fillLevel) * 0.02f * dt;
        if (m_ps->fillLevel < 0.0f) m_ps->fillLevel = 0.0f;
        if (m_ps->fillLevel > 1.0f) m_ps->fillLevel = 1.0f;
    }

    // =================================================================
    // Stage 4: Draw sparse fill edge + dim interior
    // The bright element is a 3-pixel edge at the fill boundary.
    // Interior fill is very dim (builds slowly). Colour from palette
    // via gHue — warmer when building, cooler when releasing.
    // =================================================================
    {
        const int fillPixel = static_cast<int>(m_ps->fillLevel * 79.0f);

        // Hue shifts with trend direction: building = warm, releasing = cool offset
        const uint8_t hue = ctx.gHue + static_cast<uint8_t>(m_ps->rmsTrend > 0.0f ? 0 : 128);
        const uint8_t brightVal = static_cast<uint8_t>(fminf(m_ps->smoothedRms * 255.0f, 255.0f));

        CRGB edgeColour;
        hsv2rgb_rainbow(CHSV(hue, ctx.saturation, brightVal), edgeColour);
        edgeColour.nscale8(ctx.brightness);

        // Draw 3-pixel edge at fill boundary (centre-origin right half)
        for (int d = -1; d <= 1; ++d) {
            int p = fillPixel + d;
            if (p >= 0 && p < static_cast<int>(kHalfLength)) {
                m_ps->trailBuffer[kCenterRight + p] += edgeColour;
            }
        }

        // Dim interior fill behind the edge (very low brightness)
        const uint8_t fillBright = static_cast<uint8_t>(fminf(m_ps->smoothedRms * 30.0f, 30.0f));
        if (fillBright > 0) {
            CRGB fillColour;
            hsv2rgb_rainbow(CHSV(hue, ctx.saturation, fillBright), fillColour);
            fillColour.nscale8(ctx.brightness);

            for (int p = 0; p < fillPixel; ++p) {
                m_ps->trailBuffer[kCenterRight + p] += fillColour;
            }
        }
    }

    // =================================================================
    // Stage 5: Mirror right half to left half + output
    // =================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // Output trail buffer to LEDs
    for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
        ctx.leds[i] = m_ps->trailBuffer[i];
    }

    // Copy strip 1 to strip 2
    for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
        ctx.leds[kStripLength + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbReconstructedWaveformEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Dynamic Arc",
        "Energy-driven crescendo/release arc — fills outward, retreats inward",
        plugins::EffectCategory::PARTY,
        1,
        "SB.Waveform"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface (no user-tuneable parameters)
// ---------------------------------------------------------------------------

uint8_t SbReconstructedWaveformEffect::getParameterCount() const {
    return 0;
}

const plugins::EffectParameter* SbReconstructedWaveformEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool SbReconstructedWaveformEffect::setParameter(const char* name, float value) {
    (void)name;
    (void)value;
    return false;
}

float SbReconstructedWaveformEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
