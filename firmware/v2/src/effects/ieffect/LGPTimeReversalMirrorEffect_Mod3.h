/**
 * @file LGPTimeReversalMirrorEffect_Mod3.h
 * @brief LGP Time-Reversal Mirror Mod3 - organic layered flow variant
 *
 * Effect ID: 0x1B08 (EID_LGP_TIME_REVERSAL_MIRROR_MOD3)
 * Family: SHOWPIECE_PACK3
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * Design goals vs base effect:
 *   - Keep reverse continuity fixes from Mod1/Mod2
 *   - Maintain continuous layered modulation (not discrete staccato impulses)
 *   - Add a dedicated ridge envelope for stable fang edge definition
 *   - Preserve sharp node peaks while restoring organic flow
 *
 * PSRAM: ~321.0 kB allocated via heap_caps_malloc(MALLOC_CAP_SPIRAM).
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../effects/enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPTimeReversalMirrorEffect_Mod3 : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_TIME_REVERSAL_MIRROR_MOD3;

    LGPTimeReversalMirrorEffect_Mod3();
    ~LGPTimeReversalMirrorEffect_Mod3() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // -------------------------------------------------------------------
    // Domain constants
    // -------------------------------------------------------------------
    static constexpr uint16_t kFieldSize    = 80;     // Half-strip distance domain (0..79 from centre)
    static constexpr uint16_t kHistoryDepth = 1024;   // Deep history for long-form narrative
    static constexpr float    kCsq          = 0.14f; // Propagation strength (c squared)
    static constexpr float    kDamping      = 0.035f;
    static constexpr float    kEdgeAbsorb   = 0.09f; // Extra damping near outer edge
    static constexpr uint16_t kImpulseEvery = 96;    // Base frames between impulses
    static constexpr float    kForwardSec   = 6.0f;  // +50% vs original 4.0 s
    static constexpr float    kReverseSec   = 3.75f; // +50% vs original 2.5 s

    static constexpr float    kIntroSec                  = 1.6f; // Fluid, non-staccato opening
    static constexpr float    kIntroDrive                = 0.07f;
    static constexpr uint16_t kBeatImpulseCooldownFrames = 16;
    static constexpr float    kBeatReleaseSec            = 0.42f;
    static constexpr float    kNormaliseFollowHz         = 6.0f;
    static constexpr float    kPeakGamma                 = 1.35f;
    static constexpr float    kRidgeAttackHz             = 18.0f;
    static constexpr float    kRidgeReleaseHz            = 4.5f;
    static constexpr float    kRidgeSensitivity          = 3.6f;
    static constexpr float    kRidgeBlend                = 0.56f;
    static constexpr float    kRidgeFloor                = 0.05f;

    float m_csq = kCsq;
    float m_damping = kDamping;
    float m_edgeAbsorb = kEdgeAbsorb;
    uint16_t m_impulseEvery = kImpulseEvery;
    float m_forwardSec = kForwardSec;
    float m_reverseSec = kReverseSec;
    float m_introSec = kIntroSec;
    float m_introDrive = kIntroDrive;
    float m_beatReleaseSec = kBeatReleaseSec;
    float m_normaliseFollowHz = kNormaliseFollowHz;
    float m_peakGamma = kPeakGamma;
    float m_ridgeAttackHz = kRidgeAttackHz;
    float m_ridgeReleaseHz = kRidgeReleaseHz;
    float m_ridgeSensitivity = kRidgeSensitivity;
    float m_ridgeBlend = kRidgeBlend;
    float m_ridgeFloor = kRidgeFloor;

    // -------------------------------------------------------------------
    // PSRAM storage (must not live in internal DRAM)
    // -------------------------------------------------------------------
    struct PsramData {
        float u_prev[kFieldSize];
        float u_curr[kFieldSize];
        float u_next[kFieldSize];
        float history[kHistoryDepth][kFieldSize];
    };

    PsramData* m_ps = nullptr;

    // Phase and history state
    float    m_phaseTimer       = 0.0f;
    bool     m_isReverse        = false;
    uint16_t m_frameInPhase     = 0;
    uint16_t m_historyWrite     = 0;    // Next slot to write in ring
    uint16_t m_historyCount     = 0;    // Stored frames (<= kHistoryDepth)
    float    m_reverseCursor    = 0.0f; // Chronological index for reverse playback
    uint16_t m_frameSinceImpulse = 0;
    uint16_t m_framesSinceBeatImpulse = 0;
    float    m_storyTime        = 0.0f;
    float    m_introPhase       = 0.0f;
    float    m_beatEnv          = 0.0f;
    float    m_ridgeEnv[kFieldSize] = {};

    // Visual normalisation smoothing
    float m_normMin = 0.45f;
    float m_normMax = 0.55f;

    // Fallback time-based animation
    float m_fallbackPhase = 0.0f;

    // Helpers
    void seedField();
    void beginForwardPhase(bool reseedField);
    void beginReversePhase();
    uint16_t historySlotFromChrono(uint16_t chronoIndex) const;

    // Audio smoothing (when FEATURE_AUDIO_SYNC)
#if FEATURE_AUDIO_SYNC
    float m_chromaSmoothed[12] = {};
    float m_chromaTargets[12]  = {};
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaAngle        = 0.0f;

    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.08f, 0.25f};
    float m_targetRms          = 0.0f;
    uint32_t m_lastHopSeq      = 0;
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
