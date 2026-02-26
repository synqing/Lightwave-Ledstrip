/**
 * @file LGPSpectrumBarsEffect.h
 * @brief 8-band spectrum analyzer from center outward
 *
 * Band 0 (bass) at center, Band 7 (treble) at edges.
 * Bar height shows energy, color from palette.
 *
 * Effect ID: 70 (audio demo)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | SPECTRUM
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSpectrumBarsEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SPECTRUM_BARS;

    LGPSpectrumBarsEffect() = default;
    ~LGPSpectrumBarsEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;


    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;
private:
    float m_smoothedBands[8] = {0};  // Smoothed band values for animation
    float m_chromaAngle = 0.0f;      ///< Circular chroma EMA state (radians)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
