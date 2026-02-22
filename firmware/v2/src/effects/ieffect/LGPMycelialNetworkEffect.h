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

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPMycelialNetworkEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MYCELIAL_NETWORK;

    LGPMycelialNetworkEffect();
    ~LGPMycelialNetworkEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint16_t STRIP_LENGTH = 160;  // From CoreEffects.h

#ifndef NATIVE_BUILD
    struct MycelialPsram {
        float tipPositions[16];
        float tipVelocities[16];
        bool tipActive[16];
        float tipAge[16];
        float networkDensity[STRIP_LENGTH];
    };
    MycelialPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif
    float m_nutrientPhase;
    bool m_initialized;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
