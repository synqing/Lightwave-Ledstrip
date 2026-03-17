/**
 * @file SbRawWaveformScopeEffect.cpp
 * @brief SB Spectral Shape — spectral centroid/flatness visualiser
 *
 * Renders spectral shape as a positioned gaussian blob whose width is
 * controlled by spectral flatness: pure tone = tight line, noise = wide band.
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

    const float photons = static_cast<float>(ctx.brightness) / 255.0f;
    const float dt = m_dt;

    // =====================================================================
    // Stage 1: Decay trail buffer (audio-coupled, K1 Waveform pattern)
    // =====================================================================
    {
        float rmsNow = ctx.audio.rms();
        float decayRate = 0.8f + 3.5f * rmsNow;
        uint8_t fadeAmt = static_cast<uint8_t>(fminf(decayRate * dt * 255.0f, 200.0f));
        if (fadeAmt < 1) fadeAmt = 1;
        fadeToBlackBy(m_ps->trailBuffer, kStripLength, fadeAmt);
    }

    // =====================================================================
    // Stage 2: Smooth bins64 temporally (asymmetric: fast attack, slow decay)
    // dt-corrected EMA: alpha = 1 - exp(-dt / tau)
    // Attack tau ~20ms (snappy response), decay tau ~150ms (smooth release)
    // =====================================================================
    static constexpr float kAttackTau = 0.02f;
    static constexpr float kDecayTau  = 0.15f;
    const float alphaAttack = 1.0f - expf(-dt / kAttackTau);
    const float alphaDecay  = 1.0f - expf(-dt / kDecayTau);

    for (uint8_t i = 0; i < kBinCount; ++i) {
        float target = ctx.audio.controlBus.bins64[i];
        float alpha = (target > m_ps->smoothedBins[i]) ? alphaAttack : alphaDecay;
        m_ps->smoothedBins[i] += (target - m_ps->smoothedBins[i]) * alpha;
    }

    // =====================================================================
    // Stage 3: Compute spectral centroid and flatness
    // Centroid = weighted mean bin index (0-63). Tracks perceived pitch centre.
    // Flatness = geometric mean / arithmetic mean of bin energies.
    //   0 = tonal (one dominant frequency), 1 = white noise (flat spectrum).
    // =====================================================================
    float sumEnergy = 0.0f;
    float weightedSum = 0.0f;
    float logSum = 0.0f;
    int logCount = 0;

    for (uint8_t i = 0; i < kBinCount; ++i) {
        float e = m_ps->smoothedBins[i];
        sumEnergy += e;
        weightedSum += static_cast<float>(i) * e;
        if (e > 0.001f) {
            logSum += logf(e);
            logCount++;
        }
    }

    float centroid = (sumEnergy > 0.01f) ? (weightedSum / sumEnergy) : 32.0f;
    float arithMean = sumEnergy / static_cast<float>(kBinCount);
    float flatness = (logCount > 0 && arithMean > 0.001f)
        ? expf(logSum / static_cast<float>(logCount)) / arithMean
        : 0.0f;
    flatness = fminf(flatness, 1.0f);

    // Smooth centroid and flatness (tau ~100ms for stable visual tracking)
    static constexpr float kFeatureTau = 0.1f;
    float alphaFeature = 1.0f - expf(-dt / kFeatureTau);
    m_ps->smoothedCentroid += (centroid - m_ps->smoothedCentroid) * alphaFeature;
    m_ps->smoothedFlatness += (flatness - m_ps->smoothedFlatness) * alphaFeature;

    // =====================================================================
    // Stage 4: Colour synthesis (palette-based chromagram accumulation)
    // Same pattern as K1 Waveform: additive chromagram with soft-knee cap.
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

    // Soft-knee brightness cap: preserve hue ratios
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
    // Stage 5: Gaussian blob injection at centroid position
    // Centroid (0-63) maps to pixel position (0-79) in the right half.
    // Flatness controls blob width: tonal (~0) = 2px, noisy (~1) = 10px.
    // Energy (sumEnergy) scales overall blob brightness.
    // =====================================================================
    float centroidPx = m_ps->smoothedCentroid * 79.0f / 63.0f;
    float width = 2.0f + m_ps->smoothedFlatness * 8.0f;

    // Scale blob brightness by total spectral energy (prevents phantom blobs in silence)
    float energyScale = fminf(sumEnergy * 4.0f, 1.0f);

    for (uint16_t p = 0; p < kHalfLength; ++p) {
        float dist = fabsf(static_cast<float>(p) - centroidPx);
        // Skip pixels far outside the gaussian bell (>2 sigma)
        if (dist > width * 2.0f) continue;
        float gauss = expf(-(dist * dist) / (2.0f * width * width));
        if (gauss < 0.05f) continue;

        float intensity = gauss * energyScale;
        CRGB scaled = CRGB(
            static_cast<uint8_t>(clampF(dotColor.r * intensity * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(clampF(dotColor.g * intensity * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(clampF(dotColor.b * intensity * 255.0f, 0.0f, 255.0f))
        );

        // Additive blend into trail buffer (right half, centre-origin outward)
        uint16_t ledIdx = kCenterRight + p;
        if (ledIdx < kStripLength) {
            m_ps->trailBuffer[ledIdx] += scaled;
        }
    }

    // =====================================================================
    // Stage 6: Mirror right half to left half IN the trail buffer
    // =====================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // =====================================================================
    // Output trail buffer to LEDs
    // =====================================================================
    for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
        ctx.leds[i] = m_ps->trailBuffer[i];
    }

    // =====================================================================
    // Stage 7: Copy strip 1 to strip 2
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
        "SB Spectral Shape",
        "Spectral centroid/flatness visualiser: tone=sharp line, noise=wide shimmer",
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
