/**
 * @file LGPOpalFilmEffect.h
 * @brief LGP Opal Film - Thin-film iridescence bands
 *
 * Effect ID: 124
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | DEPTH
 *
 * Instance State:
 * - m_time: Global time accumulator
 * - m_flow: Secondary drift accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPOpalFilmEffect : public plugins::IEffect {
public:
    LGPOpalFilmEffect();
    ~LGPOpalFilmEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_time;
    float m_flow;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
