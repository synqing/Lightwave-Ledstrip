/**
 * @file SbRawWaveformScopeEffect.h
 * @brief SB Raw Waveform Scope — time-domain oscilloscope with aggressive processing
 *
 * Renders the activity-gated waveform data as a full-strip oscilloscope.
 * The waveform in ControlBus is multiplied by an activity factor (0.05-1.0)
 * by AudioActor, reducing it to ~3% of full range during typical audio.
 * This effect compensates with:
 *
 *   1. 8-pass bidirectional low-pass filter (alpha=0.25) — very aggressive
 *      spatial smoothing that extracts the bass/mid envelope from noisy
 *      attenuated samples
 *   2. Asymmetric smoothed peak follower — fast attack / slow release auto-scale
 *      that tracks loudness changes without amplifying inter-frame noise
 *   3. Temporal smoothing between frames — moderate alpha EMA on pixel values
 *      to reduce flicker without losing transient response
 *   4. Envelope-mode brightness — absolute value of signed waveform, no pedestal
 *
 * Colour synthesis uses the SbK1BaseEffect chromagram pipeline with palette-based
 * additive accumulation (same pattern as SbWaveformOscilloscopeEffect).
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
    float samples[128];          ///< Current filtered waveform (workspace)
    float smoothedPeak;          ///< Peak follower for auto-scale
    float prevPixels[80];        ///< Previous frame pixel values for temporal smoothing
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
    static constexpr uint8_t  kWfPoints     = 128;

    // Effect parameters
    float m_chromaHue   = 0.0f;
    float m_contrast    = 1.0f;

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM pointer
    SbRawWfScopePsram* m_ps = nullptr;
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
