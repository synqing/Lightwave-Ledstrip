/**
 * @file LGPPerlinInterferenceWeaveAmbientEffect.h
 * @brief LGP Perlin Interference Weave Ambient - Dual-strip moiré (time-driven)
 * 
 * Effect ID: 84
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | MOIRE
 * 
 * Ambient: Time-driven phase offset modulation
 * 
 * Instance State:
 * - m_noiseX, m_noiseY: Base noise field coordinates
 * - m_phaseOffset: Phase offset between strips (time-modulated)
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

class LGPPerlinInterferenceWeaveAmbientEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT;

    LGPPerlinInterferenceWeaveAmbientEffect();
    ~LGPPerlinInterferenceWeaveAmbientEffect() override = default;

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
    
    // Phase offset between strips
    float m_phaseOffset;
    
    // Time accumulator
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

