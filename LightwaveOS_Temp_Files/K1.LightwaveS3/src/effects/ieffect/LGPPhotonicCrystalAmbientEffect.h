/**
 * @file LGPPhotonicCrystalAmbientEffect.h
 * @brief LGP Photonic Crystal (Ambient) - Time-driven bandgap structure
 * 
 * Original clean implementation from cd67598~5, restored as ambient variant.
 * Simple periodic lattice structure without audio dependencies.
 * 
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPhotonicCrystalAmbientEffect : public plugins::IEffect {
public:
    LGPPhotonicCrystalAmbientEffect();
    ~LGPPhotonicCrystalAmbientEffect() override = default;

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
