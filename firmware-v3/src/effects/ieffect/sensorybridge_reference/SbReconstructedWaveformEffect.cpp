/**
 * @file SbReconstructedWaveformEffect.cpp
 * @brief SB Reconstructed Waveform — spectrally-synthesised oscillating shape
 *
 * Synthesises a waveform-like oscillating pattern from the 64 Goertzel bins.
 * Each bin's smoothed magnitude drives a cosine component at a proportional
 * spatial frequency. The sum of these components produces a smooth, animated
 * pattern that tracks the spectral content of the audio.
 *
 * Algorithm:
 *   1. Temporally smooth bins64 (asymmetric: fast attack 0.3, slow decay 0.05)
 *   2. Advance global phase (drives animation at ~1 Hz base rate)
 *   3. Precompute 16 spatial cosine waveforms (one per bin, outside pixel loop)
 *   4. Per-pixel: accumulate weighted bin contributions, temporal-blend with previous frame
 *   5. Colour from chromagram palette (SbK1BaseEffect), brightness from waveform amplitude
 *   6. Centre-origin mirror + strip copy
 *
 * Performance: 16 bins x 80 pixels = 1280 cosf calls at ~0.1us each = ~128us.
 * Well within the 2.0ms budget.
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
// Parameter descriptors
// ---------------------------------------------------------------------------

const plugins::EffectParameter SbReconstructedWaveformEffect::s_params[kParamCount] = {
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

bool SbReconstructedWaveformEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
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

    // Reset parameters to defaults
    m_chromaHue = 0.0f;
    m_contrast  = 1.0f;

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
    // No audio: fade to black gracefully
    if (!ctx.audio.available) {
        for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
            ctx.leds[i].fadeToBlackBy(20);
        }
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    const float dt = m_dt;

    // =================================================================
    // Stage 1: Temporally smooth bins64 (asymmetric EMA)
    // Fast attack (0.3) tracks transients, slow decay (0.05) preserves
    // spectral character between beats.
    // =================================================================
    for (uint8_t i = 0; i < kNumBins; ++i) {
        const float target = ctx.audio.controlBus.bins64[i];
        const float alpha = (target > m_ps->smoothedBins[i]) ? 0.3f : 0.05f;
        m_ps->smoothedBins[i] += alpha * (target - m_ps->smoothedBins[i]);
    }

    // =================================================================
    // Stage 2: Advance global phase accumulator
    // Base rate ~1 Hz (2*pi per second). Speed parameter scales rate.
    // =================================================================
    static constexpr float kBasePhaseRate = 6.2832f; // 2*pi per second
    static constexpr float kSpeedMidpoint = 10.0f;
    const float phaseRate = kBasePhaseRate * (static_cast<float>(ctx.speed) / kSpeedMidpoint);
    m_ps->phase += dt * phaseRate;
    if (m_ps->phase > 6.2832f) m_ps->phase -= 6.2832f;

    // =================================================================
    // Stage 3: Precompute spatial cosine waveforms
    // For each of the top 16 bins, compute the cosine value at each of
    // the 80 pixel positions. This keeps the cosf calls outside the
    // per-pixel accumulation loop (cache-friendly access pattern).
    //
    // Stack: 16 * 80 * 4 = 5120 bytes — within budget.
    // =================================================================
    float binWaves[kSynthBins][kHalfLength];
    for (uint8_t b = 0; b < kSynthBins; ++b) {
        const float spatialFreq = static_cast<float>(b + 1) * 0.5f;
        for (uint16_t p = 0; p < kHalfLength; ++p) {
            const float progress = static_cast<float>(p) / static_cast<float>(kHalfLength - 1);
            binWaves[b][p] = cosf(m_ps->phase + progress * spatialFreq * 6.2832f);
        }
    }

    // =================================================================
    // Stage 4: Colour synthesis (palette-based, from SbK1BaseEffect)
    // Compute a single dotColor for the frame from chromagram state.
    // =================================================================
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F dotColor = {0.0f, 0.0f, 0.0f};
    float totalMag = 0.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        const float bin = m_chromaSmooth[c];
        float bright = applyContrast(bin, m_contrast);

        if (bright > 0.05f) {
            const float prog = c / 12.0f;
            CRGB_F noteColor = paletteColorF(ctx.palette, prog, bright);
            dotColor += noteColor;
            totalMag += bright;
        }
    }

    // Soft-knee brightness cap (preserves hue ratios)
    if (chromaticMode) {
        const float peak = fmaxf(dotColor.r, fmaxf(dotColor.g, dotColor.b));
        if (peak > 1.0f) {
            dotColor *= (1.0f / peak);
        }
    }

    // Non-chromatic: force palette position to chroma hue + auto-shifted position
    if (!chromaticMode) {
        const float forcedPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, forcedPos, fminf(totalMag, 1.0f));
    }

    // Failsafe: invisible dot prevention
    if (totalMag < 0.01f && ctx.audio.rms() > 0.02f) {
        const float fbPos = m_chromaHue + m_huePosition;
        dotColor = paletteColorF(ctx.palette, fbPos, ctx.audio.rms());
    }

    // Apply PHOTONS brightness
    const float photons = static_cast<float>(ctx.brightness) / 255.0f;
    dotColor *= photons;
    dotColor.clip();

    // =================================================================
    // Stage 5: Per-pixel waveform synthesis + temporal blend
    // Accumulate weighted cosine contributions from each bin, then
    // blend with previous frame for smooth transitions.
    // =================================================================
    for (uint16_t pixel = 0; pixel < kHalfLength; ++pixel) {
        // Additive synthesis: sum smoothed bin magnitudes * precomputed cosines
        float val = 0.0f;
        for (uint8_t b = 0; b < kSynthBins; ++b) {
            val += m_ps->smoothedBins[b] * binWaves[b][pixel];
        }

        // Normalise by number of synthesis components
        val /= static_cast<float>(kSynthBins);

        // Temporal blend with previous frame (smooths transitions)
        m_ps->prevPixels[pixel] += 0.4f * (val - m_ps->prevPixels[pixel]);

        // Brightness from absolute envelope
        float brightness = fabsf(m_ps->prevPixels[pixel]);
        if (brightness > 1.0f) brightness = 1.0f;

        // Write pixel: right half (centre-origin outward)
        const uint16_t ledIdx = kCenterRight + pixel;
        if (ledIdx < ctx.ledCount) {
            ctx.leds[ledIdx] = CRGB(
                static_cast<uint8_t>(clampF(dotColor.r * brightness * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(clampF(dotColor.g * brightness * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(clampF(dotColor.b * brightness * 255.0f, 0.0f, 255.0f))
            );
        }
    }

    // =================================================================
    // Stage 6: Mirror right half to left half (centre-origin)
    // =================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        const uint16_t rightIdx = kCenterRight + i;
        const uint16_t leftIdx  = kCenterLeft - i;
        if (rightIdx < ctx.ledCount && leftIdx < ctx.ledCount) {
            ctx.leds[leftIdx] = ctx.leds[rightIdx];
        }
    }

    // =================================================================
    // Stage 7: Copy strip 1 to strip 2
    // =================================================================
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
        "SB Reconstructed Waveform",
        "Spectrally-synthesised waveform from Goertzel bins (additive cosine synthesis)",
        plugins::EffectCategory::PARTY,
        1,
        "SB.Waveform"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbReconstructedWaveformEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbReconstructedWaveformEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbReconstructedWaveformEffect::setParameter(const char* name, float value) {
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

float SbReconstructedWaveformEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "chromaHue") == 0) return m_chromaHue;
    if (strcmp(name, "contrast") == 0)  return m_contrast;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
