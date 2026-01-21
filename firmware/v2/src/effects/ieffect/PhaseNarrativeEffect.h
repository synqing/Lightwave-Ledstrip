/**
 * @file PhaseNarrativeEffect.h
 * @brief Phase Narrative - Phase accumulation traveling waves with narrative arc envelope
 *
 * Effect ID: TBD
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_REACTIVE
 *
 * ## Mathematical Basis
 *
 * ### Phase Accumulation Traveling Waves
 * y(x,t) = sin(k * dist - phi), where phi += omega * dt
 *
 * The phase accumulator creates smooth traveling waves that propagate outward
 * from the center (LED 79/80). The wave equation:
 *
 *   brightness[i] = sin((2*PI/wavelength) * dist - phase(t)) * narrativeIntensity
 *
 * Where:
 * - wavelength: Spatial frequency of the wave pattern
 * - dist: Distance from center (via centerPairDistance())
 * - phase: Continuously accumulating phase (creates motion)
 * - narrativeIntensity: 0-1 envelope from state machine
 *
 * ### Narrative Arc State Machine
 * The effect follows a 4-phase story arc that modulates wave amplitude:
 *
 * 1. REST (0.2s default) - Intensity = 0, waiting for beat trigger
 * 2. BUILD (0.8s default) - Intensity ramps 0->1 via easeInQuad
 * 3. HOLD (0.3s default) - Intensity = 1, waves at full amplitude, phase speed 1.5x
 * 4. RELEASE (0.6s default) - Intensity decays 1->0 via easeOutQuad
 *
 * ### Audio Integration
 * - Beat detection triggers BUILD phase from REST
 * - Optional: RMS modulates wavelength (shorter waves at higher energy)
 * - Optional: Treble energy adds shimmer layer
 *
 * ## Layer Audit (2026-01-11)
 *
 * ### Layers
 * 1. Narrative Conductor: REST/BUILD/HOLD/RELEASE state machine controlling envelope
 * 2. Phase Accumulator: Traveling wave generator with continuous phase
 * 3. Wave Renderer: sin(k*dist - phase) with amplitude modulation
 * 4. Dual-Strip: Strip 2 renders with +90 hue offset
 *
 * ### Musical Fit
 * Best for: RHYTHMIC_DRIVEN music (EDM, electronic)
 * Secondary: HARMONIC_DRIVEN (chord changes can trigger new narrative cycles)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class PhaseNarrativeEffect : public plugins::IEffect {
public:
    PhaseNarrativeEffect();
    ~PhaseNarrativeEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // =========================================================================
    // Narrative Arc State Machine
    // =========================================================================
    enum class NarrativePhase { REST, BUILD, HOLD, RELEASE };
    NarrativePhase m_narrativePhase = NarrativePhase::REST;
    float m_phaseTime = 0.0f;        ///< Time in current narrative phase (seconds)
    float m_intensity = 0.0f;        ///< Current narrative intensity (0.0-1.0)

    // Narrative timing (seconds)
    float m_buildDur = 0.8f;         ///< Duration of BUILD phase
    float m_holdDur = 0.3f;          ///< Duration of HOLD phase
    float m_releaseDur = 0.6f;       ///< Duration of RELEASE phase
    float m_restDur = 0.2f;          ///< Minimum REST duration before re-trigger

    // =========================================================================
    // Phase Accumulation Wave Parameters
    // =========================================================================
    float m_phase = 0.0f;            ///< Accumulated wave phase (radians)
    float m_wavelength = 25.0f;      ///< Spatial wavelength (LEDs per cycle)
    bool m_outward = true;           ///< Wave direction: true=outward, false=inward

    // =========================================================================
    // Audio State
    // =========================================================================
    bool m_lastBeat = false;         ///< Edge detection for beat triggers
    float m_speedSmooth = 1.0f;      ///< Slew-limited speed (prevents jitter)

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * @brief Update narrative state machine
     * @param dt Delta time in seconds
     */
    void updateNarrative(float dt);

    /**
     * @brief Quadratic ease-in (slow start, fast end)
     * @param t Normalized time [0, 1]
     * @return Eased value [0, 1]
     */
    static float easeInQuad(float t);

    /**
     * @brief Quadratic ease-out (fast start, slow end)
     * @param t Normalized time [0, 1]
     * @return Eased value [0, 1]
     */
    static float easeOutQuad(float t);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
