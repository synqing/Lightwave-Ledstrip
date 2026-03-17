/**
 * @file SbSpectralBeatPulseEffect.cpp
 * @brief SB Spectral Beat Pulse — spectral envelope that pulses outward on beats
 *
 * Algorithm:
 *   1. Smooth 8 octave bands with asymmetric attack/decay
 *   2. Track beat envelope (snap to 1.0 on beat, exponential decay between)
 *   3. Compute expansion factor from beat envelope
 *   4. Per-pixel: map adjusted position through spectral shape, apply smoothstep
 *   5. Colour via palette with hue varying from centre (warm bass) to edge (cool treble)
 *   6. Add bright edge glow at the expansion front on beat
 *   7. Mirror right half to left half, copy strip 1 to strip 2
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

/// Smoothstep contrast curve: t*t*(3 - 2*t)
static inline float smoothstep01(float t) {
    t = clampF(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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
        m_ps = static_cast<SbSpecBeatPsram*>(
            heap_caps_malloc(sizeof(SbSpecBeatPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbSpecBeatPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero PSRAM state
    for (int i = 0; i < 8; ++i) {
        m_ps->smoothedBands[i] = 0.0f;
    }
    m_ps->beatEnvelope = 0.0f;

    // Reset parameters to defaults
    m_contrast  = 1.0f;
    m_chromaHue = 0.0f;

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
        // No audio: fade toward black
        for (uint16_t i = 0; i < kStripLength && i < ctx.ledCount; ++i) {
            ctx.leds[i].fadeToBlackBy(16);
        }
        // Copy strip 1 to strip 2
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    // =====================================================================
    // 1. Smooth band energies (asymmetric attack/decay)
    // =====================================================================
    const float* rawBands = ctx.audio.bands();
    if (rawBands) {
        for (int i = 0; i < 8; ++i) {
            float target = rawBands[i];
            // Rate-independent smoothing: alpha = 1 - exp(-dt / tau)
            // Attack tau ~0.025s (fast), decay tau ~0.14s (slow)
            float tau = (target > m_ps->smoothedBands[i]) ? 0.025f : 0.14f;
            float alpha = 1.0f - expf(-dt / tau);
            m_ps->smoothedBands[i] += alpha * (target - m_ps->smoothedBands[i]);
        }
    }

    // =====================================================================
    // 2. Update beat envelope
    // =====================================================================
    float beatStrength = ctx.audio.beatStrength();

    // On beat: snap envelope toward 1.0 scaled by strength
    if (ctx.audio.isOnBeat()) {
        float snapTarget = clampF(beatStrength, 0.3f, 1.0f);
        if (snapTarget > m_ps->beatEnvelope) {
            m_ps->beatEnvelope = snapTarget;
        }
    }

    // Exponential decay (half-life ~150ms)
    m_ps->beatEnvelope *= expf(-dt / 0.15f);
    if (m_ps->beatEnvelope < 0.01f) m_ps->beatEnvelope = 0.0f;

    // =====================================================================
    // 3. Compute expansion factor
    // =====================================================================
    // 0.5 (resting — inner half lit) to 1.0 (on-beat — full strip)
    float expansion = 0.5f + m_ps->beatEnvelope * 0.5f;

    // =====================================================================
    // 4. Per-pixel rendering (right half: kCenterRight..kCenterRight+79)
    // =====================================================================
    float photons = static_cast<float>(ctx.brightness) / 255.0f;

    for (uint16_t p = 0; p < kHalfLength; ++p) {
        float progress = static_cast<float>(p) / 79.0f; // 0=centre, 1=edge

        // Contract progress toward centre based on expansion
        float adjustedProgress = progress / expansion;

        CRGB pixelColor = CRGB::Black;

        if (adjustedProgress <= 1.0f) {
            // Interpolate band energy at this adjusted position
            float bandPos = adjustedProgress * 7.0f; // Map [0,1] -> [0,7] across 8 bands
            int lo = static_cast<int>(bandPos);
            float frac = bandPos - static_cast<float>(lo);
            int hi = lo + 1;
            if (hi > 7) hi = 7;
            if (lo > 7) lo = 7;

            float energy = m_ps->smoothedBands[lo] * (1.0f - frac)
                         + m_ps->smoothedBands[hi] * frac;

            // Apply contrast via smoothstep
            energy = smoothstep01(energy);

            // Apply user contrast scaling
            if (m_contrast != 1.0f) {
                energy = applyContrast(energy, m_contrast);
            }

            float brightness = clampF(energy, 0.0f, 1.0f);

            // --- Edge glow on beat ---
            // Near the expansion front: add bright white-ish pulse
            if (adjustedProgress > 0.90f && m_ps->beatEnvelope > 0.3f) {
                float edgeFactor = (adjustedProgress - 0.90f) / 0.10f; // 0..1 within edge zone
                float glowStrength = edgeFactor * m_ps->beatEnvelope;
                brightness = fmaxf(brightness, glowStrength);
            }

            // --- Colour: palette with hue varying by position ---
            // Warm bass (centre) to cool treble (edge)
            float huePos = m_chromaHue + m_huePosition + adjustedProgress * 0.78f;
            CRGB_F col = paletteColorF(ctx.palette, huePos, brightness * photons);
            col.clip();
            pixelColor = col.toCRGB();
        }

        // Write to right half (centre outward)
        uint16_t rightIdx = kCenterRight + p;
        if (rightIdx < kStripLength && rightIdx < ctx.ledCount) {
            ctx.leds[rightIdx] = pixelColor;
        }
    }

    // =====================================================================
    // 5. Mirror right half to left half
    // =====================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        uint16_t leftIdx = kCenterLeft - i;
        uint16_t rightIdx = kCenterRight + i;
        if (leftIdx < kStripLength && rightIdx < kStripLength
            && leftIdx < ctx.ledCount && rightIdx < ctx.ledCount) {
            ctx.leds[leftIdx] = ctx.leds[rightIdx];
        }
    }

    // =====================================================================
    // 6. Copy strip 1 to strip 2
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
        "SB Spectral Beat Pulse",
        "Spectral envelope that pulses outward on beats",
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

float SbSpectralBeatPulseEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "contrast") == 0)    return m_contrast;
    if (strcmp(name, "chromaHue") == 0)   return m_chromaHue;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
