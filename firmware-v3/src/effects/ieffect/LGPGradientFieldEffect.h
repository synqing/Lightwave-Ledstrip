/**
 * @file LGPGradientFieldEffect.h
 * @brief LGP Gradient Field — operator-surfaced gradient proof effect
 *
 * Effect ID: 0x1F00 (FAMILY_GRADIENT)
 * Tags: CENTER_ORIGIN
 *
 * Dedicated gradient-native effect with 6 exposed parameters:
 *   basis, repeatMode, interpolation, spread, phaseOffset, edgeAsymmetry
 *
 * Proves the gradient kernel is usable from the operator's perspective:
 * all three coordinate bases, all three repeat modes, all three interpolation
 * modes, dual-edge differentiation, and parameter-driven ramp rebuild.
 *
 * Ramp is rebuilt on parameter change only (dirty flag), not every frame.
 * render() is lookup-only: sample precomputed ramp, write LEDs.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "../gradient/GradientRamp.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPGradientFieldEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_GRADIENT_FIELD;

    LGPGradientFieldEffect();
    ~LGPGradientFieldEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    // Parameter interface
    uint8_t getParameterCount() const override { return 6; }
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    void rebuildRamp(const plugins::EffectContext& ctx);

    // Coordinate basis enum (matches parameter values)
    enum class Basis : uint8_t { CENTER = 0, LOCAL = 1, SIGNED = 2 };

    // Parameters (runtime values)
    float m_basis;            // 0=CENTER, 1=LOCAL, 2=SIGNED
    float m_repeatMode;       // 0=CLAMP, 1=REPEAT, 2=MIRROR
    float m_interpolation;    // 0=LINEAR, 1=EASED, 2=HARD_STOP
    float m_spread;           // 0.1-2.0 (ramp scale factor)
    float m_phaseOffset;      // 0.0-1.0 (ramp offset)
    float m_edgeAsymmetry;    // 0.0-1.0 (0=symmetric, 1=fully differentiated)

    // Precomputed ramp (rebuilt on dirty flag)
    gradient::GradientRamp m_rampA;   ///< Primary ramp (strip 1 / both)
    gradient::GradientRamp m_rampB;   ///< Secondary ramp (strip 2 when asymmetric)
    bool m_dirty;                      ///< True when parameters changed

    // Animation
    float m_phase;

    // Static metadata and parameters
    static const plugins::EffectMetadata s_meta;
    static const plugins::EffectParameter s_params[6];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
