/**
 * @file LGPTimeReversalMirrorEffect.h
 * @brief LGP Time-Reversal Mirror - 1D damped wave recorded then replayed backwards
 *
 * Effect ID: 0x1B00 (EID_LGP_TIME_REVERSAL_MIRROR)
 * Family: SHOWPIECE_PACK3
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * Physics: Records the field evolution of a centre-originating damped wave
 * into a history ring buffer, then plays the snapshots in reverse with a
 * phase flip (inversion around 0.5). On-camera the effect looks like
 * "physics rewinding inside glass."
 *
 * Timed sequence (~6.5 s loop):
 *   Forward phase (4 s / ~480 frames): Wave simulation runs, snapshots
 *       are recorded into the ring buffer. Centre impulse injected every
 *       ~90 frames.
 *   Reverse phase (2.5 s / ~300 frames): History played backwards with
 *       field values inverted around 0.5, giving a phase-flipped rewind.
 *
 * Audio reactivity (FEATURE_AUDIO_SYNC):
 *   - circularChromaHueSmoothed drives base hue
 *   - RMS modulates impulse strength during forward phase
 *   - Beat triggers an extra impulse during forward phase
 *
 * PSRAM: ~45.6 kB allocated via heap_caps_malloc(MALLOC_CAP_SPIRAM).
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

class LGPTimeReversalMirrorEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_TIME_REVERSAL_MIRROR;

    LGPTimeReversalMirrorEffect();
    ~LGPTimeReversalMirrorEffect() override = default;

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
    // Domain constants
    // -------------------------------------------------------------------
    static constexpr uint16_t kFieldSize     = 80;   // Half-strip (distance 0..79 from centre)
    static constexpr uint16_t kHistoryDepth  = 140;   // Snapshots in ring buffer
    static constexpr float    kCsq           = 0.15f; // Propagation strength (c squared)
    static constexpr float    kDamping       = 0.04f; // Damping coefficient
    static constexpr uint16_t kImpulseEvery  = 90;    // Frames between impulses (forward)
    static constexpr float    kForwardSec    = 4.0f;  // Forward phase duration (seconds)
    static constexpr float    kReverseSec    = 2.5f;  // Reverse phase duration (seconds)

    float m_csq = kCsq;
    float m_damping = kDamping;
    uint16_t m_impulseEvery = kImpulseEvery;
    float m_forwardSec = kForwardSec;
    float m_reverseSec = kReverseSec;

    // -------------------------------------------------------------------
    // PSRAM-allocated data (~45,600 bytes -- MUST NOT live in DRAM)
    // -------------------------------------------------------------------
    struct PsramData {
        float u_prev[kFieldSize];              //  320 B
        float u_curr[kFieldSize];              //  320 B
        float u_next[kFieldSize];              //  320 B
        float history[kHistoryDepth][kFieldSize]; // 44,800 B
    };  // Total: ~45,760 B in SPIRAM

    PsramData* m_ps = nullptr;

    // -------------------------------------------------------------------
    // Instance state (small -- safe in DRAM)
    // -------------------------------------------------------------------

    // Phase tracking
    float    m_phaseTimer   = 0.0f;  // Seconds elapsed in current phase
    bool     m_isReverse    = false; // true = reverse playback phase
    uint16_t m_frameInPhase = 0;     // Frame counter within current phase

    // History write cursor (forward phase)
    uint16_t m_historyWrite = 0;     // Next slot to write in ring buffer
    uint16_t m_historyCount = 0;     // Snapshots stored (up to kHistoryDepth)

    // History read cursor (reverse phase)
    int16_t  m_historyRead  = 0;     // Current read position (counts down)

    // Impulse timing
    uint16_t m_frameSinceImpulse = 0;

    // Fallback time-based animation (when no audio)
    float m_fallbackPhase = 0.0f;

    // -------------------------------------------------------------------
    // Audio smoothing (only used when FEATURE_AUDIO_SYNC)
    // -------------------------------------------------------------------
#if FEATURE_AUDIO_SYNC
    // Chromagram smoothing for circular hue
    float m_chromaSmoothed[12] = {};
    float m_chromaTargets[12]  = {};
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaAngle        = 0.0f;

    // Energy followers
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.08f, 0.25f};
    float m_targetRms          = 0.0f;
    uint32_t m_lastHopSeq      = 0;
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
