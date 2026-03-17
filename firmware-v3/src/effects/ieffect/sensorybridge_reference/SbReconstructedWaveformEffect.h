/**
 * @file SbReconstructedWaveformEffect.h
 * @brief SB Reconstructed Waveform — spectrally-synthesised oscillating shape
 *
 * Synthesises a waveform-like oscillating pattern from the 64 Goertzel bins.
 * This is NOT the real audio waveform — it is a musically-driven oscillating
 * shape built by additive cosine synthesis, where each bin's magnitude drives
 * the amplitude of a cosine at a proportional spatial frequency.
 *
 * The result is a smooth, animated pattern that responds to the spectral
 * content of the audio: bass-heavy content produces broad slow waves,
 * treble-heavy content produces tight fast ripples.
 *
 * Pipeline:
 *   1. Temporally smooth bins64 (fast attack, slow decay)
 *   2. Advance global phase accumulator
 *   3. Additive synthesis: sum 16 cosine components per pixel
 *   4. Temporal blend with previous frame
 *   5. Chromagram palette colour from SbK1BaseEffect
 *   6. Centre-origin mirror + strip copy
 *
 * Depends on SbK1BaseEffect for chromagram colour synthesis, peak envelope
 * following, and auto-hue-shift. See SbK1BaseEffect.h for the base contract.
 */

#pragma once

#include "SbK1BaseEffect.h"
#include "../../CoreEffects.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

class SbReconstructedWaveformEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1311; // EID_SB_RECONSTRUCTED_WAVEFORM
    SbReconstructedWaveformEffect() = default;
    ~SbReconstructedWaveformEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

    // Parameter interface
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t kHalfLength   = lightwaveos::effects::HALF_LENGTH;    // 80
    static constexpr uint16_t kStripLength  = lightwaveos::effects::STRIP_LENGTH;   // 160
    static constexpr uint16_t kCenterLeft   = lightwaveos::effects::CENTER_LEFT;    // 79
    static constexpr uint16_t kCenterRight  = lightwaveos::effects::CENTER_RIGHT;   // 80
    static constexpr uint8_t  kNumBins      = 64;
    static constexpr uint8_t  kSynthBins    = 16;  ///< Number of bins used in additive synthesis

    // Effect parameters
    float m_contrast    = 1.0f;
    float m_chromaHue   = 0.0f;

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM-allocated buffers (large state must not live in DRAM)
    struct SbReconWfPsram {
        float phase;                        ///< Global phase accumulator (advances with time)
        float smoothedBins[64];             ///< Temporally smoothed bin magnitudes
        float prevPixels[80];               ///< Previous frame for temporal blending
    };

#ifndef NATIVE_BUILD
    SbReconWfPsram* m_ps = nullptr;
#else
    SbReconWfPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
