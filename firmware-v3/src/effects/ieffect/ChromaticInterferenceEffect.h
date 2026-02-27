/**
 * @file ChromaticInterferenceEffect.h
 * @brief LGP Chromatic Interference - Dual-edge injection with dispersion, interference patterns
 * 
 * Effect ID: 67
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | SPECTRAL
 * LGP-Sensitive: YES (interference patterns + chromatic dispersion)
 * 
 * Instance State:
 * - m_interferencePhase: Phase accumulator for interference animation
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ChromaticInterferenceEffect : public plugins::IEffect {
public:
    ChromaticInterferenceEffect();
    ~ChromaticInterferenceEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static float interferencePhase)
    float m_interferencePhase;
    
    // Helper: chromaticDispersionPalette (moved from static in LGPChromaticEffects.cpp)
    CRGB chromaticDispersionPalette(float position,
                                   float aberration,
                                   float phase,
                                   float intensity,
                                   const plugins::PaletteRef& palette,
                                   uint8_t baseHue) const;
    
    // Constants
    static constexpr float PHASE_SPEED = 0.012f;
    static constexpr float INTERFERENCE_MODULATION = 0.5f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

