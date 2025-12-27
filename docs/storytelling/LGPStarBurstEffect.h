// LGPStarBurstEffect.h
/**
 * @file LGPStarBurstEffect.h
 * @brief LGP Star Burst - Narrative, center-origin, musically coherent LGP burst effect
 *
 * Effect ID: 24
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 *
 * Fixes applied:
 * - Adds a lightweight STORY CONDUCTOR (REST/BUILD/HOLD/RELEASE) so the effect has readable structure.
 * - Palette/key selection is "phrase-gated" (approximated via quiet->active energy transitions) so color doesn't jitter.
 * - Motion + trails are dt-aware (time-correct) instead of frame-rate dependent.
 * - Brightness + palette indexing are CLAMPED to avoid uint8 wrap artifacts.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStarBurstEffect : public plugins::IEffect {
public:
    LGPStarBurstEffect();
    ~LGPStarBurstEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ------------------------------------------------------------------------
    // STORY CONDUCTOR
    // ------------------------------------------------------------------------
    enum class StoryPhase : uint8_t {
        REST    = 0,
        BUILD   = 1,
        HOLD   = 2,
        RELEASE = 3
    };

    StoryPhase m_storyPhase = StoryPhase::REST;
    float      m_storyTimeS = 0.0f;    // time spent in current story phase
    float      m_quietTimeS = 0.0f;    // how long we've been "quiet"
    float      m_phraseHoldS = 0.0f;   // minimum time between palette/key commits

    // Candidate (updated each hop), committed only on "phrase boundary"
    uint8_t    m_candidateRootBin = 0;
    bool       m_candidateMinor = false;

    // Committed tonal center (stable), used to drive palette family
    uint8_t    m_keyRootBin = 0;
    bool       m_keyMinor = false;
    float      m_keyRootBinSmooth = 0.0f;

    // ------------------------------------------------------------------------
    // AUDIO FEATURES (low-level)
    // ------------------------------------------------------------------------
    float    m_phase = 0.0f;           // radians
    float    m_burst = 0.0f;           // 0..1 fast impact envelope (novelty-driven)
    uint32_t m_lastHopSeq = 0;

    static constexpr uint8_t CHROMA_HISTORY = 4;
    float   m_chromaEnergyHist[CHROMA_HISTORY] = {0.0f};
    float   m_chromaEnergySum = 0.0f;
    uint8_t m_chromaHistIdx = 0;

    float   m_energyAvg = 0.0f;
    float   m_energyDelta = 0.0f;
    uint8_t m_dominantBin = 0;

    // Smoothed values (stable render controls)
    float   m_energyAvgSmooth = 0.0f;
    float   m_energyDeltaSmooth = 0.0f;
    float   m_dominantBinSmooth = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
