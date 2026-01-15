/**
 * @file LGPRileyDissonanceEffect.h
 * @brief LGP Riley Dissonance - Op-art visual vibration
 * 
 * Effect ID: 64
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_patternPhase
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRileyDissonanceEffect : public plugins::IEffect {
public:
    LGPRileyDissonanceEffect();
    ~LGPRileyDissonanceEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_patternPhase;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
