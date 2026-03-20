/**
 * @file MotionShaper.h
 * @brief Layer 3: Temporal envelope shaping driven by motion-semantic state
 *
 * Sits between MotionSemanticEngine (Layer 2) and render primitives.
 * Detects onsets, selects envelope type from semantic state, evaluates
 * each frame. Effects consume shaped values as multipliers on existing behavior.
 *
 * Cost: ~100 bytes RAM, <0.05ms/frame. No heap allocation.
 */

#pragma once

#include "ControlBus.h"
#include "MotionSemantics.h"
#include "../../effects/enhancement/TemporalOperator.h"

namespace lightwaveos::audio {

/**
 * @brief Shaped output values for effect consumption.
 */
struct MotionShaping {
    float intensity    = 0.0f;   ///< Envelope-shaped onset response [0-1]
    float decayMs      = 300.0f; ///< Recommended decay time from fluidity [150-600ms]
    float accentScale  = 1.0f;   ///< Syncopation-derived accent multiplier [0.7-1.3]
    uint8_t envType    = 0;      ///< 0=none, 1=impact, 2=standard, 3=dramatic
    bool active        = false;  ///< True during active envelope
};

/**
 * @brief Onset-driven temporal envelope selector and evaluator.
 *
 * Mapping (from semantic state at onset time):
 *   - High Impulse (>0.6) + High Weight (>0.5) -> Impact envelope (attack+recoil+decay)
 *   - High syncopation (>0.5) -> Dramatic envelope (anticipation+attack+follow-through)
 *   - High Fluidity (>0.7) + Low Impulse (<0.4) -> Standard smooth envelope
 *   - Default -> Standard moderate envelope
 */
class MotionShaper {
public:
    void update(const ControlBusFrame& bus, const MotionSemanticFrame& motion, uint32_t now_ms) {
        // Only process on new audio hops (avoid re-triggering at 120 FPS)
        bool newHop = (bus.hop_seq != m_lastHopSeq);
        m_lastHopSeq = bus.hop_seq;

        if (newHop) {
            bool onset = (bus.fast_flux > 0.1f);
            bool risingEdge = onset && !m_prevOnset;
            m_prevOnset = onset;

            if (risingEdge) {
                // Min re-trigger guard (80ms)
                if (now_ms - m_lastTriggerMs >= 80) {
                    selectAndTrigger(motion, bus.syncopation_level, now_ms);
                }
            }
        }

        // Evaluate active envelope
        if (m_out.active) {
            float rawEnv = m_envelope.eval(now_ms);
            m_out.intensity = (rawEnv < 0.0f) ? 0.0f : ((rawEnv > 1.0f) ? 1.0f : rawEnv);
            if (m_envelope.isDone(now_ms)) {
                m_out.active = false;
                m_out.intensity = 0.0f;
                m_out.envType = 0;
            }
        }

        // Continuous shaping from semantic axes
        // Decay time: fluidity scales 150ms (jerky) to 600ms (fluid)
        m_out.decayMs = 150.0f + 450.0f * motion.fluidity;
        // Accent: syncopation boosts [0.7 - 1.3]
        m_out.accentScale = 0.7f + 0.6f * bus.syncopation_level;
    }

    const MotionShaping& shaping() const { return m_out; }

private:
    MotionShaping m_out{};
    effects::enhancement::TemporalEnvelope m_envelope{};
    uint32_t m_lastHopSeq = 0;
    uint32_t m_lastTriggerMs = 0;
    bool m_prevOnset = false;

    void selectAndTrigger(const MotionSemanticFrame& motion, float syncopation, uint32_t now_ms) {
        using namespace effects::enhancement;

        float impulse = motion.impulse_strength;
        float weight = motion.weight;
        float fluidity = motion.fluidity;

        if (impulse > 0.6f && weight > 0.5f) {
            // Strong hit -> impact (attack + recoil + decay)
            m_envelope = makeImpactEnvelope(0.0f, 1.0f);
            m_out.envType = 1;
        } else if (syncopation > 0.5f) {
            // Off-beat accent -> dramatic (anticipation + attack + follow-through)
            m_envelope = makeDramaticEnvelope(0.0f, 0.9f);
            m_out.envType = 3;
        } else if (fluidity > 0.7f && impulse < 0.4f) {
            // Fluid, gentle -> standard smooth
            m_envelope = makeStandardEnvelope(0.0f, 0.8f);
            m_out.envType = 2;
        } else {
            // Default moderate
            m_envelope = makeStandardEnvelope(0.0f, 0.7f);
            m_out.envType = 2;
        }

        m_envelope.trigger(now_ms);
        m_out.active = true;
        m_lastTriggerMs = now_ms;
    }
};

} // namespace lightwaveos::audio
