/**
 * @file LGPCrystallineGrowthEffect.h
 * @brief LGP Crystalline Growth - Growing crystal facets
 * 
 * Effect ID: 38
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_seeds/m_size/m_hue: Crystal growth parameters
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPCrystallineGrowthEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CRYSTALLINE_GROWTH;

    LGPCrystallineGrowthEffect();
    ~LGPCrystallineGrowthEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    uint8_t m_seeds[10];
    uint8_t m_size[10];
    uint8_t m_hue[10];
    bool m_initialized;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
