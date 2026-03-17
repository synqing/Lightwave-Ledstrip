/**
 * @file SbSpectralBeatPulseEffect.h
 * @brief SB Spectral Beat Pulse — spectral envelope that pulses outward on beats
 *
 * Combines frequency display with rhythmic physicality. Eight smoothed
 * octave bands are spatially mapped from centre to edge. A beat-driven
 * expansion envelope controls how far the spectral shape reaches along
 * the strip: on-beat the spectrum expands to fill the full length,
 * between beats it contracts back toward the centre.
 *
 * The result is a breathing spectral analyser that pumps with the music.
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
    float m_contrast    = 1.0f;
    float m_chromaHue   = 0.0f;

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM-allocated state
    struct SbSpecBeatPsram {
        float smoothedBands[8];    ///< Temporally smoothed band energies
        float beatEnvelope;        ///< Beat-driven expansion envelope (0-1)
    };

#ifndef NATIVE_BUILD
    SbSpecBeatPsram* m_ps = nullptr;
#else
    SbSpecBeatPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
