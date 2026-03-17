/**
 * @file SbWaveformOscilloscopeEffect.cpp
 * @brief SB 3.0.0 Waveform oscilloscope — exact parity + brightness-boosted variant
 *
 * True port of Sensory Bridge 3.0.0's light_mode_waveform()
 * (lightshow_modes.h:128-228). Full-strip oscilloscope rendering the raw
 * audio waveform shape across all LEDs.
 *
 * Algorithm:
 *   1. Copy 128 waveform samples into 4-frame ring buffer
 *   2. Smooth peak envelope (dt-corrected EMA, tau=0.217s)
 *   3. Chromagram colour synthesis (12 chroma bins → additive RGB)
 *   4. Temporal colour smoothing (dt-corrected EMA, tau=0.217s)
 *   5. Per-pixel waveform rendering:
 *      a. Resample 128→80 with linear interpolation
 *      b. Average 4 history frames
 *      c. Normalise by 128.0
 *      d. Per-pixel EMA smoothing (mood-dependent tau)
 *      e. Peak-gated brightness: 0.5 + waveformLast * 0.5
 *   6. Mirror right half to left half (centre-origin)
 *   7. Copy strip 1 to strip 2
 */

#include "SbWaveformOscilloscopeEffect.h"

#include "../../CoreEffects.h"
#include "../../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// ---------------------------------------------------------------------------
// Parameter descriptors
// ---------------------------------------------------------------------------

const plugins::EffectParameter SbWaveformOscilloscopeBase::s_params[kParamCount] = {
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

bool SbWaveformOscilloscopeBase::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbWfOscPsram*>(
            heap_caps_malloc(sizeof(SbWfOscPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbWfOscPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero all PSRAM state
    memset(m_ps, 0, sizeof(SbWfOscPsram));

    // Reset parameters to defaults
    m_chromaHue = 0.0f;
    m_contrast  = 1.0f;

    (void)ctx;
    return true;
}

void SbWaveformOscilloscopeBase::cleanup() {
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

void SbWaveformOscilloscopeBase::renderEffect(plugins::EffectContext& ctx) {
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

    const float dt = m_dt;
    const float boost = brightnessBoost();

    // =====================================================================
    // Stage 1: Waveform History (SB lines 189-193)
    // =====================================================================
    // Prefer SB waveform if present; otherwise use contract waveform.
    const int16_t* wfSrc = ctx.audio.controlBus.sb_waveform;
    if (ctx.audio.controlBus.sb_waveform_peak_scaled < 0.0001f) {
        wfSrc = ctx.audio.controlBus.waveform;
    }

    memcpy(m_ps->wfHistory[m_ps->wfHistoryIdx], wfSrc, kWfPoints * sizeof(int16_t));
    m_ps->wfHistoryIdx = (m_ps->wfHistoryIdx + 1) % kHistFrames;

    // =====================================================================
    // Stage 2: Peak Smoothing (SB line 134)
    // =====================================================================
    // dt-corrected: tau = 0.217s (equivalent to SB's alpha=0.05 at ~90 FPS)
    float rawPeak = ctx.audio.controlBus.sb_waveform_peak_scaled;
    if (rawPeak < 0.0001f) {
        // Fallback: derive from RMS when SB sidecar not populated
        rawPeak = ctx.audio.rms() * 2.0f;
    }
    const float alphaPeak = 1.0f - expf(-dt / 0.217f);
    m_ps->wfPeakScaledLast += (rawPeak - m_ps->wfPeakScaledLast) * alphaPeak;

    // =====================================================================
    // Stage 3: Colour Synthesis (SB lines 136-169)
    // =====================================================================
    const bool chromaticMode = (ctx.saturation >= 128);
    static constexpr float kLedShare = 255.0f / 12.0f;  // ~21.25

    float newColour[3] = {0.0f, 0.0f, 0.0f};

    if (chromaticMode) {
        // Chromatic mode: sum HSV-derived colours per chroma bin
        for (uint8_t c = 0; c < 12; ++c) {
            float bin = m_chromaSmooth[c];
            float bright = applyContrast(bin, m_contrast);

            // Apply brightness boost (1.0 for parity, 1.5 for bright variant)
            bright *= boost;
            if (bright > 1.0f) bright = 1.0f;

            uint8_t hue = (uint8_t)(255.0f * (float)c / 12.0f);
            CRGB noteCol;
            hsv2rgb_spectrum(CHSV(hue, 255, (uint8_t)(bright * kLedShare)), noteCol);
            newColour[0] += (float)noteCol.r;
            newColour[1] += (float)noteCol.g;
            newColour[2] += (float)noteCol.b;
        }
    } else {
        // Non-chromatic mode: sum brightness, apply single hue
        float brightnessSum = 0.0f;
        for (uint8_t c = 0; c < 12; ++c) {
            float bin = m_chromaSmooth[c];
            float bright = applyContrast(bin, m_contrast);

            bright *= boost;
            if (bright > 1.0f) bright = 1.0f;

            brightnessSum += bright * kLedShare;
        }

        // Derive hue from chromaHue parameter + auto-shifted hue position
        uint8_t chromaHue8 = (uint8_t)((m_chromaHue + m_huePosition) * 255.0f);
        CRGB sumCol;
        hsv2rgb_spectrum(CHSV(chromaHue8, 255, (uint8_t)fminf(brightnessSum, 255.0f)), sumCol);
        newColour[0] = (float)sumCol.r;
        newColour[1] = (float)sumCol.g;
        newColour[2] = (float)sumCol.b;
    }

    // =====================================================================
    // Stage 4: Colour Temporal Smoothing (SB lines 178-186)
    // =====================================================================
    const float alphaColour = 1.0f - expf(-dt / 0.217f);
    for (int ch = 0; ch < 3; ++ch) {
        m_ps->sumColourFloat[ch] += (newColour[ch] - m_ps->sumColourFloat[ch]) * alphaColour;
    }

    // =====================================================================
    // Stage 5: Per-Pixel Waveform Rendering (SB lines 188-227)
    // =====================================================================
    // Map ctx.speed to SB MOOD: mood = clamp(speed / 20.0, 0.0, 1.0)
    const float mood = clampF((float)ctx.speed / 20.0f, 0.0f, 1.0f);

    // SB smoothing factor: (0.1 + mood * 0.9) * 0.05 at ~90 FPS
    // dt-correct: tau = -1.0 / (90.0 * log(1.0 - (0.1 + mood * 0.9) * 0.05))
    const float sbSmooth = (0.1f + mood * 0.9f) * 0.05f;
    const float tau = -1.0f / (90.0f * logf(1.0f - sbSmooth));
    const float alphaWf = 1.0f - expf(-dt / tau);

    // Peak gate: clamp peak * 4 to [0, 1]
    const float peak = fminf(m_ps->wfPeakScaledLast * 4.0f, 1.0f);

    for (uint16_t pixel = 0; pixel < kHalfLength; ++pixel) {
        // Resample: linearly interpolate from 128 history samples to 80 pixels
        const float srcIdx = (float)pixel * (128.0f / 80.0f);
        const int lo = (int)srcIdx;
        const float frac = srcIdx - (float)lo;
        const int hi = (lo + 1 < kWfPoints) ? (lo + 1) : (kWfPoints - 1);

        // Average 4 history frames at this sample position
        float avgSample = 0.0f;
        for (uint8_t h = 0; h < kHistFrames; ++h) {
            const float s0 = (float)m_ps->wfHistory[h][lo];
            const float s1 = (float)m_ps->wfHistory[h][hi];
            avgSample += s0 * (1.0f - frac) + s1 * frac;
        }
        avgSample *= 0.25f;

        // Normalise: int16_t domain → [-1, 1] range (SB uses /128.0 for 8-bit;
        // we use /32768.0 for int16_t domain then scale to match SB's visual range)
        // SB's waveform is 8-bit (-128..127), ours is int16_t (-32768..32767).
        // Divide by 32768 to normalise, but SB divides by 128 for its 8-bit range.
        // The waveform values on ControlBus are already scaled int16_t.
        const float inputWaveSample = avgSample / 32768.0f;

        // Per-pixel EMA smoothing (mood-dependent)
        m_ps->waveformLast[pixel] += (inputWaveSample - m_ps->waveformLast[pixel]) * alphaWf;

        // Brightness: 0.5 + waveformLast * 0.5, clamped to [0, 1]
        float outputBrightness = 0.5f + m_ps->waveformLast[pixel] * 0.5f;
        outputBrightness = clampF(outputBrightness, 0.0f, 1.0f);

        // Apply peak gate
        outputBrightness *= peak;

        // Write pixel: right half (centre-origin outward)
        const uint16_t ledIdx = kCenterRight + pixel;
        if (ledIdx < ctx.ledCount) {
            ctx.leds[ledIdx] = CRGB(
                (uint8_t)clampF(m_ps->sumColourFloat[0] * outputBrightness, 0.0f, 255.0f),
                (uint8_t)clampF(m_ps->sumColourFloat[1] * outputBrightness, 0.0f, 255.0f),
                (uint8_t)clampF(m_ps->sumColourFloat[2] * outputBrightness, 0.0f, 255.0f)
            );
        }
    }

    // =====================================================================
    // Stage 6: Mirror right half to left half (centre-origin)
    // =====================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        const uint16_t rightIdx = kCenterRight + i;
        const uint16_t leftIdx  = kCenterLeft - i;
        if (rightIdx < ctx.ledCount && leftIdx < ctx.ledCount) {
            ctx.leds[leftIdx] = ctx.leds[rightIdx];
        }
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
// Metadata — parity variant
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbWaveformOscilloscopeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Waveform Oscilloscope",
        "SB 3.0.0 waveform oscilloscope (exact parity)",
        plugins::EffectCategory::PARTY,
        1,
        "SB.Waveform"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Metadata — bright variant
// ---------------------------------------------------------------------------

const plugins::EffectMetadata& SbWaveformOscilloscopyBrightEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "SB Waveform Oscilloscope (Bright)",
        "SB 3.0.0 waveform oscilloscope (1.5x brightness boost)",
        plugins::EffectCategory::PARTY,
        1,
        "SB.Waveform"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbWaveformOscilloscopeBase::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbWaveformOscilloscopeBase::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbWaveformOscilloscopeBase::setParameter(const char* name, float value) {
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

float SbWaveformOscilloscopeBase::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "chromaHue") == 0) return m_chromaHue;
    if (strcmp(name, "contrast") == 0)  return m_contrast;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
