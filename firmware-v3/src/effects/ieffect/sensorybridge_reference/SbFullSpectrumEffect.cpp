/**
 * @file SbFullSpectrumEffect.cpp
 * @brief SB Beat Pulse — radial expansion pulse on each beat
 *
 * When a beat lands, a bright ring launches outward from centre.
 * Between beats, the ring contracts as the beat envelope decays.
 * Sparse injection: only 3 pixels (the wavefront ring) are written
 * per frame. The trail buffer decay creates the dark background.
 */

#include "SbFullSpectrumEffect.h"

#include "../../CoreEffects.h"
#include "../../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool SbFullSpectrumEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift — provides m_dt)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbFullSpectrumPsram*>(
            heap_caps_malloc(sizeof(SbFullSpectrumPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbFullSpectrumPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero trail buffer and state
    memset(m_ps, 0, sizeof(SbFullSpectrumPsram));

    (void)ctx;
    return true;
}

void SbFullSpectrumEffect::cleanup() {
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

void SbFullSpectrumEffect::renderEffect(plugins::EffectContext& ctx) {
    if (!m_ps) return;

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    const float dt = m_dt;

    // =================================================================
    // Stage 1: Decay trail buffer (audio-coupled, punchy for beats)
    // =================================================================
    float rms = ctx.audio.available ? ctx.audio.rms() : 0.0f;
    float decayRate = 4.0f + 15.0f * rms;  // Faster than K1 Waveform — beats need punchy decay
    uint8_t fadeAmt = static_cast<uint8_t>(fminf(decayRate * dt * 255.0f, 200.0f));
    if (fadeAmt < 1) fadeAmt = 1;
    fadeToBlackBy(m_ps->trailBuffer, kStripLength, fadeAmt);

    // =================================================================
    // Stage 2: Update beat envelope
    // =================================================================

    // On beat: snap envelope to beat strength
    if (ctx.audio.available && ctx.audio.isOnBeat()) {
        float strength = ctx.audio.beatStrength();
        if (strength > m_ps->beatEnvelope) {
            m_ps->beatEnvelope = strength;
        }
    }

    // Decay envelope: tau ~150ms (fast contraction)
    m_ps->beatEnvelope *= expf(-dt / 0.06f);
    if (m_ps->beatEnvelope < 0.01f) m_ps->beatEnvelope = 0.0f;

    // Smoothed RMS for fallback breathing
    float alphaRms = 1.0f - expf(-dt / 0.3f);
    m_ps->smoothedRms += (rms - m_ps->smoothedRms) * alphaRms;

    // =================================================================
    // Stage 3: Compute expansion and draw sparse wavefront
    // =================================================================
    float expansion = m_ps->beatEnvelope;

    // If beat confidence is low, use smoothed RMS for gentle breathing
    if (expansion < 0.05f) {
        expansion = m_ps->smoothedRms * 0.4f;  // Gentle breath, max 40% of strip
    }

    // Wavefront position (in pixels from centre)
    float frontPos = expansion * 79.0f;
    int frontPixel = static_cast<int>(frontPos + 0.5f);

    // Colour from gHue with slight hue shift during expansion
    uint8_t hue = ctx.gHue + static_cast<uint8_t>(expansion * 60);
    CRGB frontColor;
    hsv2rgb_rainbow(CHSV(hue, ctx.saturation, 255), frontColor);
    frontColor.nscale8(ctx.brightness);

    // Scale by envelope (brighter at peak, dim as it contracts)
    uint8_t envelopeScale = static_cast<uint8_t>(expansion * 255.0f);
    if (envelopeScale > 0) {
        frontColor.nscale8(envelopeScale);

        // Draw 3-pixel wavefront ring at the expansion edge
        for (int d = -1; d <= 1; ++d) {
            int p = frontPixel + d;
            if (p >= 0 && p < static_cast<int>(kHalfLength)) {
                uint16_t ledIdx = kCenterRight + static_cast<uint16_t>(p);
                if (ledIdx < kStripLength) {
                    m_ps->trailBuffer[ledIdx] += frontColor;
                }
            }
        }
    }

    // =================================================================
    // Stage 4: Mirror right -> left + output
    // =================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // Copy trail buffer to LED output
    uint16_t copyLen = std::min(kStripLength, ctx.ledCount);
    memcpy(ctx.leds, m_ps->trailBuffer, sizeof(CRGB) * copyLen);

    // Copy strip 1 -> strip 2
    for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
        ctx.leds[kStripLength + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbFullSpectrumEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Beat Pulse",
        "Radial expansion pulse on each beat with sparse wavefront",
        plugins::EffectCategory::PARTY,
        1,
        "SpectraSynq"
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
