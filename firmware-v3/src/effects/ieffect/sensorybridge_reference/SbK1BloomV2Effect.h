/**
 * @file SbK1BloomV2Effect.h
 * @brief Faithful port of K1's bloom_algo_center_scroll (algorithm 0)
 *
 * This is a line-by-line port of K1's bloom algo 0 from lightshow_modes.h:593-655.
 * Key behaviours preserved exactly:
 *
 *   1. GRAYSCALE TRANSPORT — scroll state stores intensity only (r=g=b).
 *      Colour is applied AFTER mirror at output stage via bloom_apply_color().
 *   2. ALTERNATING FRAMES — only scrolls on even frames; odd frames replay.
 *   3. INTEGER SCROLL — no sub-pixel; shift by 1 or 2 whole pixels.
 *   4. NO PER-PIXEL DECAY — pixels are copied verbatim in scroll state.
 *   5. SQRT DISTORTION — applied after saving scroll state (no feedback).
 *   6. LINEAR EDGE FADE — (half-1-i)/(half-1), not quadratic.
 *   7. SAVE ONLY RIGHT HALF — left half is always from mirror.
 *   8. PRISM + BULB — post-processing after colour application.
 */

#pragma once

#include "SbK1BaseEffect.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// Energy mode enum for the 3 variants
enum class BloomEnergyMode : uint8_t {
    MEAN    = 0,  // Average of 12 contrast-applied bins (original K1 parity)
    MAX_BIN = 1,  // Single strongest bin drives intensity (most dynamic)
    RE_NORM = 2,  // Local fast-decay peak tracker re-stretches compressed data
};

class SbK1BloomV2Effect : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1303; // EID_SB_K1_BLOOM_V2

    explicit SbK1BloomV2Effect(BloomEnergyMode mode = BloomEnergyMode::MEAN)
        : m_energyMode(mode) {}
    ~SbK1BloomV2Effect() override = default;

    const plugins::EffectMetadata& getMetadata() const override;

    // Parameter interface
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;

    // ---------------------------------------------------------------
    // Constants
    // ---------------------------------------------------------------
    static constexpr uint16_t kStripLen = 160;
    static constexpr uint16_t kHalf     = 80;

    // Incandescent warm-white filter coefficients
    static constexpr float kIncanR = 1.0000f;
    static constexpr float kIncanG = 0.4453f;
    static constexpr float kIncanB = 0.1562f;

    // ---------------------------------------------------------------
    // Parameters
    // ---------------------------------------------------------------
    static constexpr uint8_t kParamCount = 7;

    float m_mood         = 0.16f;
    float m_contrast     = 1.0f;
    float m_saturation   = 1.0f;
    float m_chromaHue    = 0.0f;
    float m_incandescent = 0.0f;
    float m_prismCount   = 1.42f;
    float m_bulbOpacity  = 0.0f;

    static const plugins::EffectParameter s_params[kParamCount];

    // Energy mode (set at construction, identifies the variant)
    BloomEnergyMode m_energyMode;

    // ---------------------------------------------------------------
    // Frame counter for alternating-frame logic
    // ---------------------------------------------------------------
    uint32_t m_iter = 0;

    // Local peak tracker for energy mode RE_NORM
    float m_localPeak = 0.01f;

    // ---------------------------------------------------------------
    // PSRAM allocation (large buffers)
    // ---------------------------------------------------------------
    struct BloomV2Psram {
        CRGB_F workBuffer[kStripLen];
        CRGB_F scrollState[kStripLen];
        CRGB_F auxBuffer[kStripLen];
        CRGB_F fxBuffer[kStripLen];
        CRGB_F tmpBuffer[kStripLen];
    };

#ifndef NATIVE_BUILD
    BloomV2Psram* m_ps2 = nullptr;
#else
    void* m_ps2 = nullptr;
#endif

    // ---------------------------------------------------------------
    // Private methods
    // ---------------------------------------------------------------
    static void prismTransform(CRGB_F* buf, CRGB_F* tmp);
};

// =========================================================================
// 3 concrete variants — each bakes in a different energy mode
// =========================================================================

class SbK1BloomV2MaxBinEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1304; // EID_SB_K1_BLOOM_V2_MAXBIN
    SbK1BloomV2MaxBinEffect() : SbK1BloomV2Effect(BloomEnergyMode::MAX_BIN) {}
    const plugins::EffectMetadata& getMetadata() const override;
};

class SbK1BloomV2ReNormEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1305; // EID_SB_K1_BLOOM_V2_RENORM
    SbK1BloomV2ReNormEffect() : SbK1BloomV2Effect(BloomEnergyMode::RE_NORM) {}
    const plugins::EffectMetadata& getMetadata() const override;
};

/**
 * Beat Pulse Bloom — onset-gated variant of Bloom V2
 *
 * Same scroll/mirror/post-processing as BloomV2, but pixels are only injected
 * when the novelty curve exceeds a threshold.  This creates discrete colour
 * blobs separated by black gaps, emphasising transients and onsets.
 *
 * When novelty IS above threshold the max-bin energy drives brightness.
 * When below threshold a black (zero) center pixel is scrolled in.
 */
class SbK1BloomV2BeatPulseEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1306; // EID_SB_K1_BLOOM_V2_BEAT_PULSE

    SbK1BloomV2BeatPulseEffect()
        : SbK1BloomV2Effect(BloomEnergyMode::MAX_BIN) {}

    const plugins::EffectMetadata& getMetadata() const override;

    // Extended parameter interface (base 7 + beatThreshold)
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

private:
    static constexpr uint8_t kBeatPulseParamCount = 8; // base 7 + beatThreshold
    static const plugins::EffectParameter s_beatPulseParams[kBeatPulseParamCount];

    float m_beatThreshold = 0.15f;
};

// =========================================================================
// Color History Scroll — full-color spectrogram variant
//
// Instead of grayscale transport + color-at-output, this variant injects
// the dominant note's FULL COLOR at center and scrolls it outward.
// The strip becomes a time-history of what notes were playing.
// =========================================================================

class SbK1BloomV2ColorHistoryEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1307; // EID_SB_K1_BLOOM_V2_COLOR_HISTORY

    SbK1BloomV2ColorHistoryEffect() : SbK1BloomV2Effect(BloomEnergyMode::MAX_BIN) {}
    ~SbK1BloomV2ColorHistoryEffect() override = default;

    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;
};

// =========================================================================
// Spectral Delta variant — intensity driven by frame-to-frame CHANGE in
// chromagram bins rather than absolute levels. Sustained notes become
// invisible; only transitions and new note onsets produce output.
// =========================================================================

class SbK1BloomV2SpectralDeltaEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1308; // EID_SB_K1_BLOOM_V2_SPECTRAL_DELTA

    SbK1BloomV2SpectralDeltaEffect() : SbK1BloomV2Effect(BloomEnergyMode::MEAN) {}
    ~SbK1BloomV2SpectralDeltaEffect() override = default;

    const plugins::EffectMetadata& getMetadata() const override;

protected:
    bool init(plugins::EffectContext& ctx) override;
    void renderEffect(plugins::EffectContext& ctx) override;

private:
    float m_prevChroma[12] = {};
};

// =========================================================================
// Bass-Treble Split variant — chromagram split into low 6 (bass) and
// high 6 (treble) bins.  Bass drives pixel brightness; treble drives
// scroll speed.  Colour shifts warm-to-cool based on bass/treble balance.
// =========================================================================

class SbK1BloomV2BassTrebleEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1309; // EID_SB_K1_BLOOM_V2_BASSTREBLE

    SbK1BloomV2BassTrebleEffect() : SbK1BloomV2Effect(BloomEnergyMode::MEAN) {}

    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;
};

// =========================================================================
// Exponential Energy variant — max-bin energy cubed for extreme contrast.
// Quiet passages are nearly black; loud peaks explode in brightness.
// =========================================================================

class SbK1BloomV2ExponentialEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x130A; // EID_SB_K1_BLOOM_V2_EXPONENTIAL

    SbK1BloomV2ExponentialEffect()
        : SbK1BloomV2Effect(BloomEnergyMode::MAX_BIN) {} // mode unused — renderEffect fully overridden

    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;
};

// =========================================================================
// Spectral Spread variant — intensity driven by spectral "peakedness"
// (inverse flatness).  Pure tones/notes produce bright output; noise or
// full mixes produce dim output.
//
// flatness   = geometric_mean(chroma) / arithmetic_mean(chroma)
// peakedness = 1.0 - flatness
// intensity  = peakedness * max_bin_energy
//
// Geometric mean computed via log-sum-exp to avoid float underflow.
// =========================================================================

class SbK1BloomV2SpectralSpreadEffect final : public SbK1BloomV2Effect {
public:
    static constexpr lightwaveos::EffectId kId = 0x130B; // EID_SB_K1_BLOOM_V2_SPECTRAL_SPREAD

    SbK1BloomV2SpectralSpreadEffect()
        : SbK1BloomV2Effect(BloomEnergyMode::MEAN) {} // mode unused — renderEffect fully overridden

    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
