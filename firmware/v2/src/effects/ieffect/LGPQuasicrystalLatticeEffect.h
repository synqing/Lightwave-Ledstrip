/**
 * @file LGPQuasicrystalLatticeEffect.h
 * @brief LGP Quasicrystal Lattice - Penrose-like incommensurate interference lattice
 *
 * Effect ID: 0x1B03 (EID_LGP_QUASICRYSTAL_LATTICE)
 * Family: SHOWPIECE_PACK3
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | LATTICE | INTERFERENCE
 *
 * Physics: Five spatially-locked sinusoidal components with mutually
 * incommensurate Fibonacci-ratio frequencies (13, 21, 34, 55, 89) create
 * quasi-periodic structure. Nonlinear threshold extraction sharpens the
 * sum into crisp lattice nodes/antinodes -- an optical lattice, not a
 * wavy gradient.
 *
 * Instance State (~24 bytes, no PSRAM):
 * - m_timeA, m_timeB: float accumulators for independent time phases
 * - m_chromaAngle: float circular EMA state for audio hue smoothing
 * - m_rmsSmooth: float smoothed RMS for time-phase modulation
 * - m_beatFlash: float decaying beat brightness boost
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPQuasicrystalLatticeEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_QUASICRYSTAL_LATTICE;

    LGPQuasicrystalLatticeEffect();
    ~LGPQuasicrystalLatticeEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_timeA;          ///< Primary time phase accumulator
    float m_timeB;          ///< Secondary time phase accumulator (incommensurate drift)
    float m_chromaAngle;    ///< Circular EMA state for audio chroma hue
    float m_rmsSmooth;      ///< Smoothed RMS for time-phase speed modulation
    float m_beatFlash;      ///< Decaying beat brightness boost (0..1)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
