/**
 * @file SbK1BaseEffect.h
 * @brief Shared base class for K1.Lightwave visual effects ported to Ledstrip firmware-v3
 *
 * Provides the full K1 audio analysis pipeline:
 * - 256-bin FFT -> 96 semitone bins (reconstructSpectrum96)
 * - Magnitude averaging + smoothing (spectrogram)
 * - 96 -> 12-bin chromagram with peak tracking
 * - Spectral novelty computation
 * - K1 color shift algorithm (6-step hue position walk)
 * - Waveform peak envelope follower
 * - VU level tracking
 * - Chromagram color synthesis (CRGB_F float precision)
 *
 * Derived effects implement renderEffect() and getMetadata().
 * The base class handles init/render/cleanup lifecycle, calling
 * baseProcessAudio() each frame before delegating to renderEffect().
 *
 * SB Parity Shortcut: When Sensory Bridge parity data is available on the
 * ControlBus (sb_chromagram_smooth, sb_hue_position), the base class uses
 * those values directly instead of reconstructing from bins256.
 */

#pragma once

#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include <FastLED.h>
#endif

#include <cstring>
#include <cmath>

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// =========================================================================
// Float RGB for K1-parity sub-byte precision rendering
// =========================================================================

struct CRGB_F {
    float r = 0.0f, g = 0.0f, b = 0.0f;

    CRGB_F() = default;
    CRGB_F(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}

    CRGB_F& operator+=(const CRGB_F& o) { r += o.r; g += o.g; b += o.b; return *this; }
    CRGB_F& operator*=(float s) { r *= s; g *= s; b *= s; return *this; }
    CRGB_F  operator*(float s) const { return {r * s, g * s, b * s}; }

    CRGB toCRGB() const {
        return CRGB(
            (uint8_t)fminf(fmaxf(r * 255.0f, 0.0f), 255.0f),
            (uint8_t)fminf(fmaxf(g * 255.0f, 0.0f), 255.0f),
            (uint8_t)fminf(fmaxf(b * 255.0f, 0.0f), 255.0f)
        );
    }

    static CRGB_F fromCRGB(const CRGB& c) {
        return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f};
    }

    void clip() {
        r = fminf(fmaxf(r, 0.0f), 1.0f);
        g = fminf(fmaxf(g, 0.0f), 1.0f);
        b = fminf(fmaxf(b, 0.0f), 1.0f);
    }
};

// =========================================================================
// SbK1BaseEffect — abstract base for K1 effect ports
// =========================================================================

class SbK1BaseEffect : public plugins::IEffect {
public:
    SbK1BaseEffect() = default;
    ~SbK1BaseEffect() override = default;

    // IEffect lifecycle — calls base pipeline then derived renderEffect
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;

    // getMetadata() remains pure virtual — derived effects must implement
    // const plugins::EffectMetadata& getMetadata() const override = 0;

protected:
    // -----------------------------------------------------------------
    // K1 frequency table: 96 semitone notes (A1 55 Hz .. C#9 13290 Hz)
    // -----------------------------------------------------------------
    static constexpr int kSpecBins = 96;
    static constexpr int kChromaBins = 12;
    static constexpr int kHistoryDepth = 5;

    static constexpr float kK1Notes[96] = {
        55.00f, 58.27f, 61.74f, 65.41f, 69.30f, 73.42f, 77.78f, 82.41f, 87.31f, 92.50f, 98.00f, 103.83f,
        110.00f, 116.54f, 123.47f, 130.81f, 138.59f, 146.83f, 155.56f, 164.81f, 174.61f, 185.00f, 196.00f, 207.65f,
        220.00f, 233.08f, 246.94f, 261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f, 369.99f, 392.00f, 415.30f,
        440.00f, 466.16f, 493.88f, 523.25f, 554.37f, 587.33f, 622.25f, 659.26f, 698.46f, 739.99f, 783.99f, 830.61f,
        880.00f, 932.33f, 987.77f, 1046.50f, 1108.73f, 1174.66f, 1244.51f, 1318.51f, 1396.91f, 1479.98f, 1567.98f, 1661.22f,
        1760.00f, 1864.66f, 1975.53f, 2093.00f, 2217.46f, 2349.32f, 2489.02f, 2637.02f, 2793.83f, 2959.96f, 3135.96f, 3322.44f,
        3520.00f, 3729.31f, 3951.07f, 4186.01f, 4434.92f, 4698.64f, 4978.03f, 5274.04f, 5587.65f, 5919.91f, 6271.93f, 6644.88f,
        7040.00f, 7458.62f, 7902.13f, 8372.02f, 8869.84f, 9397.27f, 9956.06f, 10548.08f, 11175.30f, 11839.82f, 12543.85f, 13289.75f
    };

    // -----------------------------------------------------------------
    // Note color hues: 12 evenly-spaced hues (one per semitone class)
    // -----------------------------------------------------------------
    static constexpr float kNoteHues[12] = {
        0.000f, 0.083f, 0.167f, 0.250f, 0.333f, 0.417f,
        0.500f, 0.583f, 0.667f, 0.750f, 0.833f, 0.917f
    };

    // -----------------------------------------------------------------
    // PSRAM-allocated struct (large buffers MUST NOT live in DRAM)
    // -----------------------------------------------------------------
    struct SbK1BasePsram {
        float specSmooth[96];               // Smoothed spectrogram
        float spectralHistory[5][96];       // Novelty history buffer
        float magAvg[96];                   // Magnitude averaging (0.3/0.7 EMA)
    };

    // -----------------------------------------------------------------
    // DRAM-resident state (small — lives with class instance)
    // -----------------------------------------------------------------

    // Chromagram
    float m_chromaSmooth[12]    = {};
    float m_noveltyCurve[5]     = {};
    uint8_t m_histIdx           = 0;
    float m_chromaPeakTracker   = 0.01f;

    // Color shift state (K1 6-step algorithm)
    float m_huePosition         = 0.0f;
    float m_hueShiftSpeed       = 0.0f;
    float m_huePushDir          = -1.0f;
    float m_hueDest             = 0.0f;
    float m_hueShiftMix         = -0.35f;
    float m_hueShiftMixTarget   = 1.0f;

    // VU / peak state
    float m_vuLast              = 0.0f;
    float m_vuAvg               = 0.0f;
    float m_wfPeakScaled        = 0.0f;
    float m_wfPeakLast          = 0.0f;
    float m_wfFollower          = 750.0f;

    // Frame delta time for rate-independent smoothing
    float m_dt              = 1.0f / 120.0f;

    // Per-zone tracking
    uint32_t m_lastHopSeq       = 0;

    // Chromagram color output (written by synthesizeChromaColor)
    CRGB_F m_chromaColor;
    float  m_chromaTotalMag     = 0.0f;

    // -----------------------------------------------------------------
    // Rate-independent time constants (tau in seconds)
    // Derived from original frame-coupled alphas assuming 120 FPS
    // -----------------------------------------------------------------
    static constexpr float kTauMagAvg           = 0.0069f;  // was alpha=0.7 new/0.3 old
    static constexpr float kTauSpecAttack       = 0.0044f;  // was alpha=0.85
    static constexpr float kTauSpecDecay        = 0.0091f;  // was alpha=0.6
    static constexpr float kTauChromaPeakDecay  = 0.4125f;  // was *=0.98 per frame
    static constexpr float kTauChromaPeakAttack = 0.0163f;  // was alpha=0.4
    static constexpr float kTauHueSpeedDecay    = 0.8291f;  // was *=0.99 per frame
    static constexpr float kTauHueMix           = 0.8291f;  // was alpha=0.01
    static constexpr float kTauWfFollowerAttack = 0.0069f;  // was alpha=0.7
    static constexpr float kTauWfFollowerDecay  = 0.0999f;  // was alpha=0.08
    static constexpr float kTauWfPeakLast       = 0.0234f;  // was alpha=0.3
    static constexpr float kTauVuAvg            = 0.0374f;  // was alpha=0.2

    // -----------------------------------------------------------------
    // Protected methods — called by derived effects
    // -----------------------------------------------------------------

    /// Allocate PSRAM, zero all state. Returns false on alloc failure.
    bool baseInit();

    /// Free PSRAM.
    void baseCleanup();

    /// Full per-frame audio pipeline. Call at the start of each render().
    void baseProcessAudio(plugins::EffectContext& ctx);

    /// Pure virtual: derived effects implement their per-frame rendering here.
    /// Called by render() after baseProcessAudio().
    virtual void renderEffect(plugins::EffectContext& ctx) = 0;

    /// Returns floor-subtracted VU level.
    float getVu(plugins::EffectContext& ctx) const;

    /// K1 contrast curve: (bin*bin)*0.65 + bin*0.35, with fractional iteration.
    static float applyContrast(float bin, float squareIter);

    /// K1-style HSV to RGB (float domain, uses FastLED CHSV internally).
    static CRGB_F hsvToRgbF(float h, float s, float v);

    /// Palette-based colour lookup using the user-selected palette from EffectContext.
    /// Maps a [0,1] position to a palette colour scaled by brightness — no HSV round-trip.
    static CRGB_F paletteColorF(const plugins::PaletteRef& palette, float position, float brightness);

private:
    // PSRAM pointer
#ifndef NATIVE_BUILD
    SbK1BasePsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

    // -----------------------------------------------------------------
    // Private audio pipeline stages
    // -----------------------------------------------------------------

    /// Map 256 linear FFT bins to 96 semitone bins via interpolation.
    void reconstructSpectrum96(const float* bins256, float binHz);

    /// Spectrogram smoothing: attack 0.5, decay 0.25.
    void smoothSpectrogram();

    /// Fold 96 bins -> 12 chromagram bins, peak-track, normalise.
    void buildChromagram();

    /// Positive spectral diffs, sqrt, history buffer.
    void computeNovelty();

    /// K1 6-step color shift algorithm.
    void processColorShift(float noveltyNow);

    /// Waveform peak envelope follower (two-stage + smoothing).
    void updateWaveformPeak(plugins::EffectContext& ctx);

    /// 12-bin chromagram -> weighted color sum.
    void synthesizeChromaColor(float squareIter, float saturation, float chroma, bool chromaticMode);
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
