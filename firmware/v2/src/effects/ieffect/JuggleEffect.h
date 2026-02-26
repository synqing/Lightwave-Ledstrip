/**
 * @file JuggleEffect.h
 * @brief Juggle - Multiple colored balls juggling
 * 
 * Effect ID: 5
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class JuggleEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_JUGGLE;

    JuggleEffect();
    ~JuggleEffect() override = default;

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
    float m_chromaAngle = 0.0f;  // Circular chroma hue smoothing state
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

