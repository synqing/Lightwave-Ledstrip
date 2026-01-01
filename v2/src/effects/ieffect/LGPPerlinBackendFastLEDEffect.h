/**
 * @file LGPPerlinBackendFastLEDEffect.h
 * @brief Perlin Backend Test A: FastLED inoise8 baseline
 * 
 * Effect ID: 85 (TEST)
 * Family: EXPERIMENTAL
 * Tags: CENTER_ORIGIN, TEST
 * 
 * Test effect for comparing noise backends.
 * Uses FastLED's inoise8() with proper coordinate scaling (no >>8 collapse).
 * Centre-origin mapping with seed + advection for organic motion.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinBackendFastLEDEffect : public plugins::IEffect {
public:
    LGPPerlinBackendFastLEDEffect();
    ~LGPPerlinBackendFastLEDEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Seed for "non-reproducible" feel (set once in init)
    uint32_t m_seed;
    
    // Noise field coordinates (advection over time)
    uint16_t m_noiseX;
    uint16_t m_noiseY;
    uint16_t m_noiseZ;
    
    // Audio-driven momentum (Emotiscope-style)
    float m_momentum;
    
    // Time accumulator
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

