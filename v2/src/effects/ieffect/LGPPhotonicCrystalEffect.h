/**
 * @file LGPPhotonicCrystalEffect.h
 * @brief LGP Photonic Crystal - Bandgap structure simulation
 * 
 * Effect ID: 33
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phase: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPhotonicCrystalEffect : public plugins::IEffect {
public:
    LGPPhotonicCrystalEffect();
    ~LGPPhotonicCrystalEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_phase;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
