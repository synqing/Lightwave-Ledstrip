/**
 * @file SbSpectralBeatPulseEffect.cpp
 * @brief SB Onset Map — percussive transient mapper
 *
 * Algorithm:
 *   1. Decay trail buffer (fast, RMS-coupled for percussive snap)
 *   2. Update onset envelopes: snare (~120ms tau), hi-hat (~80ms tau)
 *   3. Draw sparse onset flashes at mapped spatial positions:
 *      - Snare: Gaussian blob near centre (~pixel 20 from centre, ~20px wide)
 *      - Hi-hat: Gaussian blob near edges (~pixel 68 from centre, ~10px wide)
 *   4. Add dim ambient flux shimmer at centre point between onsets
 *   5. Mirror right half to left half, copy strip 1 to strip 2
 *
 * Crossmodal: snare (low freq) = warm/central/large, hi-hat (high freq) = cool/peripheral/small.
 */

#include "SbSpectralBeatPulseEffect.h"

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

const plugins::EffectParameter SbSpectralBeatPulseEffect::s_params[kParamCount] = {
    {"sensitivity", "Sensitivity", 0.1f, 3.0f, 1.0f, plugins::EffectParameterType::FLOAT, 0.1f, "audio", "x", false},
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

bool SbSpectralBeatPulseEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbOnsetMapPsram*>(
            heap_caps_malloc(sizeof(SbOnsetMapPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbOnsetMapPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero all PSRAM state (trail buffer, envelopes, flux)
    memset(m_ps, 0, sizeof(SbOnsetMapPsram));

    // Reset parameters to defaults
    m_sensitivity = 1.0f;
    m_chromaHue   = 0.0f;

    (void)ctx;
    return true;
}

void SbSpectralBeatPulseEffect::cleanup() {
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

void SbSpectralBeatPulseEffect::renderEffect(plugins::EffectContext& ctx) {
    if (!m_ps) return;

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    const float dt = ctx.getSafeDeltaSeconds();

    if (!ctx.audio.available) {
        // No audio: decay trail buffer toward black
        fadeToBlackBy(m_ps->trailBuffer, kStripLength, 16);
        for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
            ctx.leds[i] = m_ps->trailBuffer[i];
        }
        // Copy strip 1 to strip 2
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    // =====================================================================
    // 1. Decay trail buffer (fast, RMS-coupled for percussive snap)
    // =====================================================================
    {
        float rmsNow = ctx.audio.rms();
        float decayRate = 5.0f + 18.0f * rmsNow;
        uint8_t fadeAmt = static_cast<uint8_t>(fminf(decayRate * dt * 255.0f, 220.0f));
        if (fadeAmt < 1) fadeAmt = 1;
        fadeToBlackBy(m_ps->trailBuffer, kStripLength, fadeAmt);
    }

    // =====================================================================
    // 2. Update onset envelopes
    // =====================================================================

    // Exponential decay of existing envelopes (rate-independent)
    m_ps->snareDecay *= expf(-dt / 0.05f);  // ~120ms tau for snare
    m_ps->hihatDecay *= expf(-dt / 0.03f);  // ~80ms tau for hi-hat (faster)

    if (m_ps->snareDecay < 0.005f) m_ps->snareDecay = 0.0f;
    if (m_ps->hihatDecay < 0.005f) m_ps->hihatDecay = 0.0f;

    // Snare trigger: snap envelope to energy level (scaled by sensitivity)
    if (ctx.audio.isSnareHit()) {
        float energy = clampF(ctx.audio.snare() * m_sensitivity, 0.0f, 1.0f);
        m_ps->snareDecay = fmaxf(m_ps->snareDecay, energy);
    }

    // Hi-hat trigger: snap envelope to energy level
    if (ctx.audio.isHihatHit()) {
        float energy = clampF(ctx.audio.hihat() * m_sensitivity, 0.0f, 1.0f);
        m_ps->hihatDecay = fmaxf(m_ps->hihatDecay, energy);
    }

    // Ambient flux: smooth follower of spectral flux for inter-onset shimmer
    {
        float rawFlux = ctx.audio.fastFlux();
        float aFlux = 1.0f - expf(-dt / 0.2f);
        m_ps->fluxSmoothed += (rawFlux - m_ps->fluxSmoothed) * aFlux;
    }

    // =====================================================================
    // 3. Draw SPARSE onset flashes (right half only, mirrored later)
    // =====================================================================

    // --- Snare flash: warm colour, centred at pixel 20 from centre, width ~20px ---
    if (m_ps->snareDecay > 0.02f) {
        uint8_t bright = static_cast<uint8_t>(clampF(m_ps->snareDecay * 255.0f, 0.0f, 255.0f));

        // Warm hue offset (+10 from gHue)
        CRGB snareColor;
        hsv2rgb_rainbow(CHSV(ctx.gHue + 10, ctx.saturation, bright), snareColor);
        snareColor.nscale8(ctx.brightness);

        // Gaussian blob: centre at pixel 20, sigma ~5px
        // Loop range [10, 35) covers ~3 sigma each side
        for (int p = 10; p < 35; ++p) {
            float dist = fabsf(static_cast<float>(p) - 20.0f);
            float gauss = expf(-(dist * dist) / 50.0f);  // sigma^2 = 25, denom = 2*sigma^2
            if (gauss < 0.05f) continue;

            CRGB scaled = snareColor;
            scaled.nscale8(static_cast<uint8_t>(gauss * 255.0f));

            uint16_t idx = kCenterRight + static_cast<uint16_t>(p);
            if (idx < kStripLength) {
                m_ps->trailBuffer[idx] += scaled;
            }
        }
    }

    // --- Hi-hat flash: cool colour, centred at pixel 68 from centre, width ~10px ---
    if (m_ps->hihatDecay > 0.02f) {
        uint8_t bright = static_cast<uint8_t>(clampF(m_ps->hihatDecay * 255.0f, 0.0f, 255.0f));

        // Cool hue offset (+160 from gHue)
        CRGB hihatColor;
        hsv2rgb_rainbow(CHSV(ctx.gHue + 160, ctx.saturation, bright), hihatColor);
        hihatColor.nscale8(ctx.brightness);

        // Gaussian blob: centre at pixel 68, sigma ~3px
        // Loop range [60, 79) covers ~3 sigma each side
        for (int p = 60; p < 79; ++p) {
            float dist = fabsf(static_cast<float>(p) - 68.0f);
            float gauss = expf(-(dist * dist) / 18.0f);  // sigma^2 = 9, denom = 2*sigma^2
            if (gauss < 0.05f) continue;

            CRGB scaled = hihatColor;
            scaled.nscale8(static_cast<uint8_t>(gauss * 255.0f));

            uint16_t idx = kCenterRight + static_cast<uint16_t>(p);
            if (idx < kStripLength) {
                m_ps->trailBuffer[idx] += scaled;
            }
        }
    }

    // --- Ambient flux shimmer (very dim, between onsets) ---
    if (m_ps->fluxSmoothed > 0.02f) {
        uint8_t fluxBright = static_cast<uint8_t>(clampF(m_ps->fluxSmoothed * 40.0f, 0.0f, 40.0f));
        if (fluxBright > 0) {
            CRGB dimGlow;
            hsv2rgb_rainbow(CHSV(ctx.gHue, ctx.saturation, fluxBright), dimGlow);
            dimGlow.nscale8(ctx.brightness);
            m_ps->trailBuffer[kCenterRight] += dimGlow;
        }
    }

    // =====================================================================
    // 4. Mirror right half to left half IN the trail buffer
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
    // 5. Copy strip 1 to strip 2
    // =====================================================================
    for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
        ctx.leds[kStripLength + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbSpectralBeatPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Onset Map",
        "Percussive transient mapper: snare near centre, hi-hat at edges",
        plugins::EffectCategory::PARTY,
        1,
        "SpectraSynq"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbSpectralBeatPulseEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbSpectralBeatPulseEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbSpectralBeatPulseEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    if (strcmp(name, "sensitivity") == 0) {
        m_sensitivity = clampF(value, 0.1f, 3.0f);
        return true;
    }
    if (strcmp(name, "chromaHue") == 0) {
        m_chromaHue = clampF(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float SbSpectralBeatPulseEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "sensitivity") == 0) return m_sensitivity;
    if (strcmp(name, "chromaHue") == 0)   return m_chromaHue;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
