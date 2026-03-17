/**
 * @file SbSpectralBeatPulseEffect.h
 * @brief SB Onset Map — percussive transient mapper
 *
 * Maps percussive onsets to specific spatial positions along the strip:
 *   - Snare triggers flash near centre (warm colour, wide blob)
 *   - Hi-hat triggers flash near edges (cool colour, narrow blob)
 *   - Ghost notes produce subtle ambient shimmer via spectral flux
 *
 * Crossmodal mapping: low-frequency events are larger and central,
 * high-frequency events are smaller and peripheral.
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

class SbSpectralBeatPulseEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1312; // EID_SB_SPECTRAL_BEAT_PULSE
    SbSpectralBeatPulseEffect() = default;
    ~SbSpectralBeatPulseEffect() override = default;

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

    // Effect parameters
    float m_sensitivity = 1.0f;   ///< Onset sensitivity multiplier
    float m_chromaHue   = 0.0f;   ///< Hue offset for palette lookup

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM-allocated state
    struct SbOnsetMapPsram {
        CRGB trailBuffer[160];   ///< Persistent pixel buffer for frame-to-frame trails
        float snareDecay;        ///< Snare flash envelope (fast decay)
        float hihatDecay;        ///< Hi-hat flash envelope (fast decay)
        float fluxSmoothed;      ///< Background flux for ambient shimmer
    };

#ifndef NATIVE_BUILD
    SbOnsetMapPsram* m_ps = nullptr;
#else
    SbOnsetMapPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
