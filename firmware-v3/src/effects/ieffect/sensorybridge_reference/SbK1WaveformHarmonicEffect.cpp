/**
 * @file SbK1WaveformHarmonicEffect.cpp
 * @brief K1 Waveform Harmonic — pitch-mapped chroma dot constellation
 *
 * Architectural departure from K1 Waveform (0x1302):
 *
 *   0x1302 / 0x1313: Single dot. Position = waveform peak amplitude.
 *                    Harmony only affects colour.
 *
 *   0x1314 (this):   12 possible dots. Position = pitch class.
 *                    Bin 0 (C)  → pixel 80 (centre)
 *                    Bin 6 (F#) → pixel ~120 (mid-strip)
 *                    Bin 11 (B) → pixel ~153 (outer edge)
 *                    Brightness = that bin's chroma energy × RMS.
 *                    Harmony drives BOTH colour AND spatial position.
 *
 * A chord (e.g. C major = C, E, G) injects three coloured dots at three
 * distinct strip positions simultaneously. Their trails scroll outward and
 * decay at the RMS-driven rate. The mirror folds the right half to the left.
 *
 * Colour synthesis uses SB character (1.5x boost + led_share per bin).
 * Trail, scroll, mirror, and output are identical to K1 Waveform.
 */

#include "SbK1WaveformHarmonicEffect.h"

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
// Parameter descriptors
// ---------------------------------------------------------------------------

const plugins::EffectParameter SbK1WaveformHarmonicEffect::s_params[kParamCount] = {
    {"sensitivity", "Audio Gain",  0.01f, 10.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.1f,  "audio",  "x", false},
    {"contrast",    "Contrast",    0.0f,  3.0f,  0.5f, plugins::EffectParameterType::FLOAT, 0.25f, "visual", "x", false},
    {"chromaHue",   "Hue Offset",  0.0f,  1.0f,  0.0f, plugins::EffectParameterType::FLOAT, 0.01f, "colour", "",  false},
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

static inline float clampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool SbK1WaveformHarmonicEffect::init(plugins::EffectContext& ctx) {
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<HarmonicPsram*>(
            heap_caps_malloc(sizeof(HarmonicPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) HarmonicPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i] = {0.0f, 0.0f, 0.0f};
    }
    m_ps->scrollAccum = 0.0f;

    m_sensitivity = 1.0f;
    m_contrast    = 0.5f;
    m_chromaHue   = 0.0f;

    (void)ctx;
    return true;
}

void SbK1WaveformHarmonicEffect::cleanup() {
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

void SbK1WaveformHarmonicEffect::renderEffect(plugins::EffectContext& ctx) {
    if (!m_ps) return;

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        for (uint16_t i = 0; i < kStripLength; ++i) {
            m_ps->trailBuffer[i].r *= 0.9f;
            m_ps->trailBuffer[i].g *= 0.9f;
            m_ps->trailBuffer[i].b *= 0.9f;
            m_ps->trailBuffer[i].clip();
            if (i < ctx.ledCount) ctx.leds[i] = m_ps->trailBuffer[i].toCRGB();
        }
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    // =====================================================================
    // TRAIL FADE (dt-corrected, RMS-driven) — same formula as K1 Waveform
    // =====================================================================

    float rmsNow = clampF(ctx.audio.rms() * m_sensitivity, 0.0f, 1.0f);
    static constexpr float kMinDecayRate = 0.8f;
    static constexpr float kDecayScale   = 3.5f;
    float decayRate = kMinDecayRate + kDecayScale * rmsNow;
    float fade = expf(-decayRate * m_dt);

    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i].r *= fade;
        m_ps->trailBuffer[i].g *= fade;
        m_ps->trailBuffer[i].b *= fade;
    }

    // =====================================================================
    // SCROLL (dt-corrected sub-pixel) — identical to K1 Waveform
    // =====================================================================

    static constexpr float kBaseScrollRate = 150.0f;
    static constexpr float kSpeedMidpoint  = 10.0f;
    float scrollRate = kBaseScrollRate * (static_cast<float>(ctx.speed) / kSpeedMidpoint);
    m_ps->scrollAccum += scrollRate * m_dt;
    int pixelsToScroll = static_cast<int>(m_ps->scrollAccum);
    m_ps->scrollAccum -= static_cast<float>(pixelsToScroll);

    for (int s = 0; s < pixelsToScroll; ++s) {
        for (int i = kStripLength - 1; i > 0; --i) {
            m_ps->trailBuffer[i] = m_ps->trailBuffer[i - 1];
        }
        m_ps->trailBuffer[0] = {0.0f, 0.0f, 0.0f};
    }

    // =====================================================================
    // HARMONIC DOT INJECTION — one dot per active chroma bin
    //
    // Each of 12 chroma bins maps to a fixed pitch position on the right half:
    //   posF = kCenterRight + (c / 12.0) * (kHalfLength - 1)
    //     → bin 0 (C)  = pixel 80.0  (centre)
    //     → bin 6 (F#) = pixel 119.5 (mid-strip)
    //     → bin 11 (B) = pixel 153.1 (near edge)
    //
    // Brightness per bin: SB parity (1.5x boost, led_share scaling).
    // RMS scales the overall injection level so the display dims with audio.
    // =====================================================================

    const float photons  = (float)ctx.brightness / 255.0f;
    // led_share: each bin contributes proportionally (12 full bins = 1.0 total)
    static constexpr float kLedShare = 1.0f / 12.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        float bin    = m_chromaSmooth[c];
        float bright = applyContrast(bin, m_contrast);

        // SB parity: 1.5x brightness boost
        bright *= 1.5f;
        if (bright > 1.0f) bright = 1.0f;

        // Gate inactive bins — skip if below perceptual threshold
        if (bright < 0.02f) continue;

        // led_share keeps 12 full bins at 1.0 total.
        // RMS is already encoded in trail fade rate — don't double-apply.
        float dotBright = bright * kLedShare * photons;

        // Palette colour for this pitch class (hue position + chroma offset)
        float prog = (float)c / 12.0f;
        CRGB_F dotColor = paletteColorF(ctx.palette,
                                        prog + m_huePosition + m_chromaHue,
                                        dotBright);

        // Pitch-mapped position: C=80, chromatic ascent toward B=153
        float posF = (float)kCenterRight + ((float)c / 12.0f) * (float)(kHalfLength - 1);

        // Sub-pixel injection — blend across two adjacent pixels
        int posI   = static_cast<int>(posF);
        float frac = posF - static_cast<float>(posI);

        if (posI >= 0 && posI < (int)kStripLength) {
            m_ps->trailBuffer[posI].r += dotColor.r * (1.0f - frac);
            m_ps->trailBuffer[posI].g += dotColor.g * (1.0f - frac);
            m_ps->trailBuffer[posI].b += dotColor.b * (1.0f - frac);
        }
        if (posI + 1 >= 0 && posI + 1 < (int)kStripLength) {
            m_ps->trailBuffer[posI + 1].r += dotColor.r * frac;
            m_ps->trailBuffer[posI + 1].g += dotColor.g * frac;
            m_ps->trailBuffer[posI + 1].b += dotColor.b * frac;
        }
    }

    // =====================================================================
    // MIRROR right half to left half — centre-origin
    // =====================================================================

    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // =====================================================================
    // OUTPUT — identical to K1 Waveform
    // =====================================================================

    for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
        CRGB_F pixel = m_ps->trailBuffer[i];
        pixel.clip();
        ctx.leds[i] = pixel.toCRGB();
    }

    for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
        ctx.leds[kStripLength + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbK1WaveformHarmonicEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Waveform Harmonic",
        "Pitch-mapped chroma dot constellation",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbK1WaveformHarmonicEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbK1WaveformHarmonicEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbK1WaveformHarmonicEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "sensitivity") == 0) { m_sensitivity = clampF(value, 0.01f, 10.0f); return true; }
    if (strcmp(name, "contrast")    == 0) { m_contrast    = clampF(value, 0.0f,  3.0f);  return true; }
    if (strcmp(name, "chromaHue")   == 0) { m_chromaHue   = clampF(value, 0.0f,  1.0f);  return true; }
    return false;
}

float SbK1WaveformHarmonicEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "sensitivity") == 0) return m_sensitivity;
    if (strcmp(name, "contrast")    == 0) return m_contrast;
    if (strcmp(name, "chromaHue")   == 0) return m_chromaHue;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
