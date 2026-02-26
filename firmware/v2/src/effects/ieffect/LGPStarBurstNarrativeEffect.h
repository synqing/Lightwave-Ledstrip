/**
 * @file LGPStarBurstNarrativeEffect.h
 * @brief LGP Star Burst (Narrative) - legacy starburst core with phrase-gated narrative state
 *
 * Effect ID: 74
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 *
 * Design intent:
 * - Keep the recognisable legacy starburst render core.
 * - Keep lightweight narrative phrasing (REST/BUILD/HOLD/RELEASE).
 * - Keep phrase-gated chord commits for stable harmonic colouring.
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

class LGPStarBurstNarrativeEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_STAR_BURST_NARRATIVE;

    LGPStarBurstNarrativeEffect();
    ~LGPStarBurstNarrativeEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    enum class StoryPhase { REST, BUILD, HOLD, RELEASE };

    // Narrative phrase state
    StoryPhase m_storyPhase = StoryPhase::REST;
    float m_storyTimeS = 0.0f;
    float m_quietTimeS = 0.0f;
    float m_phraseHoldS = 0.0f;

    // Legacy starburst motion state
    float m_phase = 0.0f;
    float m_burst = 0.0f;
    uint32_t m_lastHopSeq = 0;

    // Harmonic colour state
    uint8_t m_candidateRootBin = 0;
    bool m_candidateMinor = false;
    uint8_t m_keyRootBin = 0;
    bool m_keyMinor = false;
    uint8_t m_dominantBin = 0;
    float m_keyRootAngle = 0.0f;     // Circular EMA angle for key root (radians)

    // Audio smoothing for stable speed coupling
    enhancement::Spring m_phaseSpeedSpring;
    float m_heavyBassSmooth = 0.0f;
    bool m_heavyBassSmoothInitialised = false;

    // Kick envelope for beat-locked motion
    float m_kickEnv = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
