/**
 * @file SbRawWaveformScopeEffect.h
 * @brief SB Spectral Shape — spectral centroid/flatness visualiser
 *
 * Renders spectral shape as a positioned gaussian blob:
 *   - Pure tone = single sharp bright line (2-4 pixels wide)
 *   - Dense chord / noise = wide shimmering band (8-10 pixels wide)
 *
 * Algorithm:
 *   1. Decay trail buffer (audio-coupled, same pattern as K1 Waveform)
 *   2. Smooth bins64 temporally (asymmetric: fast attack, slow decay)
 *   3. Compute spectral centroid (weighted mean frequency) and flatness
 *      (geometric/arithmetic mean ratio — 0=tonal, 1=white noise)
 *   4. Render a gaussian blob at centroid position, width driven by flatness
 *   5. Mirror right half to left half (centre-origin), copy strip 1 to strip 2
 *
 * Colour synthesis uses the SbK1BaseEffect chromagram pipeline with palette-based
 * additive accumulation.
 *
 * Centre-origin mirror: renders right half (80 -> 159), mirrors to left.
 * Copy strip 1 to strip 2.
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

// =========================================================================
// PSRAM layout — allocated once in init(), freed in cleanup()
// =========================================================================

struct SbRawWfScopePsram {
    CRGB trailBuffer[160];       ///< Persistent pixel buffer for frame-to-frame trails
    float smoothedBins[64];      ///< Temporally smoothed spectrum (asymmetric attack/decay)
    float smoothedCentroid;      ///< Smoothed spectral centroid position (0-63)
    float smoothedFlatness;      ///< Smoothed spectral flatness (0=tonal, 1=noise)
};

// =========================================================================
// SbRawWaveformScopeEffect
// =========================================================================

class SbRawWaveformScopeEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1310; // EID_SB_RAW_WAVEFORM_SCOPE

    SbRawWaveformScopeEffect() = default;
    ~SbRawWaveformScopeEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    // Parameter interface
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

private:
    static constexpr uint16_t kHalfLength   = lightwaveos::effects::HALF_LENGTH;    // 80
    static constexpr uint16_t kStripLength  = lightwaveos::effects::STRIP_LENGTH;   // 160
    static constexpr uint16_t kCenterLeft   = lightwaveos::effects::CENTER_LEFT;    // 79
    static constexpr uint16_t kCenterRight  = lightwaveos::effects::CENTER_RIGHT;   // 80
    static constexpr uint8_t  kBinCount     = 64;

    // Effect parameters
    float m_chromaHue   = 0.0f;
    float m_contrast    = 1.0f;

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM pointer
    SbRawWfScopePsram* m_ps = nullptr;
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
