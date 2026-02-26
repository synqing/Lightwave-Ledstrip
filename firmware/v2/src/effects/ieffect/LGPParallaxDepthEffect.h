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
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPParallaxDepthEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PARALLAX_DEPTH;

    LGPParallaxDepthEffect();
    ~LGPParallaxDepthEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;


    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;
private:
    float m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
