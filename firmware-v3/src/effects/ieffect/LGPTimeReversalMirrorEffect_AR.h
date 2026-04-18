/**
 * @file LGPTimeReversalMirrorEffect_AR.h
 * @brief LGP Time-Reversal Mirror (AR) - Audio-reactive variant with EDM beat locking
 *
 * Effect ID: 0x1B05 (EID_LGP_TIME_REVERSAL_MIRROR_AR)
 * Family: SHOWPIECE_PACK3
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * Architecture: Faithful to the original LGPTimeReversalMirrorEffect's visual
 * pipeline (palette colours, dynamic min/max normalisation, linear brightness,
 * phase flip during reverse, Strip B at fi+10) with layered AR enhancements:
 *   - isOnBeat() triggers extra centre impulse during forward phase
 *   - isSnareHit() can trigger early reverse transition (rate-limited)
 *   - Kick envelope modulates impulse strength
 *   - Speed controls phase durations and impulse cadence
 *   - Mood controls smoothing time constants and wave damping
 *
 * Timed sequence (~6.5 s base loop, modulated by Speed):
 *   Forward phase: Wave simulation + history recording + periodic impulses
 *   Reverse phase: History playback with phase flip (1.0 - snap[i])
 *
 * PSRAM: ~45.8 kB allocated via heap_caps_malloc(MALLOC_CAP_SPIRAM).
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

class LGPTimeReversalMirrorEffect_AR : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_TIME_REVERSAL_MIRROR_AR;

    LGPTimeReversalMirrorEffect_AR();
    ~LGPTimeReversalMirrorEffect_AR() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint8_t kMaxZones = 4;

    // -------------------------------------------------------------------
    // Domain constants (match original for visual fidelity)
    // -------------------------------------------------------------------
    static constexpr uint16_t kFieldSize     = 80;    // Half-strip (distance 0..79 from centre)
    static constexpr uint16_t kHistoryDepth  = 140;   // Snapshots in ring buffer
    static constexpr float    kCsq           = 0.15f; // Propagation strength (c squared) -- CFL stable
    static constexpr float    kBaseDamping   = 0.04f; // Base damping coefficient
    static constexpr uint16_t kBaseImpulseEvery = 90; // Base frames between impulses
    static constexpr float    kForwardSec    = 4.0f;  // Base forward phase duration
    static constexpr float    kReverseSec    = 2.5f;  // Base reverse phase duration

    float m_csq = kCsq;
    float m_baseDamping = kBaseDamping;
    uint16_t m_baseImpulseEvery = kBaseImpulseEvery;
    float m_forwardSec = kForwardSec;
    float m_reverseSec = kReverseSec;

    // AR-specific constants
    static constexpr float    kMinReverseCooldownSec = 0.9f;  // Min gap between snare-triggered reverses

    // -------------------------------------------------------------------
    // PSRAM-allocated data (~45,760 bytes -- MUST NOT live in DRAM)
    // -------------------------------------------------------------------
    struct PsramData {
        float u_prev[kMaxZones][kFieldSize];
        float u_curr[kMaxZones][kFieldSize];
        float u_next[kMaxZones][kFieldSize];
        float history[kMaxZones][kHistoryDepth][kFieldSize];
    };

    PsramData* m_ps = nullptr;

    // -------------------------------------------------------------------
    // Per-zone temporal state. ZoneComposer reuses one effect instance across
    // multiple zones, so timers, cursors, and envelopes must be indexed by
    // zone to avoid accelerated playback and cross-zone contamination.
    // -------------------------------------------------------------------
    float    m_phaseTimer[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    bool     m_isReverse[kMaxZones] = {false, false, false, false};
    uint16_t m_frameInPhase[kMaxZones] = {0, 0, 0, 0};

    // History cursors
    uint16_t m_historyWrite[kMaxZones] = {0, 0, 0, 0};
    uint16_t m_historyCount[kMaxZones] = {0, 0, 0, 0};
    int16_t  m_historyRead[kMaxZones] = {0, 0, 0, 0};

    // Impulse timing
    uint16_t m_frameSinceImpulse[kMaxZones] = {0, 0, 0, 0};

    // Fallback time-based animation (when no audio)
    float m_fallbackPhase[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // -------------------------------------------------------------------
    // AR additions: kick/snare envelopes
    // -------------------------------------------------------------------
    float    m_kickEnv[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float    m_snareEnv[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t m_lastReverseMs[kMaxZones] = {0, 0, 0, 0};

    // -------------------------------------------------------------------
    // Audio smoothing (only used when FEATURE_AUDIO_SYNC)
    // -------------------------------------------------------------------
#if FEATURE_AUDIO_SYNC
    // Chromagram smoothing for circular hue
    float m_chromaSmoothed[kMaxZones][12] = {};
    float m_chromaTargets[kMaxZones][12] = {};
    enhancement::AsymmetricFollower m_chromaFollowers[kMaxZones][12];
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Energy followers
    enhancement::AsymmetricFollower m_rmsFollower[kMaxZones] = {
        {0.0f, 0.08f, 0.25f},
        {0.0f, 0.08f, 0.25f},
        {0.0f, 0.08f, 0.25f},
        {0.0f, 0.08f, 0.25f}
    };
    float m_targetRms[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t m_lastHopSeq[kMaxZones] = {0, 0, 0, 0};
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
