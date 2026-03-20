/**
 * @file SbK1WaveformHarmonicEffect.h
 * @brief K1 Waveform Harmonic — pitch-mapped chroma dot constellation
 *
 * Instead of one dot driven by waveform amplitude, injects one dot per
 * active chroma bin at a fixed pitch-mapped position on the strip:
 *
 *   pos = 80 + (bin / 12.0) * 79   →   C=80, C#=87, D=93, ... B=153
 *
 * Each dot's brightness is that bin's energy (SB colour character: 1.5x
 * boost + led_share scaling). Trails, scroll, and centre mirror are
 * identical to K1 Waveform (0x1302).
 *
 * Visual result: harmony becomes spatial. A major chord produces three
 * coloured trails fanning from three pitch positions. Complex music fills
 * the strip; a single note fires one focused fountain.
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

class SbK1WaveformHarmonicEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1314; // EID_SB_K1_WAVEFORM_HARMONIC
    SbK1WaveformHarmonicEffect() = default;
    ~SbK1WaveformHarmonicEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t kHalfLength   = lightwaveos::effects::HALF_LENGTH;    // 80
    static constexpr uint16_t kStripLength  = lightwaveos::effects::STRIP_LENGTH;   // 160
    static constexpr uint16_t kCenterLeft   = lightwaveos::effects::CENTER_LEFT;    // 79
    static constexpr uint16_t kCenterRight  = lightwaveos::effects::CENTER_RIGHT;   // 80

    // sensitivity: audio gain — scales how easily quiet notes trigger dots
    // contrast: contrast curve iterations on chroma bins
    // chromaHue: manual hue offset added to bin palette positions
    float m_sensitivity = 1.0f;
    float m_contrast    = 1.0f;
    float m_chromaHue   = 0.0f;

    static constexpr uint8_t kParamCount = 3;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM-allocated trail buffer
    struct HarmonicPsram {
        CRGB_F trailBuffer[160];
        float scrollAccum;
    };

#ifndef NATIVE_BUILD
    HarmonicPsram* m_ps = nullptr;
#else
    HarmonicPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
