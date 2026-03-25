/**
 * @file SbK1WaveformHybridEffect.cpp
 * @brief K1 Waveform dot mode with SB 3.0.0 colour synthesis character
 *
 * Option C hybrid: K1's bouncing-dot-with-trails visual concept, but with
 * three colour synthesis corrections that bring the colour character closer
 * to SB 3.0.0's intent:
 *
 *   1. 1.5x brightness boost — SB applies bright *= 1.5 after squaring,
 *      making dominant notes visibly brighter. K1 original is missing this.
 *
 *   2. led_share scaling — SB divides brightness budget by 12 (chroma bins)
 *      so individual bins contribute proportionally. 12 bins at full = white.
 *      K1 original passes full brightness per bin → overflows immediately,
 *      soft-knee cap always engages, losing relative dynamics.
 *
 *   3. Temporal RGB smoothing — SB applies a heavy 0.05/0.95 EMA on the
 *      output colour (~20-frame inertia at 120 FPS). Produces smooth,
 *      organic colour evolution. K1 original has no smoothing → colour
 *      changes 20x faster, appearing twitchy.
 *
 * Everything else (trail fade, scroll, sub-pixel dot, mirror, output) is
 * identical to K1 Waveform (0x1302).
 */

#include "SbK1WaveformHybridEffect.h"

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

const plugins::EffectParameter SbK1WaveformHybridEffect::s_params[kParamCount] = {
    {"sensitivity", "Waveform Gain", 0.01f, 10.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.1f, "audio", "x", false},
    {"contrast", "Contrast", 0.0f, 3.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.25f, "visual", "x", false},
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

bool SbK1WaveformHybridEffect::init(plugins::EffectContext& ctx) {
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<HybridPsram*>(
            heap_caps_malloc(sizeof(HybridPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) HybridPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero trail buffer and scroll accumulator
    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i] = {0.0f, 0.0f, 0.0f};
    }
    m_ps->scrollAccum = 0.0f;

    // Zero colour smoothing state
    m_dotColorSmooth = {0.0f, 0.0f, 0.0f};

    // Reset parameters to defaults
    m_sensitivity = 1.0f;
    m_contrast    = 1.0f;
    m_chromaHue   = 0.0f;

    (void)ctx;
    return true;
}

void SbK1WaveformHybridEffect::cleanup() {
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

void SbK1WaveformHybridEffect::renderEffect(plugins::EffectContext& ctx) {
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
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    // =====================================================================
    // COLOUR SYNTHESIS — SB 3.0.0 parity character
    // =====================================================================

    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F dotColor = {0.0f, 0.0f, 0.0f};
    float totalMag = 0.0f;

    // SB parity: led_share distributes brightness budget across 12 bins.
    // In SB: led_share = 255.0/12.0 = 21.25 (on 0-255 scale).
    // Float equivalent: 1.0/12.0 ≈ 0.0833.
    static constexpr float kLedShare = 1.0f / 12.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        float bin = m_chromaSmooth[c];
        float bright = applyContrast(bin, m_contrast);

        // SB parity: 1.5x brightness boost after contrast squaring
        bright *= 1.5f;
        if (bright > 1.0f) bright = 1.0f;

        // SB parity: no threshold — process all bins (SB doesn't skip near-zero)
        totalMag += bright;

        float prog = c / 12.0f;
        // SB parity: scale by led_share so 12 bins at full = 1.0 total
        CRGB_F noteColor = paletteColorF(ctx.palette, prog, bright * kLedShare);
        dotColor += noteColor;
    }

    // Soft-knee brightness cap (same as K1 — preserves hue ratios)
    if (chromaticMode) {
        float peak = fmaxf(dotColor.r, fmaxf(dotColor.g, dotColor.b));
        if (peak > 1.0f) {
            dotColor *= (1.0f / peak);
        }
    }

    // FAILSAFE: invisible dot prevention (gated by silentScale to prevent
    // firing on AGC-amplified microphone noise during true silence)
    if (totalMag < 0.01f && ctx.audio.rms() > 0.02f
        && ctx.audio.controlBus.silentScale > 0.5f) {
        float fbPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, fbPos, ctx.audio.rms());
        totalMag = ctx.audio.rms();
    }

    // Non-chromatic: single palette colour at hue position
    if (!chromaticMode) {
        float forcedPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, forcedPos, fminf(totalMag * kLedShare, 1.0f));
    }

    // Apply PHOTONS brightness
    float photons = (float)ctx.brightness / 255.0f;
    dotColor *= photons;
    dotColor.clip();

    // SB parity: temporal RGB smoothing (0.05/0.95 EMA)
    // tau = -1 / (120 * ln(0.95)) ≈ 0.163s → ~20-frame inertia at 120 FPS.
    // dt-corrected so frame-rate independent.
    static constexpr float kTauColorSmooth = 0.163f;
    float aColor = 1.0f - expf(-m_dt / kTauColorSmooth);
    m_dotColorSmooth.r += (dotColor.r - m_dotColorSmooth.r) * aColor;
    m_dotColorSmooth.g += (dotColor.g - m_dotColorSmooth.g) * aColor;
    m_dotColorSmooth.b += (dotColor.b - m_dotColorSmooth.b) * aColor;
    dotColor = m_dotColorSmooth;

    // Audio confidence gate: use the novelty-assisted confidence envelope
    // instead of instantaneous RMS.  Confidence has hold (500ms) so it
    // stays at 1.0 through inter-beat gaps — no visual cutting during music.
    const float confidence = ctx.audio.controlBus.audioConfidence;
    const float silScale = ctx.audio.controlBus.silentScale;

    // Scale dot brightness by confidence × silentScale.
    dotColor *= confidence;
    dotColor *= silScale;

    // =====================================================================
    // DYNAMIC TRAIL FADE (dt-corrected) — identical to K1 Waveform
    // =====================================================================

    float absAmp = clampF(fabsf(m_wfPeakScaled), 0.0f, 1.0f);
    static constexpr float kMinDecayRate = 0.8f;
    static constexpr float kDecayScale   = 3.5f;
    float decayRate = kMinDecayRate + kDecayScale * absAmp;

    // Accelerate trail fade only when confidence drops (actual silence),
    // not on inter-beat RMS dips.
    float quietFactor = fminf(confidence, silScale);
    if (quietFactor < 0.9f) {
        decayRate += 10.0f * (1.0f - quietFactor);
    }
    float fade = expf(-decayRate * m_dt);

    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i].r *= fade;
        m_ps->trailBuffer[i].g *= fade;
        m_ps->trailBuffer[i].b *= fade;
    }

    // =====================================================================
    // SHIFT (dt-corrected sub-pixel scroll) — identical to K1 Waveform
    // =====================================================================

    static constexpr float kBaseScrollRate = 150.0f;
    static constexpr float kSpeedMidpoint = 10.0f;
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
    // DOT POSITION — identical to K1 Waveform
    // =====================================================================

    float amp = m_wfPeakLast;
    float safeSensitivity = (m_sensitivity > 0.01f) ? m_sensitivity : 0.01f;
    amp *= 0.7f / safeSensitivity;
    if (amp > 1.0f) amp = 1.0f;
    if (amp < -1.0f) amp = -1.0f;

    float halfLen = static_cast<float>(kStripLength) * 0.5f;
    float posF = halfLen + amp * halfLen;

    // =====================================================================
    // SUB-PIXEL DOT RENDERING — identical to K1 Waveform
    // =====================================================================

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

    // =====================================================================
    // MIRROR right half to left half — identical to K1 Waveform
    // =====================================================================

    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // =====================================================================
    // OUTPUT TO LEDS — identical to K1 Waveform
    // =====================================================================

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

const plugins::EffectMetadata& SbK1WaveformHybridEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Waveform Hybrid",
        "K1 dot mode with SB colour character",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbK1WaveformHybridEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbK1WaveformHybridEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbK1WaveformHybridEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    if (strcmp(name, "sensitivity") == 0) {
        m_sensitivity = clampF(value, 0.01f, 10.0f);
        return true;
    }
    if (strcmp(name, "contrast") == 0) {
        m_contrast = clampF(value, 0.0f, 3.0f);
        return true;
    }
    if (strcmp(name, "chromaHue") == 0) {
        m_chromaHue = clampF(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float SbK1WaveformHybridEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "sensitivity") == 0) return m_sensitivity;
    if (strcmp(name, "contrast") == 0)    return m_contrast;
    if (strcmp(name, "chromaHue") == 0)   return m_chromaHue;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
