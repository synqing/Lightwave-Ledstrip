/**
 * @file SbK1WaveformEffect.cpp
 * @brief K1 Lightwave waveform dot mode
 *
 * K1-native audio-reactive dot mode inspired by Sensory Bridge's chromagram
 * colour synthesis. This is NOT a 1:1 parity port of SB's light_mode_waveform()
 * — the SB original was a full-strip oscilloscope that was never shipped
 * (dead code in SB 3.0.0 and 3.1.0). This effect is an original K1 design:
 *
 *   1. Chromagram-driven colour synthesis (shared with SB)
 *   2. Single bouncing dot positioned by waveform peak amplitude
 *   3. Dynamic amplitude-responsive trail fade (K1 original)
 *   4. dt-corrected sub-pixel scroll (K1 original)
 *   5. Sub-pixel dot rendering (K1 original)
 *   6. Mirror right half to left half (centre-origin)
 *   7. Copy strip 1 to strip 2
 *
 * For the true SB waveform oscilloscope, see SbWaveformOscilloscopeEffect.
 */

#include "SbK1WaveformEffect.h"

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

const plugins::EffectParameter SbK1WaveformEffect::s_params[kParamCount] = {
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

bool SbK1WaveformEffect::init(plugins::EffectContext& ctx) {
    // Initialise base class (chromagram, peak follower, hue shift)
    if (!baseInit()) return false;

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbK1WaveformPsram*>(
            heap_caps_malloc(sizeof(SbK1WaveformPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#else
    if (!m_ps) {
        m_ps = new (std::nothrow) SbK1WaveformPsram;
        if (!m_ps) {
            baseCleanup();
            return false;
        }
    }
#endif

    // Zero the trail buffer and scroll accumulator
    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i] = {0.0f, 0.0f, 0.0f};
    }
    m_ps->scrollAccum = 0.0f;

    // Reset parameters to defaults
    m_sensitivity = 1.0f;
    m_contrast    = 1.0f;
    m_chromaHue   = 0.0f;

    (void)ctx;
    return true;
}

void SbK1WaveformEffect::cleanup() {
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

void SbK1WaveformEffect::renderEffect(plugins::EffectContext& ctx) {
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
        // Mirror strip 1 to strip 2
        for (uint16_t i = 0; i < kStripLength && (kStripLength + i) < ctx.ledCount; ++i) {
            ctx.leds[kStripLength + i] = ctx.leds[i];
        }
        return;
    }

    // =====================================================================
    // K1 Waveform — chromagram dot mode (K1-native design)
    // =====================================================================

    // --- K1 WAVEFORM COLOUR SYNTHESIS ---
    // K1 parity: additive accumulation of palette colours per chromagram bin.
    // K1 waveform: NO 0.5 cyan offset (that's Bloom). NO hue_position in chromatic mode.
    // K1's normalize is a no-op (divide then multiply by total_magnitude),
    // so the raw sum is the output — intentionally bright, clipped at output.
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F dotColor = {0.0f, 0.0f, 0.0f};
    float totalMag = 0.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        float bin = m_chromaSmooth[c];
        float bright = applyContrast(bin, m_contrast);

        if (bright > 0.05f) {
            float prog = c / 12.0f;
            // K1 parity: palette lookup at bin brightness, then accumulate
            // (matches K1's hsv(prog, SATURATION, bright) + additive sum)
            CRGB_F noteColor = paletteColorF(ctx.palette, prog, bright);
            dotColor += noteColor;
            totalMag += bright;
        }
    }

    // Soft-knee brightness cap: scale dotColor so the brightest channel
    // never exceeds 1.0. Preserves hue ratios (unlike hard clip which
    // desaturates toward white). Only engages when accumulation overflows.
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

    // Non-chromatic: force palette position to CHROMA + hue_position
    // K1 parity: hsv(chroma_val + hue_position, SATURATION, total_magnitude)
    if (!chromaticMode) {
        float forcedPos = m_chromaHue + m_huePosition;
        // Clamp totalMag to [0,1] — unbounded sum passed as brightness
        // to paletteColorF produces channels >1.0 which clip to white.
        dotColor = paletteColorF(ctx.palette, forcedPos, fminf(totalMag, 1.0f));
    }

    // Apply PHOTONS brightness
    float photons = (float)ctx.brightness / 255.0f;
    dotColor *= photons;
    dotColor.clip();

    // Audio energy gate: scale dot brightness proportionally to actual RMS.
    const float rmsNow = ctx.audio.rms();
    const float rmsScale = clampF(rmsNow * 3.0f, 0.0f, 1.0f);
    dotColor *= rmsScale;

    // ControlBus silence gate as final safety net
    const float silScale = ctx.audio.controlBus.silentScale;

    // Silence gate: scale dot brightness by silentScale.
    dotColor *= silScale;

    // --- DYNAMIC TRAIL FADE (dt-corrected) ---
    // Rate-independent exponential decay. The decay rate scales with
    // waveform amplitude: louder audio = faster fade (shorter trails).
    // At silence the minimum rate produces ~2.5s full decay.
    // At moderate audio (absAmp=0.5): half-life ~0.35s (visible trail ~1s).
    // At full amplitude: half-life ~0.12s (tight, punchy trail).
    float absAmp = clampF(fabsf(m_wfPeakScaled), 0.0f, 1.0f);
    static constexpr float kMinDecayRate = 0.8f;   // per-second (silence floor, ~3.5s full decay)
    static constexpr float kDecayScale   = 3.5f;   // additional rate per unit amplitude
    float decayRate = kMinDecayRate + kDecayScale * absAmp;

    // Accelerate trail fade when audio is quiet or silent
    float quietFactor = fminf(rmsScale, silScale);
    if (quietFactor < 0.9f) {
        decayRate += 10.0f * (1.0f - quietFactor);
    }
    float fade = expf(-decayRate * m_dt);

    for (uint16_t i = 0; i < kStripLength; ++i) {
        m_ps->trailBuffer[i].r *= fade;
        m_ps->trailBuffer[i].g *= fade;
        m_ps->trailBuffer[i].b *= fade;
    }

    // --- SHIFT (dt-corrected sub-pixel scroll) ---
    // Scroll speed in pixels/second, decoupled from frame rate.
    // At 120 FPS this produces ~0.5 pixels/frame (vs old 1.0 px/frame),
    // giving trails roughly twice the visual persistence on the strip.
    static constexpr float kBaseScrollRate = 150.0f;
    static constexpr float kSpeedMidpoint = 10.0f;  // DEFAULT_SPEED from RendererActor
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

    // --- DOT POSITION ---
    float amp = m_wfPeakLast;
    // Hard gate removed. m_wfPeakLast is already EMA-smoothed (tau=23ms)
    // so it doesn't jitter near zero. With the fade floor, a near-zero dot
    // at centre fades naturally instead of painting a permanent stripe.

    float safeSensitivity = (m_sensitivity > 0.01f) ? m_sensitivity : 0.01f;
    amp *= 0.7f / safeSensitivity;
    if (amp > 1.0f) amp = 1.0f;
    if (amp < -1.0f) amp = -1.0f;

    float halfLen = static_cast<float>(kStripLength) * 0.5f;
    float posF = halfLen + amp * halfLen;

    // --- SUB-PIXEL DOT RENDERING ---
    // Blend dot across two adjacent pixels using fractional position
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

    // --- MIRROR right half to left half ---
    for (uint16_t i = 0; i < kHalfLength; ++i) {
        m_ps->trailBuffer[kCenterLeft - i] = m_ps->trailBuffer[kCenterRight + i];
    }

    // --- OUTPUT TO LEDS ---
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

const plugins::EffectMetadata& SbK1WaveformEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Waveform",
        "K1 Lightwave waveform dot mode (parity port)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Parameter interface
// ---------------------------------------------------------------------------

uint8_t SbK1WaveformEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbK1WaveformEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbK1WaveformEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    // Simple string matching for the 4 parameters
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

float SbK1WaveformEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (strcmp(name, "sensitivity") == 0) return m_sensitivity;
    if (strcmp(name, "contrast") == 0)    return m_contrast;
    if (strcmp(name, "chromaHue") == 0)   return m_chromaHue;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
