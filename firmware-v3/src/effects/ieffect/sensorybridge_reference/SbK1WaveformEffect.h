/**
 * @file SbK1WaveformEffect.h
 * @brief K1 Lightwave waveform dot mode (parity port)
 *
 * Port of K1 Lightwave's `light_mode_waveform()` (lightshow_modes.h:1393-1507).
 * This is the scrolling-dot waveform mode with chromagram-derived colour,
 * dynamic trail fade, and centre-origin mirroring.
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

class SbK1WaveformEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1302; // EID_SB_K1_WAVEFORM
    SbK1WaveformEffect() = default;
    ~SbK1WaveformEffect() override = default;

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
    float m_sensitivity = 1.0f;
    float m_contrast    = 1.0f;
    float m_chromaHue   = 0.0f;

    static constexpr uint8_t kParamCount = 3;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM-allocated trail buffer for float-precision persistence
    struct SbK1WaveformPsram {
        CRGB_F trailBuffer[160];
        float scrollAccum;  // Sub-pixel scroll accumulator for dt-independent scroll
    };

#ifndef NATIVE_BUILD
    SbK1WaveformPsram* m_ps = nullptr;
#else
    SbK1WaveformPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
