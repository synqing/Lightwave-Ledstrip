/**
 * @file LGPPerlinCausticsAmbientEffect.h
 * @brief LGP Perlin Caustics Ambient - Sparkling caustic lobes (time-driven)
 * 
 * Effect ID: 83
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Ambient: Time-driven only, slow parameter modulation
 * 
 * Instance State:
 * - m_noiseX, m_noiseY, m_noiseZ: Perlin noise field coordinates
 * - m_time: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinCausticsAmbientEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_CAUSTICS_AMBIENT;

    LGPPerlinCausticsAmbientEffect();
    ~LGPPerlinCausticsAmbientEffect() override = default;

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
    // Noise field coordinates
    uint16_t m_noiseX;
    uint16_t m_noiseY;
    uint16_t m_noiseZ;
    
    // Time accumulator
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

