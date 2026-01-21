/**
 * @file NarrativePerlinEffect.h
 * @brief Narrative Perlin - Story conductor + organic Perlin noise texture
 *
 * Effect ID: TBD
 * Family: AMBIENT
 * Tags: CENTER_ORIGIN
 *
 * ## Layer Audit
 *
 * ### Layers
 * 1. NarrativeArc: BUILD/HOLD/RELEASE/REST state machine controlling intensity envelope
 * 2. Perlin Noise Field: Organic texture with complexity scaled by narrative intensity
 * 3. Beat Trigger: Starts new BUILD phase on beat detection (when resting)
 * 4. Dual Strip Rendering: Strip 2 has +90 hue offset for LGP interference
 *
 * ### Musical Fit
 * Best for: Any music style - responds to beats universally
 * Behavior: Builds tension on beats, holds briefly, releases smoothly
 *
 * ### State Machine (NarrativeArc)
 *
 * BUILD (1.5s default):
 *   intensity = easeInQuad(t / buildDuration)
 *   Noise complexity increases, brightness ramps up
 *
 * HOLD (0.4s default):
 *   intensity = 1.0 + breatheAmount * sin(t * breatheFreq * TWO_PI)
 *   Subtle breathing at peak intensity
 *
 * RELEASE (1.0s default):
 *   intensity = 1.0 - easeOutQuad(t / releaseDuration)
 *   Graceful decay back to rest
 *
 * REST (0.5s minimum):
 *   intensity = 0
 *   Waiting for next beat trigger
 *
 * ### Perlin Modulation
 * - Octaves: 1-3 based on intensity (more detail at peak)
 * - Amplitude: Scales with intensity
 * - Speed: Constant (time-based, not audio-driven to prevent jitter)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ============================================================================
// Easing Functions (inline for performance)
// ============================================================================

/**
 * @brief Quadratic ease-in: starts slow, accelerates
 * @param t Normalized time (0.0 to 1.0)
 * @return Eased value (0.0 to 1.0)
 */
inline float easeInQuad(float t) {
    return t * t;
}

/**
 * @brief Quadratic ease-out: starts fast, decelerates
 * @param t Normalized time (0.0 to 1.0)
 * @return Eased value (0.0 to 1.0)
 */
inline float easeOutQuad(float t) {
    return t * (2.0f - t);
}

/**
 * @brief Quadratic ease-in-out: slow start, fast middle, slow end
 * @param t Normalized time (0.0 to 1.0)
 * @return Eased value (0.0 to 1.0)
 */
inline float easeInOutQuad(float t) {
    return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
}

// ============================================================================
// NarrativeArc Class - Story Conductor State Machine
// ============================================================================

/**
 * @brief State machine for narrative intensity envelope
 *
 * Controls the dramatic arc of visual effects:
 * - BUILD: Tension rises (slow attack)
 * - HOLD: Peak intensity with subtle breathing
 * - RELEASE: Graceful decay
 * - REST: Waiting for next trigger
 */
class NarrativeArc {
public:
    enum class Phase { BUILD, HOLD, RELEASE, REST };

    NarrativeArc()
        : m_phase(Phase::REST)
        , m_phaseTime(0.0f)
        , m_buildDuration(1.5f)
        , m_holdDuration(0.4f)
        , m_releaseDuration(1.0f)
        , m_restMinDuration(0.5f)
        , m_breatheAmount(0.08f)
        , m_breatheFreq(1.5f)
    {}

    /**
     * @brief Configure arc durations
     * @param build Build phase duration (seconds)
     * @param hold Hold phase duration (seconds)
     * @param release Release phase duration (seconds)
     * @param restMin Minimum rest duration (seconds)
     */
    void setDurations(float build, float hold, float release, float restMin) {
        m_buildDuration = build;
        m_holdDuration = hold;
        m_releaseDuration = release;
        m_restMinDuration = restMin;
    }

    /**
     * @brief Configure hold phase breathing
     * @param amount Breath amplitude (0.0-0.2 typical)
     * @param freq Breath frequency (Hz)
     */
    void setBreathing(float amount, float freq) {
        m_breatheAmount = amount;
        m_breatheFreq = freq;
    }

    /**
     * @brief Trigger a new narrative arc (starts BUILD phase)
     *
     * Only triggers if currently in REST phase and minimum rest time has elapsed.
     * @return true if arc was triggered
     */
    bool trigger() {
        if (m_phase == Phase::REST && m_phaseTime >= m_restMinDuration) {
            m_phase = Phase::BUILD;
            m_phaseTime = 0.0f;
            return true;
        }
        return false;
    }

    /**
     * @brief Force trigger regardless of current state
     *
     * Use sparingly - bypasses the natural flow.
     */
    void forceTrigger() {
        m_phase = Phase::BUILD;
        m_phaseTime = 0.0f;
    }

    /**
     * @brief Update state machine and return current intensity
     * @param dt Delta time in seconds
     * @return Intensity value (0.0 to ~1.08, can exceed 1.0 during HOLD breathing)
     */
    float update(float dt) {
        m_phaseTime += dt;
        float intensity = 0.0f;

        switch (m_phase) {
            case Phase::BUILD: {
                float t = m_phaseTime / m_buildDuration;
                if (t >= 1.0f) {
                    // Transition to HOLD
                    m_phase = Phase::HOLD;
                    m_phaseTime = 0.0f;
                    intensity = 1.0f;
                } else {
                    intensity = easeInQuad(t);
                }
                break;
            }

            case Phase::HOLD: {
                if (m_phaseTime >= m_holdDuration) {
                    // Transition to RELEASE
                    m_phase = Phase::RELEASE;
                    m_phaseTime = 0.0f;
                    intensity = 1.0f;
                } else {
                    // Subtle breathing at peak
                    float breath = sinf(m_phaseTime * m_breatheFreq * 6.28318530718f);
                    intensity = 1.0f + m_breatheAmount * breath;
                }
                break;
            }

            case Phase::RELEASE: {
                float t = m_phaseTime / m_releaseDuration;
                if (t >= 1.0f) {
                    // Transition to REST
                    m_phase = Phase::REST;
                    m_phaseTime = 0.0f;
                    intensity = 0.0f;
                } else {
                    intensity = 1.0f - easeOutQuad(t);
                }
                break;
            }

            case Phase::REST:
            default:
                intensity = 0.0f;
                break;
        }

        return intensity;
    }

    /**
     * @brief Check if narrative arc is currently active (not in REST)
     * @return true if in BUILD, HOLD, or RELEASE phase
     */
    bool isActive() const {
        return m_phase != Phase::REST;
    }

    /**
     * @brief Get current phase
     * @return Current narrative phase
     */
    Phase getPhase() const {
        return m_phase;
    }

    /**
     * @brief Reset to REST state
     */
    void reset() {
        m_phase = Phase::REST;
        m_phaseTime = 0.0f;
    }

private:
    Phase m_phase;
    float m_phaseTime;
    float m_buildDuration;
    float m_holdDuration;
    float m_releaseDuration;
    float m_restMinDuration;
    float m_breatheAmount;
    float m_breatheFreq;
};

// ============================================================================
// NarrativePerlinEffect Class
// ============================================================================

class NarrativePerlinEffect : public plugins::IEffect {
public:
    NarrativePerlinEffect();
    ~NarrativePerlinEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Story conductor
    NarrativeArc m_arc;

    // Beat detection state
    bool m_lastBeat = false;
    uint32_t m_lastHopSeq = 0;

    // Noise field coordinates (16-bit for wide range)
    uint16_t m_noiseX = 0;
    uint16_t m_noiseY = 0;
    uint16_t m_noiseZ = 0;

    // Perlin parameters modulated by narrative intensity
    uint8_t m_octaves = 1;           // 1-3 based on intensity
    float m_amplitudeScale = 0.0f;   // 0-1 based on intensity

    // Audio smoothing
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_fluxFollower{0.0f, 0.08f, 0.25f};
    float m_targetRms = 0.0f;
    float m_targetFlux = 0.0f;
    float m_smoothRms = 0.0f;
    float m_smoothFlux = 0.0f;

    // Optional: RMS-based early trigger threshold
    float m_energySpikeThreshold = 0.8f;
    bool m_allowEnergyTrigger = true;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
