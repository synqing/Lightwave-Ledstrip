/**
 * @file LGPPhotonicCrystalEffect.h
 * @brief LGP Photonic Crystal - Bandgap structure simulation with audio layering
 *
 * Effect ID: 33
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN, AUDIO_REACTIVE
 *
 * v7: RESTORED V1 ALGORITHM + PROPER AUDIO LAYERING
 *
 * The base algorithm is the ORIGINAL v1 photonic crystal pattern:
 * - Lattice size from ctx.complexity (4-10 LEDs per cell)
 * - Defect probability from ctx.variation
 * - Bandgap test: cellPosition < (latticeSize >> 1)
 * - Wave patterns: sin8((distFromCenter << 2) - (phase >> 7))
 *
 * Audio reactivity is LAYERED ON TOP (not baked in):
 * - Speed modulation: spring-smoothed bass energy
 * - Brightness modulation: 0.4 + 0.5 * energyAvg + 0.4 * energyDelta
 * - Collision flash: snare-triggered, spatial decay from center
 * - Color offset: chroma dominant bin (smoothed over 250ms)
 *
 * This follows the proven pattern from WaveCollision/Chevron/StarBurst effects.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include "ChromaUtils.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPhotonicCrystalEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PHOTONIC_CRYSTAL;

    LGPPhotonicCrystalEffect();
    ~LGPPhotonicCrystalEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // =========================================================================
    // PHASE ACCUMULATOR (float for dt-based advancement)
    // =========================================================================
    float m_phase = 0.0f;

    // =========================================================================
    // AUDIO SAMPLING (per-hop only, NOT per-frame)
    // =========================================================================
    uint32_t m_lastHopSeq = 0;

    // =========================================================================
    // ENERGY HISTORY (4-hop rolling average - proven pattern)
    // =========================================================================
    static constexpr uint8_t ENERGY_HISTORY = 4;
    float m_energyHist[ENERGY_HISTORY] = {0};
    float m_energySum = 0.0f;
    uint8_t m_energyHistIdx = 0;
    float m_energyAvg = 0.0f;      // Rolling average of recent energy
    float m_energyDelta = 0.0f;    // Current energy ABOVE average (positive only)

    // =========================================================================
    // ASYMMETRIC FOLLOWERS (frame-rate independent smoothing)
    // riseTau = fast attack, fallTau = slow decay
    // =========================================================================
    enhancement::AsymmetricFollower m_energyAvgFollower{0.5f, 0.08f, 0.20f};
    enhancement::AsymmetricFollower m_energyDeltaFollower{0.0f, 0.05f, 0.15f};

    // =========================================================================
    // SPRING PHYSICS FOR SPEED (stiffness=50, critically damped)
    // Prevents lurching from rapid audio changes
    // =========================================================================
    enhancement::Spring m_speedSpring;

    // =========================================================================
    // COLLISION FLASH (snare-triggered, spatial decay from center)
    // =========================================================================
    float m_collisionBoost = 0.0f;
    float m_lastFastFlux = 0.0f;  // Backend-agnostic transient proxy for collision flash

    // =========================================================================
    // CIRCULAR CHROMA HUE (replaces argmax + linear EMA for colour offset)
    // =========================================================================
    float m_chromaAngle = 0.0f;      // Persistent angle for circular EMA (radians)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
