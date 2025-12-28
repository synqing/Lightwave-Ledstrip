/**
 * @file LGPStarBurstNarrativeEffect.h
 * @brief LGP Star Burst (Narrative) - Story conductor + chord-based coloring
 *
 * Effect ID: 74
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStarBurstNarrativeEffect : public plugins::IEffect {
public:
    LGPStarBurstNarrativeEffect();
    ~LGPStarBurstNarrativeEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Story Conductor state
    enum class StoryPhase { REST, BUILD, HOLD, RELEASE };
    StoryPhase m_storyPhase = StoryPhase::REST;
    float m_storyTimeS = 0.0f;
    float m_quietTimeS = 0.0f;
    float m_phraseHoldS = 0.0f;
    float m_chordChangePulse = 0.0f;  // Snare-driven pulse event in HOLD phase

    // Key/palette gating
    uint8_t m_candidateRootBin = 0;
    bool m_candidateMinor = false;
    uint8_t m_keyRootBin = 0;
    bool m_keyMinor = false;
    float m_keyRootBinSmooth = 0.0f;

    // Audio features
    float m_phase;
    float m_burst = 0.0f;
    uint32_t m_lastHopSeq = 0;
    static constexpr uint8_t CHROMA_HISTORY = 4;
    float m_chromaEnergyHist[CHROMA_HISTORY] = {0.0f};
    float m_chromaEnergySum = 0.0f;
    uint8_t m_chromaHistIdx = 0;
    float m_energyAvg = 0.0f;
    float m_energyDelta = 0.0f;
    uint8_t m_dominantBin = 0;
    float m_energyAvgSmooth = 0.0f;
    float m_energyDeltaSmooth = 0.0f;
    float m_dominantBinSmooth = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
