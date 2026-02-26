/**
 * @file LGPFresnelZonesEffect.h
 * @brief LGP Fresnel Zones - Fresnel lens zone plate pattern
 * 
 * Effect ID: 32
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPFresnelZonesEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_FRESNEL_ZONES;

    LGPFresnelZonesEffect();
    ~LGPFresnelZonesEffect() override = default;

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
    int m_zoneCount;
    int m_edgeThreshold;
    float m_strip2Scale;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
