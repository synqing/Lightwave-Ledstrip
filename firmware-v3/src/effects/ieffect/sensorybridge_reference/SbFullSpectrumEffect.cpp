/**
 * @file SbFullSpectrumEffect.cpp
 * @brief SB Full Spectrum — maps 64 Goertzel bins to 80 pixels
 *
 * High-resolution frequency display with per-pixel smoothing.
 * Centre origin: pixel 0 (centre) = bass, pixel 79 (edge) = treble.
 * Uses asymmetric dt-corrected EMA for smooth attack/decay.
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

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// ---------------------------------------------------------------------------
// Smoothing time constants (seconds)
// ---------------------------------------------------------------------------

static constexpr float kTauAttack = 0.02f;   // ~0.4 alpha at 120 FPS — fast rise
static constexpr float kTauDecay  = 0.1f;    // ~0.08 alpha at 120 FPS — slow fade

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

    // Zero smoothing buffer and trail buffer
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
    // dt-corrected smoothing alphas
    const float dt = m_dt;
    const float alphaAttack = 1.0f - expf(-dt / kTauAttack);
    const float alphaDecay  = 1.0f - expf(-dt / kTauDecay);

    // Get raw 64-bin spectrum
    const float* bins = ctx.audio.bins64();

    if (!ctx.audio.available || !bins) {
        // No audio: decay smoothed values and output
        for (uint16_t p = 0; p < kHalfLength; ++p) {
            m_ps->smoothed[p] *= (1.0f - alphaDecay);
        }
    } else {
        // -----------------------------------------------------------------
        // Map 64 bins → 80 pixels via linear interpolation + smooth
        // -----------------------------------------------------------------
        for (uint16_t p = 0; p < kHalfLength; ++p) {
            // Source position in bin space
            const float srcIdx = static_cast<float>(p) * (63.0f / 79.0f);
            const uint8_t lo = static_cast<uint8_t>(srcIdx);
            const uint8_t hi = (lo < 63) ? (lo + 1) : 63;
            const float frac = srcIdx - static_cast<float>(lo);

            // Linearly interpolate between adjacent bins
            float energy = bins[lo] * (1.0f - frac) + bins[hi] * frac;
            if (energy < 0.0f) energy = 0.0f;
            if (energy > 1.0f) energy = 1.0f;

            // Asymmetric EMA: fast attack, slow decay
            float prev = m_ps->smoothed[p];
            float alpha = (energy > prev) ? alphaAttack : alphaDecay;
            m_ps->smoothed[p] = prev + alpha * (energy - prev);
        }
    }

    // -----------------------------------------------------------------
    // Trail persistence: decay the persistent buffer BEFORE writing new
    // Audio-coupled: loud = faster decay (punchy), quiet = slower (ambient)
    // Matches K1 Waveform pattern: decayRate = base + scale * amplitude
    // -----------------------------------------------------------------
    float rmsNow = ctx.audio.available ? ctx.audio.rms() : 0.0f;
    static constexpr float kMinDecayRate = 1.5f;   // per-second (silence: ~2s full decay)
    static constexpr float kDecayScale   = 6.0f;   // additional rate per unit RMS
    float decayRate = kMinDecayRate + kDecayScale * rmsNow;
    uint8_t fadeAmount = static_cast<uint8_t>(decayRate * dt * 255.0f);
    if (fadeAmount < 1) fadeAmount = 1;
    if (fadeAmount > 200) fadeAmount = 200;
    fadeToBlackBy(m_ps->trailBuffer, kStripLength, fadeAmount);

    // -----------------------------------------------------------------
    // Render target pixels into the trail buffer (additive blend)
    // New spectral energy is ADDED to the persistent buffer, creating
    // bright cores where audio is active and smooth decay where it fades.
    // -----------------------------------------------------------------
    const uint8_t userBrightness = ctx.brightness;

    for (uint16_t p = 0; p < kHalfLength; ++p) {
        float smoothedEnergy = m_ps->smoothed[p];
        if (smoothedEnergy < 0.0f) smoothedEnergy = 0.0f;
        if (smoothedEnergy > 1.0f) smoothedEnergy = 1.0f;

        // Hue from frequency position + gHue rotation
        uint8_t hue = static_cast<uint8_t>(
            static_cast<uint16_t>(p) * 255 / kHalfLength + ctx.gHue);
        uint8_t sat = ctx.saturation;
        uint8_t val = static_cast<uint8_t>(smoothedEnergy * 255.0f);

        // Scale by master brightness
        val = scale8(val, userBrightness);

        CRGB pixel;
        hsv2rgb_rainbow(CHSV(hue, sat, val), pixel);

        // Additive blend into trail buffer (bright cores accumulate)
        uint16_t ledIdx = kCenterRight + p;
        if (ledIdx < kStripLength) {
            m_ps->trailBuffer[ledIdx] += pixel;
        }
    }

    // -----------------------------------------------------------------
    // Mirror right → left in trail buffer (centre-origin symmetry)
    // -----------------------------------------------------------------
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        uint16_t rightIdx = kCenterRight + i;
        uint16_t leftIdx  = kCenterLeft - i;
        if (rightIdx < kStripLength && leftIdx < kStripLength) {
            m_ps->trailBuffer[leftIdx] = m_ps->trailBuffer[rightIdx];
        }
    }

    // -----------------------------------------------------------------
    // Output trail buffer to LEDs + copy strip 1 → strip 2
    // -----------------------------------------------------------------
    for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
        ctx.leds[i] = m_ps->trailBuffer[i];
    }
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
        "SB Full Spectrum",
        "High-resolution 64-bin frequency display across 80 pixels",
        plugins::EffectCategory::PARTY,
        1,
        "SpectraSynq"
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
