/**
 * @file LGPStarBurstNarrativeEffect.h
 * @brief LGP Star Burst (Narrative) - Story conductor + chord-based coloring
 *
 * Effect ID: 74
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 *
 * ## Layer Audit (2025-12-28)
 *
 * ### Layers
 * 1. Story Conductor: REST/BUILD/HOLD/RELEASE state machine controlling envelope
 * 2. Chord/Key Tracking: Phrase-gated root note and major/minor detection
 * 3. Motion Layer: Center-origin starburst rays driven by phase accumulator
 * 4. Color Layer: Triad-based coloring (root/third/fifth) from detected chords
 * 5. Impact Layer: Burst/pulse triggers on novelty spikes
 *
 * ### Musical Fit
 * Best for: HARMONIC_DRIVEN music (jazz, classical, chord progressions)
 * Secondary: RHYTHMIC_DRIVEN music benefits from pulse layer
 *
 * ### Adaptive Response (MIS Phase 2)
 * - State machine timing adapts to detected music style
 * - Behavior selection changes rendering strategy per style
 * - Palette strategy varies with music type
 * - Saliency emphasis weights visual dimensions
 *
 * ### Temporal Awareness
 * - 4-hop chroma history for novelty detection
 * - Phrase gating (1.5-6s) for palette commits based on style
 * - Asymmetric smoothing (fast rise, slow fall)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../plugins/api/BehaviorSelection.h"
#include "../enhancement/SmoothingEngine.h"

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

    // -----------------------------------------
    // MIS Phase 2: Behavior Selection State
    // -----------------------------------------
    plugins::VisualBehavior m_currentBehavior = plugins::VisualBehavior::DRIFT_WITH_HARMONY;
    plugins::PaletteStrategy m_paletteStrategy = plugins::PaletteStrategy::HARMONIC_COMMIT;
    plugins::StyleTiming m_styleTiming{};
    plugins::SaliencyEmphasis m_saliencyEmphasis{0.25f, 0.25f, 0.25f, 0.25f};

    // Shimmer layer state (for SHIMMER_WITH_MELODY)
    float m_shimmerPhase = 0.0f;

    // Slew-limited speed (prevents jog-dial jitter - copied from ChevronWaves golden pattern)
    float m_speedSmooth = 1.0f;

    // Smooth style transition (prevent abrupt style switching)
    float m_styleBlend = 0.0f;  // 0=old style, 1=new style
    audio::MusicStyle m_prevStyle = audio::MusicStyle::UNKNOWN;

    // -----------------------------------------
    // Enhancement 1: Dynamic Color Warmth
    // -----------------------------------------
    float m_warmthOffset = 0.0f;        // Hue offset (-30 to +30)

    // -----------------------------------------
    // Enhancement 2: Behavior Transition Blending
    // -----------------------------------------
    plugins::VisualBehavior m_prevBehavior = plugins::VisualBehavior::DRIFT_WITH_HARMONY;
    float m_behaviorBlend = 1.0f;       // 0=old behavior, 1=new behavior

    // -----------------------------------------
    // Enhancement 3: Texture Layer State
    // -----------------------------------------
    float m_texturePhase = 0.0f;        // Wave phase accumulator
    float m_textureIntensity = 0.0f;    // Layer intensity (0-1)
    float m_fluxSmooth = 0.0f;          // Smoothed spectral flux

    // -----------------------------------------
    // 64-bin Spectrum Enhancement
    // BYPASSES story conductor for immediate kick/treble response
    // -----------------------------------------
    float m_kickBurst = 0.0f;           ///< Sub-bass energy (bins 0-5) for instant burst
    float m_trebleShimmerIntensity = 0.0f;  ///< Treble energy (bins 48-63) for shimmer boost
    
    // Audio smoothing (AsymmetricFollower for mood-adjusted smoothing)
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.05f, 0.30f};
    float m_targetRms = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
