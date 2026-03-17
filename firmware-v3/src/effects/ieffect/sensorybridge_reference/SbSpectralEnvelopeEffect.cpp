/**
 * @file SbSpectralEnvelopeEffect.cpp
 * @brief SB Spectral Envelope — sparse anchor dots with outward scroll
 *
 * Contract: "When bass hits, the centre glows. When treble hits, the edges glow."
 *
 * Writes ONLY 8 sparse anchor dots (2 pixels wide each, ~20% of strip) per
 * frame. The trail buffer provides temporal history via audio-coupled decay
 * and outward scroll. This replaces the previous Hermite interpolation
 * approach that wrote all 80 pixels per frame, producing a saturated blob.
 *
 * Pipeline per frame:
 *   1. Decay trail buffer (audio-coupled: louder = faster fade)
 *   2. Scroll trail outward at ~60 px/s (slower than K1 Waveform's 150)
 *   3. Write 8 sparse anchor dots from bands[0..7] via HSV
 *   4. Mirror right half → left half (centre-origin)
 *   5. Output trail buffer to LEDs + copy strip 2
 */

#include "SbSpectralEnvelopeEffect.h"

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

const plugins::EffectParameter SbSpectralEnvelopeEffect::s_params[kParamCount] = {
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

bool SbSpectralEnvelopeEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbSpecEnvPsram*>(
            heap_caps_malloc(sizeof(SbSpecEnvPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbSpecEnvPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero PSRAM state (trail buffer + scroll accumulator)
    memset(m_ps, 0, sizeof(SbSpecEnvPsram));

    // Reset parameters to defaults
    m_contrast  = 1.0f;
    m_chromaHue = 0.0f;

    (void)ctx;
    return true;
}

void SbSpectralEnvelopeEffect::cleanup() {
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
// Render — sparse anchor dots with outward scroll
// ---------------------------------------------------------------------------

void SbSpectralEnvelopeEffect::renderEffect(plugins::EffectContext& ctx) {
    if (!m_ps) return;

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        // No audio: gentle decay and output
        fadeToBlackBy(m_ps->trailBuffer, kStripLength, 16);
        memcpy(ctx.leds, m_ps->trailBuffer,
               sizeof(CRGB) * (kStripLength < ctx.ledCount ? kStripLength : ctx.ledCount));
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    const float dt = m_dt;

    // =================================================================
    // Stage 1: Decay trail buffer (audio-coupled)
    // Louder audio = faster fade, keeping trails tight during loud
    // passages and lingering during quiet ones.
    // =================================================================
    {
        float rms = ctx.audio.rms();
        float decayRate = 3.0f + 12.0f * rms;
        uint8_t fadeAmt = static_cast<uint8_t>(fminf(decayRate * dt * 255.0f, 200.0f));
        if (fadeAmt < 1) fadeAmt = 1;
        fadeToBlackBy(m_ps->trailBuffer, kStripLength, fadeAmt);
    }

    // =================================================================
    // Stage 2: Scroll trail outward from centre (~60 px/s)
    // Old anchor readings drift outward, creating visible temporal
    // history as trails. Slower than K1 Waveform (150 px/s) so the
    // spectral envelope reads as a spatial map, not a rapid stream.
    // =================================================================
    m_ps->scrollAccum += kScrollRate * dt;
    int pixelsToScroll = static_cast<int>(m_ps->scrollAccum);
    m_ps->scrollAccum -= static_cast<float>(pixelsToScroll);

    for (int s = 0; s < pixelsToScroll; ++s) {
        for (int i = 159; i > 0; --i) {
            m_ps->trailBuffer[i] = m_ps->trailBuffer[i - 1];
        }
        m_ps->trailBuffer[0] = CRGB::Black;
    }

    // =================================================================
    // Stage 3: Write 8 SPARSE anchor dots (not all 80 pixels)
    // Each band maps to a fixed position in the right half. Band 0
    // (sub-bass) sits at the centre, band 7 (air) at the edge.
    // Two pixels wide per dot for sub-pixel coverage.
    // =================================================================
    for (uint8_t i = 0; i < kBandCount; ++i) {
        float energy = ctx.audio.controlBus.bands[i];
        if (energy < 0.02f) continue;  // Skip silent bands

        // Apply contrast curve for perceptual shaping
        energy = applyContrast(energy, m_contrast);

        // HSV colour: hue spreads across bands, shifts with gHue
        uint8_t hue = static_cast<uint8_t>(i * 32 + ctx.gHue);
        uint8_t val = static_cast<uint8_t>(clampF(energy * 255.0f, 0.0f, 255.0f));

        CRGB dotColour;
        hsv2rgb_rainbow(CHSV(hue, ctx.saturation, val), dotColour);
        dotColour.nscale8(ctx.brightness);

        // Place dot at anchor position in right half (2 pixels wide)
        uint16_t pos = kCenterRight + kAnchors[i];
        if (pos < kStripLength) {
            m_ps->trailBuffer[pos] += dotColour;
        }
        if (pos + 1 < kStripLength) {
            m_ps->trailBuffer[pos + 1] += dotColour;
        }
    }

    // =================================================================
    // Stage 4: Mirror right half → left half in trail buffer
    // =================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // =================================================================
    // Stage 5: Output trail buffer to LEDs + copy strip 1 → strip 2
    // =================================================================
    memcpy(ctx.leds, m_ps->trailBuffer,
           sizeof(CRGB) * (kStripLength < ctx.ledCount ? kStripLength : ctx.ledCount));

    for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
        ctx.leds[kStripLength + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbSpectralEnvelopeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Spectral Envelope",
        "Octave bands mapped to pixel positions, bass at centre, treble at edges",
        plugins::EffectCategory::PARTY,
        1,
        "SpectraSynq"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbSpectralEnvelopeEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbSpectralEnvelopeEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbSpectralEnvelopeEffect::setParameter(const char* name, float value) {
    if (!name) return false;

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

float SbSpectralEnvelopeEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "contrast") == 0)    return m_contrast;
    if (strcmp(name, "chromaHue") == 0)   return m_chromaHue;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
