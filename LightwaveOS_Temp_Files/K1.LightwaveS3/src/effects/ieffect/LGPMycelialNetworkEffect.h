/**
 * @file LGPMycelialNetworkEffect.h
 * @brief LGP Mycelial Network - Fungal network expansion
 * 
 * Effect ID: 63
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_tipPositions/m_tipVelocities/m_tipActive/m_tipAge: Growth tip state
 * - m_networkDensity: Per-LED nutrient density
 * - m_nutrientPhase/m_initialized
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPMycelialNetworkEffect : public plugins::IEffect {
public:
    LGPMycelialNetworkEffect();
    ~LGPMycelialNetworkEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint16_t STRIP_LENGTH = 160;  // From CoreEffects.h

    float m_tipPositions[16];
    float m_tipVelocities[16];
    bool m_tipActive[16];
    float m_tipAge[16];
    float m_networkDensity[STRIP_LENGTH];
    float m_nutrientPhase;
    bool m_initialized;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
