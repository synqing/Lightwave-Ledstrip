/**
 * @file LGPBoxWaveEffect.h
 * @brief LGP Box Wave - Square wave standing patterns from center
 * 
 * Effect ID: 13
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | STANDING
 * 
 * Instance State:
 * - m_boxMotionPhase: Phase accumulator for box motion
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBoxWaveEffect : public plugins::IEffect {
public:
    LGPBoxWaveEffect();
    ~LGPBoxWaveEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_boxMotionPhase;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
