/**
 * @file LGPDNAHelixEffect.h
 * @brief LGP DNA Helix - Double helix structure
 * 
 * Effect ID: 56
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_rotation: Helix rotation accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPDNAHelixEffect : public plugins::IEffect {
public:
    LGPDNAHelixEffect();
    ~LGPDNAHelixEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_rotation;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
