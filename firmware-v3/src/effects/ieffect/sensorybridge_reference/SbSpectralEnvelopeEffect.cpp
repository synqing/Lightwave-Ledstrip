/**
 * @file SbSpectralEnvelopeEffect.cpp
 * @brief SB Spectral Envelope — octave bands mapped to pixel positions
 *
 * Maps 8 ControlBus octave energy bands (sub-bass through air) to spatial
 * positions along the LED strip. Bass sits at the centre, treble at the
 * edges. Smooth Hermite interpolation fills the gaps between the 8 anchor
 * points. Colour comes from the palette via the SbK1BaseEffect chromagram
 * pipeline, producing a musically coherent spectral visualiser.
 *
 * Algorithm:
 *   1. Read bands[0..7] from ControlBus (already smoothed, 0-1)
 *   2. For each of 80 right-half pixels, Hermite-interpolate between the
 *      two nearest band anchors to get energy
 *   3. Apply smoothstep contrast curve to the interpolated energy
 *   4. Look up palette colour at (huePosition + pixel/80) scaled by energy
 *   5. Apply photons brightness
 *   6. Mirror right half to left half (centre-origin)
 *   7. Copy strip 1 to strip 2
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
// Smooth Hermite interpolation between band anchor points
// ---------------------------------------------------------------------------

float SbSpectralEnvelopeEffect::interpolateBands(const float* bands, float pixelPos) {
    // Clamp pixel position to valid range
    if (pixelPos <= kBandPositions[0]) return bands[0];
    if (pixelPos >= kBandPositions[kBandCount - 1]) return bands[kBandCount - 1];

    // Find the segment containing this pixel position
    uint8_t seg = 0;
    for (uint8_t i = 1; i < kBandCount; ++i) {
        if (pixelPos < kBandPositions[i]) {
            seg = i - 1;
            break;
        }
    }

    // Normalised position within this segment [0, 1]
    float segLen = kBandPositions[seg + 1] - kBandPositions[seg];
    float t = (segLen > 0.001f) ? (pixelPos - kBandPositions[seg]) / segLen : 0.0f;

    // Catmull-Rom style tangent estimation for smooth cubic interpolation.
    // For boundary points, use the nearest available difference.
    float p0 = bands[seg];
    float p1 = bands[seg + 1];

    // Tangent at p0 (central difference, or forward difference at boundary)
    float m0;
    if (seg > 0) {
        float dPrev = kBandPositions[seg] - kBandPositions[seg - 1];
        float dCurr = segLen;
        // Weighted average of slopes on either side
        m0 = 0.5f * ((bands[seg] - bands[seg - 1]) / (dPrev > 0.001f ? dPrev : 1.0f) * dCurr
                    + (p1 - p0) / (dCurr > 0.001f ? dCurr : 1.0f) * dCurr);
    } else {
        m0 = (p1 - p0);  // Forward difference scaled by segment length
    }

    // Tangent at p1
    float m1;
    if (seg + 2 < kBandCount) {
        float dCurr = segLen;
        float dNext = kBandPositions[seg + 2] - kBandPositions[seg + 1];
        m1 = 0.5f * ((p1 - p0) / (dCurr > 0.001f ? dCurr : 1.0f) * dCurr
                    + (bands[seg + 2] - p1) / (dNext > 0.001f ? dNext : 1.0f) * dCurr);
    } else {
        m1 = (p1 - p0);  // Backward difference scaled by segment length
    }

    // Hermite basis functions
    float t2 = t * t;
    float t3 = t2 * t;
    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;

    float result = h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
    return clampF(result, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool SbSpectralEnvelopeEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
    if (!baseInit()) return false;

    // Reset parameters to defaults
    m_contrast  = 1.0f;
    m_chromaHue = 0.0f;

    (void)ctx;
    return true;
}

void SbSpectralEnvelopeEffect::cleanup() {
    baseCleanup();
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void SbSpectralEnvelopeEffect::renderEffect(plugins::EffectContext& ctx) {
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        // No audio: fade to black gracefully
        for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
            ctx.leds[i].fadeToBlackBy(16);
        }
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    // Read the 8 octave energy bands from the ControlBus
    const float* bands = ctx.audio.controlBus.bands;

    // Photons brightness scaling
    const float photons = static_cast<float>(ctx.brightness) / 255.0f;

    // --- PER-PIXEL RENDERING (right half: pixels 80..159, centre-origin) ---
    for (uint16_t px = 0; px < kHalfLength; ++px) {
        const float pixelPos = static_cast<float>(px);

        // Interpolate band energy at this spatial position
        float energy = interpolateBands(bands, pixelPos);

        // Apply configurable contrast via the base class K1 contrast curve
        energy = applyContrast(energy, m_contrast);

        // Smoothstep for perceptual contrast shaping
        energy = energy * energy * (3.0f - 2.0f * energy);

        // Palette colour: position shifts with hue + spatial gradient
        float palettePos = m_chromaHue + m_huePosition + static_cast<float>(px) / static_cast<float>(kHalfLength);

        // Get colour from palette at this position, scaled by energy
        CRGB_F colour = paletteColorF(ctx.palette, palettePos, energy);

        // Apply photons brightness
        colour *= photons;
        colour.clip();

        // Write to right half (centre-origin: pixel 80 + px)
        const uint16_t rightIdx = kCenterRight + px;
        if (rightIdx < ctx.ledCount) {
            ctx.leds[rightIdx] = colour.toCRGB();
        }
    }

    // --- MIRROR right half to left half ---
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        const uint16_t leftIdx  = kCenterLeft - i;
        const uint16_t rightIdx = kCenterRight + i;
        if (rightIdx < ctx.ledCount && leftIdx < ctx.ledCount) {
            ctx.leds[leftIdx] = ctx.leds[rightIdx];
        }
    }

    // --- COPY strip 1 to strip 2 ---
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
