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
        float u_prev[kFieldSize];                      //    320 B
        float u_curr[kFieldSize];                      //    320 B
        float u_next[kFieldSize];                      //    320 B
        float history[kHistoryDepth][kFieldSize];      // 44,800 B
    };  // Total: ~45,760 B in SPIRAM

    PsramData* m_ps = nullptr;

    // -------------------------------------------------------------------
    // Phase tracking (timer-based, matching original architecture)
    // -------------------------------------------------------------------
    float    m_phaseTimer       = 0.0f;   // Seconds elapsed in current phase
    bool     m_isReverse        = false;  // true = reverse playback phase
    uint16_t m_frameInPhase     = 0;      // Frame counter within current phase

    // History cursors
    uint16_t m_historyWrite     = 0;      // Next slot to write in ring buffer
    uint16_t m_historyCount     = 0;      // Snapshots stored (up to kHistoryDepth)
    int16_t  m_historyRead      = 0;      // Current read position (counts down)

    // Impulse timing
    uint16_t m_frameSinceImpulse = 0;

    // Fallback time-based animation (when no audio)
    float m_fallbackPhase       = 0.0f;

    // -------------------------------------------------------------------
    // AR additions: kick/snare envelopes
    // -------------------------------------------------------------------
    float    m_kickEnv          = 0.0f;   // Kick envelope (0-1), fast attack / exp release
    float    m_snareEnv         = 0.0f;   // Snare envelope (0-1)
    uint32_t m_lastReverseMs    = 0;      // Timestamp of last reverse trigger (cooldown)

    // -------------------------------------------------------------------
    // Audio smoothing (only used when FEATURE_AUDIO_SYNC)
    // -------------------------------------------------------------------
#if FEATURE_AUDIO_SYNC
    // Chromagram smoothing for circular hue
    float m_chromaSmoothed[12]  = {};
    float m_chromaTargets[12]   = {};
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaAngle         = 0.0f;

    // Energy followers
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.08f, 0.25f};
    float m_targetRms           = 0.0f;
    uint32_t m_lastHopSeq       = 0;
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
