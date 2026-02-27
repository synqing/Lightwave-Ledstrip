/**
 * @file ModalResonanceEffect.h
 * @brief LGP Modal Resonance - Explores different optical cavity resonance modes
 * 
 * Effect ID: 15
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | STANDING
 * LGP-Sensitive: YES (precise amplitude relationships for interference)
 * 
 * Instance State:
 * - m_modalModePhase: Phase accumulator for mode variation
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ModalResonanceEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_MODAL_RESONANCE;

    ModalResonanceEffect();
    ~ModalResonanceEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static float modalModePhase)
    float m_modalModePhase;
    
    // Mode parameters
    static constexpr float BASE_MODE_MIN = 5.0f;
    static constexpr float BASE_MODE_RANGE = 4.0f;
    static constexpr float HARMONIC_WEIGHT = 0.5f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

