/**
 * @file SbK1WaveformHybridEffect.h
 * @brief K1 Waveform with SB 3.0.0 colour synthesis character
 *
 * Keeps the K1-native bouncing dot + trail visual concept but applies
 * three SB 3.0.0 colour synthesis corrections:
 *   1. 1.5x brightness boost after contrast squaring
 *   2. led_share scaling (1/12 per chroma bin)
 *   3. Temporal RGB smoothing (0.05/0.95 EMA, ~20-frame inertia)
 *
 * The result: more nuanced colour mixing where individual chroma bins
 * contribute proportionally, smoother colour transitions, and the SB
 * brightness curve. Visual layout (dot, trails, scroll, mirror) is
 * unchanged from K1 Waveform (0x1302).
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

class SbK1WaveformHybridEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1313; // EID_SB_K1_WAVEFORM_HYBRID
    SbK1WaveformHybridEffect() = default;
    ~SbK1WaveformHybridEffect() override = default;

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

    // Effect parameters (same as K1 Waveform)
    float m_sensitivity = 1.0f;
    float m_contrast    = 1.0f;
    float m_chromaHue   = 0.0f;

    static constexpr uint8_t kParamCount = 3;
    static const plugins::EffectParameter s_params[kParamCount];

    // SB colour smoothing state (DRAM — 12 bytes)
    CRGB_F m_dotColorSmooth = {0.0f, 0.0f, 0.0f};

    // PSRAM-allocated trail buffer for float-precision persistence
    struct HybridPsram {
        CRGB_F trailBuffer[160];
        float scrollAccum;  // Sub-pixel scroll accumulator
    };

#ifndef NATIVE_BUILD
    HybridPsram* m_ps = nullptr;
#else
    HybridPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
