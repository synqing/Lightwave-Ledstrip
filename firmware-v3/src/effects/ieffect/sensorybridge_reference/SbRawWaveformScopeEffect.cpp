/**
 * @file SbRawWaveformScopeEffect.cpp
 * @brief SB Raw Waveform Scope — time-domain oscilloscope with aggressive processing
 *
 * Aggressively processes the activity-gated waveform data with 8-pass
 * bidirectional low-pass filtering, asymmetric peak following, and temporal
 * smoothing to extract a clean oscilloscope display from attenuated samples.
 *
 * See header for full algorithm description.
 */

#include "SbRawWaveformScopeEffect.h"

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

const plugins::EffectParameter SbRawWaveformScopeEffect::s_params[kParamCount] = {
    {"chromaHue", "Hue Offset", 0.0f, 1.0f, 0.0f, plugins::EffectParameterType::FLOAT, 0.01f, "colour", "", false},
    {"contrast", "Contrast", 0.0f, 3.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.25f, "visual", "x", false},
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

bool SbRawWaveformScopeEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbRawWfScopePsram*>(
            heap_caps_malloc(sizeof(SbRawWfScopePsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbRawWfScopePsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero all PSRAM state
    memset(m_ps, 0, sizeof(SbRawWfScopePsram));

    // Reset parameters to defaults
    m_chromaHue = 0.0f;
    m_contrast  = 1.0f;

    (void)ctx;
    return true;
}

void SbRawWaveformScopeEffect::cleanup() {
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

void SbRawWaveformScopeEffect::renderEffect(plugins::EffectContext& ctx) {
    if (!m_ps) return;

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    // No audio: fade to black
    if (!ctx.audio.available) {
        for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
            ctx.leds[i].fadeToBlackBy(20);
        }
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    const float photons = static_cast<float>(ctx.brightness) / 255.0f;

    // =====================================================================
    // Stage 1: Read waveform — normalise int16_t to float [-1, 1]
    // =====================================================================
    for (uint8_t i = 0; i < kWfPoints; ++i) {
        m_ps->samples[i] = static_cast<float>(ctx.audio.controlBus.waveform[i]) / 32768.0f;
    }

    // =====================================================================
    // Stage 2: Compute RMS of current frame (for peak follower input)
    // =====================================================================
    float frameRms = 0.0f;
    for (uint8_t i = 0; i < kWfPoints; ++i) {
        frameRms += m_ps->samples[i] * m_ps->samples[i];
    }
    frameRms = sqrtf(frameRms / static_cast<float>(kWfPoints));

    // =====================================================================
    // Stage 3: 8-pass bidirectional low-pass filter (very aggressive)
    // 4 forward + 4 reverse passes at alpha=0.25 produce extremely smooth
    // bass/mid-frequency envelope from the noisy activity-gated waveform.
    // Bidirectional passes prevent phase shift (symmetric filtering).
    // =====================================================================
    static constexpr float kLpfAlpha = 0.25f;
    static constexpr uint8_t kFilterPasses = 4; // 4 forward+reverse = 8 total

    for (uint8_t pass = 0; pass < kFilterPasses; ++pass) {
        // Forward pass
        float y = m_ps->samples[0];
        for (uint8_t i = 0; i < kWfPoints; ++i) {
            y += kLpfAlpha * (m_ps->samples[i] - y);
            m_ps->samples[i] = y;
        }
        // Reverse pass
        y = m_ps->samples[kWfPoints - 1];
        for (int16_t i = kWfPoints - 1; i >= 0; --i) {
            y += kLpfAlpha * (m_ps->samples[i] - y);
            m_ps->samples[i] = y;
        }
    }

    // =====================================================================
    // Stage 4: Auto-scale with smoothed peak follower
    // Asymmetric: fast attack (0.3) tracks loud passages immediately,
    // slow release (0.01) avoids amplifying noise during quiet passages.
    // =====================================================================
    float maxAbs = 0.0f;
    for (uint8_t i = 0; i < kWfPoints; ++i) {
        const float a = fabsf(m_ps->samples[i]);
        if (a > maxAbs) maxAbs = a;
    }

    static constexpr float kPeakAttackAlpha  = 0.3f;
    static constexpr float kPeakReleaseAlpha = 0.01f;

    if (maxAbs > m_ps->smoothedPeak) {
        m_ps->smoothedPeak += kPeakAttackAlpha * (maxAbs - m_ps->smoothedPeak);
    } else {
        m_ps->smoothedPeak += kPeakReleaseAlpha * (maxAbs - m_ps->smoothedPeak);
    }

    // Floor: prevent division by zero / extreme amplification
    if (m_ps->smoothedPeak < 0.0001f) m_ps->smoothedPeak = 0.0001f;

    const float autoScale = 1.0f / m_ps->smoothedPeak;

    // =====================================================================
    // Stage 5: Colour synthesis (palette-based, from SbK1WaveformEffect)
    // Uses additive chromagram accumulation with soft-knee cap.
    // =====================================================================
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F dotColor = {0.0f, 0.0f, 0.0f};
    float totalMag = 0.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        float bin = m_chromaSmooth[c];
        float bright = applyContrast(bin, m_contrast);
        if (bright > 1.0f) bright = 1.0f;

        if (bright > 0.05f) {
            float prog = c / 12.0f;
            CRGB_F noteColor = paletteColorF(ctx.palette, prog, bright);
            dotColor += noteColor;
            totalMag += bright;
        }
    }

    // Soft-knee brightness cap: scale so brightest channel <= 1.0
    // Preserves hue ratios (unlike hard clip which desaturates to white)
    if (chromaticMode) {
        float peak = fmaxf(dotColor.r, fmaxf(dotColor.g, dotColor.b));
        if (peak > 1.0f) {
            dotColor *= (1.0f / peak);
        }
    }

    // Non-chromatic: force palette position to chroma hue + auto-shifted position
    if (!chromaticMode) {
        float forcedPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, forcedPos, fminf(totalMag, 1.0f));
    }

    // Failsafe: invisible dot prevention
    if (totalMag < 0.01f && ctx.audio.rms() > 0.02f) {
        float fbPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, fbPos, ctx.audio.rms());
    }

    // Apply PHOTONS brightness
    dotColor *= photons;
    dotColor.clip();

    // =====================================================================
    // Stage 6: Interpolate 128 -> 80 pixels + temporal smoothing
    // Linear interpolation maps 128 filtered samples to 80 pixel positions.
    // Temporal smoothing (alpha=0.3) blends with previous frame to reduce
    // flicker without losing transient response.
    // =====================================================================
    static constexpr float kTemporalAlpha = 0.3f;

    for (uint16_t pixel = 0; pixel < kHalfLength; ++pixel) {
        // Progress: 0.0 at centre, 1.0 at edge
        const float progress = (kHalfLength <= 1) ? 0.0f
            : (static_cast<float>(pixel) / static_cast<float>(kHalfLength - 1));

        // Interpolate from 128 filtered samples
        const float srcIdx = progress * 127.0f;
        const int lo = static_cast<int>(srcIdx);
        const float frac = srcIdx - static_cast<float>(lo);
        const int hi = (lo + 1 < kWfPoints) ? (lo + 1) : (kWfPoints - 1);

        float raw = (m_ps->samples[lo] * (1.0f - frac) + m_ps->samples[hi] * frac) * autoScale;
        raw = clampF(raw, -1.0f, 1.0f);

        // Temporal smoothing between frames
        m_ps->prevPixels[pixel] += kTemporalAlpha * (raw - m_ps->prevPixels[pixel]);

        // Envelope mode: absolute value of temporally-smoothed signal
        float val = fabsf(m_ps->prevPixels[pixel]);
        if (val > 1.0f) val = 1.0f;

        // Write pixel: right half (centre-origin outward)
        const uint16_t ledIdx = kCenterRight + pixel;
        if (ledIdx < ctx.ledCount) {
            ctx.leds[ledIdx] = CRGB(
                static_cast<uint8_t>(clampF(dotColor.r * val * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(clampF(dotColor.g * val * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(clampF(dotColor.b * val * 255.0f, 0.0f, 255.0f))
            );
        }
    }

    // =====================================================================
    // Stage 7: Mirror right half to left half (centre-origin)
    // =====================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        const uint16_t rightIdx = kCenterRight + i;
        const uint16_t leftIdx  = kCenterLeft - i;
        if (rightIdx < ctx.ledCount && leftIdx < ctx.ledCount) {
            ctx.leds[leftIdx] = ctx.leds[rightIdx];
        }
    }

    // =====================================================================
    // Stage 8: Copy strip 1 to strip 2
    // =====================================================================
    for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
        ctx.leds[kStripLength + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbRawWaveformScopeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Raw Waveform Scope",
        "Time-domain waveform oscilloscope with aggressive activity-gate compensation",
        plugins::EffectCategory::PARTY,
        1,
        "SB.Waveform"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbRawWaveformScopeEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbRawWaveformScopeEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbRawWaveformScopeEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    if (strcmp(name, "chromaHue") == 0) {
        m_chromaHue = clampF(value, 0.0f, 1.0f);
        return true;
    }
    if (strcmp(name, "contrast") == 0) {
        m_contrast = clampF(value, 0.0f, 3.0f);
        return true;
    }
    return false;
}

float SbRawWaveformScopeEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "chromaHue") == 0) return m_chromaHue;
    if (strcmp(name, "contrast") == 0)  return m_contrast;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
