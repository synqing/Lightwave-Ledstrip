/**
 * @file FireEffect.h
 * @brief Fire - Realistic fire simulation radiating from center
 * 
 * Effect ID: 0
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_fireHeat[STRIP_LENGTH]: Heat array for fire simulation
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

class FireEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_FIRE;

    FireEffect();
    ~FireEffect() override = default;

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
    static constexpr uint16_t STRIP_LENGTH = 160;
#ifndef NATIVE_BUILD
    struct FirePsram {
        byte fireHeat[STRIP_LENGTH];
    };
    FirePsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

