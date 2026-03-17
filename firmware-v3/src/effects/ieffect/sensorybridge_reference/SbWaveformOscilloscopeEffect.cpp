/**
 * @file SbWaveformOscilloscopeEffect.cpp
 * @brief SB Waveform oscilloscope — rewritten with proven K1 patterns
 *
 * Full-strip oscilloscope rendering the audio waveform shape across all LEDs.
 * Combines three proven patterns from the codebase:
 *
 *   1. Waveform rendering from EsWaveformRefEffect:
 *      - Hop-gated 4-frame history ring buffer
 *      - History frame averaging to stabilise morphology
 *      - 3-pass one-pole low-pass filter (alpha ~0.54, ~2.1 kHz cutoff)
 *      - Auto-scaling by peak magnitude to fill [-1, 1]
 *      - interp128-style linear interpolation (128 samples -> 80 pixels)
 *
 *   2. Colour synthesis from SbK1WaveformEffect:
 *      - Palette-based additive accumulation per chromagram bin
 *      - Soft-knee cap (preserves hue ratios, no white clipping)
 *      - Non-chromatic fallback via paletteColorF
 *      - PHOTONS brightness scaling
 *
 *   3. Per-pixel brightness from waveform:
 *      - 0.5 pedestal + auto-scaled sample * 0.5 => [0, 1]
 *      - Peak-gated dimming during silence
 *
 * Centre-origin mirror: renders right half (80 -> 159), mirrors to left.
 * Copy strip 1 to strip 2.
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

    // Sentinel for hop-gate: UINT32_MAX guarantees the first frame detects
    // a "new hop" and primes all 4 history slots (m_lastHopSeq was zeroed
    // by baseInit, override it here).
    m_lastHopSeq = UINT32_MAX;

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
    // Stage 1: Hop-gated waveform history update
    // (EsWaveformRefEffect pattern: only store new waveform when hop changes)
    // =====================================================================
    const uint32_t hopSeq = ctx.audio.controlBus.hop_seq;
    const bool newHop = (hopSeq != m_lastHopSeq);
    const bool firstHop = (m_lastHopSeq == UINT32_MAX);  // Sentinel from init()

    if (newHop || firstHop) {
        m_lastHopSeq = hopSeq;

        if (firstHop) {
            // First time: fill all 4 history slots with the current waveform
            for (uint8_t f = 0; f < kHistFrames; ++f) {
                memcpy(m_ps->wfHistory[f], ctx.audio.controlBus.waveform,
                       kWfPoints * sizeof(int16_t));
            }
            m_ps->wfHistoryIdx = 0;
        } else {
            // Normal: write current waveform into ring buffer slot
            memcpy(m_ps->wfHistory[m_ps->wfHistoryIdx], ctx.audio.controlBus.waveform,
                   kWfPoints * sizeof(int16_t));
            m_ps->wfHistoryIdx = (m_ps->wfHistoryIdx + 1) % kHistFrames;
        }
    }

    // =====================================================================
    // Stage 2: Average 4 history frames into local float buffer
    // (Stack allocation: 128 floats = 512 bytes — well within budget)
    // =====================================================================
    float samples[kWfPoints];
    for (uint8_t i = 0; i < kWfPoints; ++i) {
        float sum = 0.0f;
        for (uint8_t f = 0; f < kHistFrames; ++f) {
            sum += static_cast<float>(m_ps->wfHistory[f][i]);
        }
        // Normalise: int16_t range to [-1, 1]
        samples[i] = (sum / static_cast<float>(kHistFrames)) / 32768.0f;
    }

    // =====================================================================
    // Stage 3: Aggressive spatial low-pass filter (6-pass)
    // The activity gate attenuates waveform samples to ±1000 of ±32768,
    // leaving high-frequency noise. 6 passes at alpha=0.35 produce a
    // smooth bass/mid-frequency envelope rather than choppy noise stripes.
    // Forward + reverse passes prevent phase shift (symmetric filtering).
    // =====================================================================
    static constexpr float kLpfAlpha = 0.35f;

    for (uint8_t pass = 0; pass < 3; ++pass) {
        // Forward pass
        float y = samples[0];
        for (uint8_t i = 0; i < kWfPoints; ++i) {
            y = y + kLpfAlpha * (samples[i] - y);
            samples[i] = y;
        }
        // Reverse pass (prevents rightward phase shift)
        y = samples[kWfPoints - 1];
        for (int16_t i = kWfPoints - 1; i >= 0; --i) {
            y = y + kLpfAlpha * (samples[i] - y);
            samples[i] = y;
        }
    }

    // =====================================================================
    // Stage 4: Auto-scale by SMOOTHED peak magnitude
    // Use a follower on the peak rather than raw max to prevent
    // frame-to-frame noise amplification during quiet passages.
    // =====================================================================
    float maxAbs = 0.0f;
    for (uint8_t i = 0; i < kWfPoints; ++i) {
        const float a = fabsf(samples[i]);
        if (a > maxAbs) maxAbs = a;
    }
    // Smoothed peak follower: fast attack (track loud passages immediately),
    // slow release (don't amplify noise when audio drops)
    static constexpr float kPeakAttackAlpha = 0.3f;
    static constexpr float kPeakReleaseAlpha = 0.02f;
    if (maxAbs > m_ps->waveformLast[79]) {
        // Reuse waveformLast[79] as peak follower storage (last pixel, rarely read)
        m_ps->waveformLast[79] = m_ps->waveformLast[79] + kPeakAttackAlpha * (maxAbs - m_ps->waveformLast[79]);
    } else {
        m_ps->waveformLast[79] = m_ps->waveformLast[79] + kPeakReleaseAlpha * (maxAbs - m_ps->waveformLast[79]);
    }
    const float smoothedPeak = fmaxf(m_ps->waveformLast[79], 0.001f);
    const float autoScale = 1.0f / smoothedPeak;

    // =====================================================================
    // Stage 5: Peak smoothing for silence gate
    // dt-corrected EMA, tau=0.217s
    // =====================================================================
    float rawPeak = ctx.audio.controlBus.sb_waveform_peak_scaled;
    if (rawPeak < 0.0001f) {
        // Fallback: derive from RMS when SB sidecar not populated
        rawPeak = ctx.audio.rms() * 2.0f;
    }
    const float alphaPeak = 1.0f - expf(-dt / 0.217f);
    m_ps->wfPeakScaledLast += (rawPeak - m_ps->wfPeakScaledLast) * alphaPeak;

    // Peak gate: ramp up quickly, [0, 1]
    const float peakGate = fminf(m_ps->wfPeakScaledLast * 4.0f, 1.0f);

    // =====================================================================
    // Stage 6: Colour synthesis (palette-based, from SbK1WaveformEffect)
    // Uses paletteColorF for bright, palette-driven output instead of
    // the original dim HSV/kLedShare approach.
    // =====================================================================
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F dotColor = {0.0f, 0.0f, 0.0f};
    float totalMag = 0.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        float bin = m_chromaSmooth[c];
        float bright = applyContrast(bin, m_contrast);

        // Apply brightness boost (1.0 for parity, 1.5 for bright variant)
        bright *= boost;
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
    float photons = static_cast<float>(ctx.brightness) / 255.0f;
    dotColor *= photons;
    dotColor.clip();

    // =====================================================================
    // Stage 7: Per-pixel waveform rendering (80 pixels, right half)
    // interp128-style interpolation: map 128 filtered samples to 80 pixels
    // =====================================================================
    for (uint16_t pixel = 0; pixel < kHalfLength; ++pixel) {
        // Progress: 0.0 at centre, 1.0 at edge
        const float progress = (kHalfLength <= 1) ? 0.0f
            : (static_cast<float>(pixel) / static_cast<float>(kHalfLength - 1));

        // Interpolate from 128 filtered samples
        const float srcIdx = progress * 127.0f;
        const int lo = static_cast<int>(srcIdx);
        const float frac = srcIdx - static_cast<float>(lo);
        const int hi = (lo + 1 < kWfPoints) ? (lo + 1) : (kWfPoints - 1);

        float signedSample = (samples[lo] * (1.0f - frac) + samples[hi] * frac) * autoScale;

        // Envelope mode: brightness tracks waveform AMPLITUDE (not signed value).
        // Avoids dark bands at zero crossings and removes the need for a 0.5
        // pedestal that makes the strip uniformly dim.
        float brightness = fabsf(signedSample);
        if (brightness > 1.0f) brightness = 1.0f;

        // Dim during silence via peak gate
        brightness *= peakGate;

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

    // =====================================================================
    // Stage 8: Mirror right half to left half (centre-origin)
    // =====================================================================
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        const uint16_t rightIdx = kCenterRight + i;
        const uint16_t leftIdx  = kCenterLeft - i;
        if (rightIdx < ctx.ledCount && leftIdx < ctx.ledCount) {
            ctx.leds[leftIdx] = ctx.leds[rightIdx];
        }
    }

    // =====================================================================
    // Stage 9: Copy strip 1 to strip 2
    // =====================================================================
    for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
        ctx.leds[kStripLength + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
}

// ---------------------------------------------------------------------------
// Metadata -- parity variant
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
// Metadata -- bright variant
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
