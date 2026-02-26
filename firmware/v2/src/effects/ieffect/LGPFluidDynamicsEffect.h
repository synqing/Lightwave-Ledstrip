/**
 * @file LGPFluidDynamicsEffect.h
 * @brief LGP Fluid Dynamics - Fluid flow simulation
 * 
 * Effect ID: 39
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_velocity/m_pressure: Flow fields
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPFluidDynamicsEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_FLUID_DYNAMICS;

    LGPFluidDynamicsEffect();
    ~LGPFluidDynamicsEffect() override = default;

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
#ifndef NATIVE_BUILD
    struct FluidPsram {
        float velocity[STRIP_LENGTH];
        float pressure[STRIP_LENGTH];
    };
    FluidPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif
    uint16_t m_time;
    float m_turbulenceScale;
    float m_velocityDecay;
    float m_pressureDecay;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
