/**
 * @file LGPPerlinShocklinesAmbientEffect.h
 * @brief LGP Perlin Shocklines Ambient - Time-driven travelling ridges (ambient)
 * 
 * Effect ID: 82
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Ambient: Time-driven only, periodic shockwave injection
 * 
 * Instance State:
 * - m_shockBuffer[STRIP_LENGTH]: Shockwave energy buffer
 * - m_noiseX, m_noiseY: Base noise field coordinates
 * - m_time: Time accumulator
 * - m_lastShockTime: Last shockwave injection time
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinShocklinesAmbientEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_SHOCKLINES_AMBIENT;

    LGPPerlinShocklinesAmbientEffect();
    ~LGPPerlinShocklinesAmbientEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint16_t STRIP_LENGTH = 160;

#ifndef NATIVE_BUILD
    struct ShocklinesPsram {
        float shockBuffer[STRIP_LENGTH];
    };
    ShocklinesPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

    // Noise field coordinates
    uint16_t m_noiseX;
    uint16_t m_noiseY;
    
    // Time accumulator
    uint16_t m_time;
    
    // Periodic shockwave timing
    uint32_t m_lastShockTime;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

