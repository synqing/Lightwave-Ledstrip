/**
 * @file LGPParallaxDepthEffect.h
 * @brief LGP Parallax Depth - Two-layer refractive parallax
 *
 * Effect ID: 129
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | DEPTH
 *
 * Instance State:
 * - m_time: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPParallaxDepthEffect : public plugins::IEffect {
public:
    LGPParallaxDepthEffect();
    ~LGPParallaxDepthEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
