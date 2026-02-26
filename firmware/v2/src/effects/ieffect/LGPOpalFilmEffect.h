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
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPOpalFilmEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_OPAL_FILM;

    LGPOpalFilmEffect();
    ~LGPOpalFilmEffect() override = default;

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
    float m_flow;
    float m_r = 1.00f;
    float m_g = 1.23f;
    float m_b = 1.55f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
