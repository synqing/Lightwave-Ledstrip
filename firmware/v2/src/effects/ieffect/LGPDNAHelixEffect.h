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
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPDNAHelixEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_DNA_HELIX;

    LGPDNAHelixEffect();
    ~LGPDNAHelixEffect() override = default;

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
    float m_rotation;
    float m_rotationRate;
    float m_helixPitch;
    float m_linkDensity;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
