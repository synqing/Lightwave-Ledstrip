/**
 * @file SbK1WaveformEffect.cpp
 * @brief K1 Lightwave waveform dot mode (parity port)
 *
 * Exact algorithm from lightshow_modes.h:1393-1507:
 *   1. Base class provides smoothed peak, chromagram colour synthesis, failsafe
 *   2. Dynamic trail fade based on waveform amplitude
 *   3. Shift right-half trail buffer outward by 1 pixel (scroll)
 *   4. Sensitivity-scaled dot position from waveform peak
 *   5. Draw coloured dot at computed position
 *   6. Mirror right half to left half
 *   7. Copy strip 1 to strip 2
 */

#include "SbK1WaveformEffect.h"

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

const plugins::EffectParameter SbK1WaveformEffect::s_params[kParamCount] = {
    {"sensitivity", "Waveform Gain", 0.01f, 10.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.1f, "audio", "x", false},
    {"contrast", "Contrast", 0.0f, 3.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.25f, "visual", "x", false},
    {"saturation", "Saturation", 0.0f, 1.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.05f, "colour", "", false},
    {"chromaHue", "Hue Offset", 0.0f, 1.0f, 0.0f, plugins::EffectParameterType::FLOAT, 0.01f, "colour", "", false},
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

bool SbK1WaveformEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbK1WaveformPsram*>(
            heap_caps_malloc(sizeof(SbK1WaveformPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbK1WaveformPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero the trail buffer
    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i] = {0.0f, 0.0f, 0.0f};
    }

    // Reset parameters to defaults
    m_sensitivity = 1.0f;
    m_contrast    = 1.0f;
    m_saturation  = 1.0f;
    m_chromaHue   = 0.0f;

    (void)ctx;
    return true;
}

void SbK1WaveformEffect::cleanup() {
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

void SbK1WaveformEffect::renderEffect(plugins::EffectContext& ctx) {
    if (!m_ps) return;

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        // No audio: fade trail buffer and output to LEDs
        for (uint16_t i = 0; i < kStripLength; ++i) {
            m_ps->trailBuffer[i].r *= 0.9f;
            m_ps->trailBuffer[i].g *= 0.9f;
            m_ps->trailBuffer[i].b *= 0.9f;
            m_ps->trailBuffer[i].clip();
            if (i < ctx.ledCount) ctx.leds[i] = m_ps->trailBuffer[i].toCRGB();
        }
        // Mirror strip 1 to strip 2
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    // =====================================================================
    // K1 Waveform — exact parity with light_mode_waveform()
    // =====================================================================

    // --- K1 WAVEFORM COLOUR SYNTHESIS ---
    // K1 parity: additive accumulation of palette colours per chromagram bin.
    // K1 waveform: NO 0.5 cyan offset (that's Bloom). NO hue_position in chromatic mode.
    // K1's normalize is a no-op (divide then multiply by total_magnitude),
    // so the raw sum is the output — intentionally bright, clipped at output.
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F dotColor = {0.0f, 0.0f, 0.0f};
    float totalMag = 0.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        float bin = m_chromaSmooth[c];
        float bright = applyContrast(bin, m_contrast);

        if (bright > 0.05f) {
            float prog = c / 12.0f;
            // K1 parity: palette lookup at bin brightness, then accumulate
            // (matches K1's hsv(prog, SATURATION, bright) + additive sum)
            CRGB_F noteColor = paletteColorF(ctx.palette, prog, bright);
            dotColor += noteColor;
            totalMag += bright;
        }
    }

    // FAILSAFE: invisible dot prevention (K1 parity)
    if (totalMag < 0.01f && ctx.audio.rms() > 0.02f) {
        float fbPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, fbPos, ctx.audio.rms());
        totalMag = ctx.audio.rms();
    }

    // Non-chromatic: force palette position to CHROMA + hue_position
    // K1 parity: hsv(chroma_val + hue_position, SATURATION, total_magnitude)
    if (!chromaticMode) {
        float forcedPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, forcedPos, totalMag);
    }

    // Apply PHOTONS brightness
    float photons = (float)ctx.brightness / 255.0f;
    dotColor *= photons;
    dotColor.clip();

    // --- DYNAMIC TRAIL FADE ---
    // K1: abs_amp from waveform_peak_scaled (RAW, not smoothed)
    float absAmp = clampF(fabsf(m_wfPeakScaled), 0.0f, 1.0f);
    float fade = 1.0f - 0.10f * absAmp;

    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i].r *= fade;
        m_ps->trailBuffer[i].g *= fade;
        m_ps->trailBuffer[i].b *= fade;
    }

    // --- SHIFT UP (K1 shift_leds_up parity) ---
    // K1 shifts ENTIRE array up by 1 (higher indices), zeros index 0
    // Then mirrors right→left, so only right half scroll matters
    for (int i = kStripLength - 1; i > 0; --i) {
        m_ps->trailBuffer[i] = m_ps->trailBuffer[i - 1];
    }
    m_ps->trailBuffer[0] = {0.0f, 0.0f, 0.0f};

    // --- DOT POSITION ---
    float amp = m_wfPeakLast;
    if (fabsf(amp) < 0.05f) amp = 0.0f;

    float safeSensitivity = (m_sensitivity > 0.01f) ? m_sensitivity : 0.01f;
    amp *= 0.7f / safeSensitivity;
    if (amp > 1.0f) amp = 1.0f;
    if (amp < -1.0f) amp = -1.0f;

    float posF = (float)(kStripLength / 2) + amp * (float)(kStripLength / 2);

    // --- SUB-PIXEL DOT RENDERING ---
    // Blend dot across two adjacent pixels using fractional position
    int posI = static_cast<int>(posF);
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

    // --- MIRROR right half to left half ---
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // --- OUTPUT TO LEDS ---
    for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
        CRGB_F pixel = m_ps->trailBuffer[i];
        pixel.clip();
        ctx.leds[i] = pixel.toCRGB();
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

const plugins::EffectMetadata& SbK1WaveformEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Waveform",
        "K1 Lightwave waveform dot mode (parity port)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbK1WaveformEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbK1WaveformEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbK1WaveformEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    // Simple string matching for the 4 parameters
    if (strcmp(name, "sensitivity") == 0) {
        m_sensitivity = clampF(value, 0.01f, 10.0f);
        return true;
    }
    if (strcmp(name, "contrast") == 0) {
        m_contrast = clampF(value, 0.0f, 3.0f);
        return true;
    }
    if (strcmp(name, "saturation") == 0) {
        m_saturation = clampF(value, 0.0f, 1.0f);
        return true;
    }
    if (strcmp(name, "chromaHue") == 0) {
        m_chromaHue = clampF(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float SbK1WaveformEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "sensitivity") == 0) return m_sensitivity;
    if (strcmp(name, "contrast") == 0)    return m_contrast;
    if (strcmp(name, "saturation") == 0)  return m_saturation;
    if (strcmp(name, "chromaHue") == 0)   return m_chromaHue;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
