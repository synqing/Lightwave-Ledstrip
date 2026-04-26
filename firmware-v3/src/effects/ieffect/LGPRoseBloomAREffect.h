/**
 * @file LGPRoseBloomAREffect.h
 * @brief Rose Bloom (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Effect ID: 0x1C0B (EID_LGP_ROSE_BLOOM_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRoseBloomAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_ROSE_BLOOM_AR;

    LGPRoseBloomAREffect();
    ~LGPRoseBloomAREffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

    // ------------------------------------------------------------------------
    // Runtime test-mode selector — Captain hardware A/B framework.
    // Baseline preserves shipped behaviour. A-I exercise distinct hypotheses
    // for "carry-through" (per F1 + F1b option matrix, RB-FC fact-checked).
    // Non-destructive: render() reads s_roseBloomMode and applies LOCAL
    // overrides; existing m_* state is never mutated outside its mode.
    // Cycle via SerialCLI 'R' key.
    //
    // FC-guard: ESV11 isOnDownbeat() is a 4-beat bar heuristic, NOT musical
    // phrase detection. Modes A/D/F using "downbeat" inherit this semantic.
    // ------------------------------------------------------------------------
    enum class RoseBloomMode : uint8_t {
        Baseline = 0,   // current shipped behaviour (single-stage EMA + max-follower + impact 180ms)
        A        = 1,   // downbeat-armed multiplicative LIFT (carry-through, lift ceiling-clamped 1.30)
        B        = 2,   // beat-armed multiplicative LIFT (companion to A; downbeat reliability fallback)
        C        = 3,   // tempo-locked geometric LFO (m_t advance from bpm/4 instead of speed)
        D        = 4,   // petal-count latch on downbeat with bar-period release
        E        = 5,   // dual-layer slow EMA on normBass/normMid (70/30 fast/slow blend)
        F        = 6,   // chroma phrase persistence (capture on downbeat, 15% blend)
        G        = 7,   // multi-scale impact decay (fast 180ms + slow 600ms, 65/35 composite)
        H        = 8,   // petal spring inertia (critically-damped, vel-clamped, boundary-zeroed)
        I        = 9,   // adaptive fadeAmt (lengthen trail between beats, floor 7)
    };
    static constexpr uint8_t kRoseBloomModeCount = 10;

    static RoseBloomMode getRoseBloomMode()              { return s_roseBloomMode; }
    static void          setRoseBloomMode(RoseBloomMode m) { s_roseBloomMode = (static_cast<uint8_t>(m) < kRoseBloomModeCount) ? m : RoseBloomMode::Baseline; }
    static const char*   getRoseBloomModeName(RoseBloomMode m);

private:
    static constexpr uint8_t kMaxZones = 4;

    float m_t[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Single-stage smoothed audio
    float m_bass[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_mid[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Asymmetric max followers
    float m_bassMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_midMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};

    // Rose petal count with smoothing
    float m_petalK[kMaxZones] = {5.0f, 5.0f, 5.0f, 5.0f};

    // Impact
    float m_impact[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // ------------------------------------------------------------------------
    // Runtime test-mode state (per-zone scalars, DRAM-resident, ~144 bytes)
    // All decay naturally to neutral when their mode is inactive — no
    // mode-switch reset required EXCEPT m_petalVel (Mode H) which carries
    // momentum (handled in render() top dispatch block).
    // ------------------------------------------------------------------------
    float m_barEnvelope[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};        // Modes A, B
    bool  m_prevBeatFired[kMaxZones] = {false, false, false, false};   // Modes B, I (rising-edge)
    bool  m_prevDownbeatFired[kMaxZones] = {false, false, false, false}; // Modes A, D, F (rising-edge)
    float m_iBeatLift[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};           // Mode I
    float m_petalLatch[kMaxZones] = {3.0f, 3.0f, 3.0f, 3.0f};          // Mode D
    float m_latchReleaseTime[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};    // Mode D
    float m_petalVel[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};            // Mode H (zero on mode-switch)
    float m_normBassSlow[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};        // Mode E
    float m_normMidSlow[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};         // Mode E
    float m_impactSlow[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};          // Mode G
    float m_chromaPhrase[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};        // Mode F

    static RoseBloomMode s_roseBloomMode;
    static RoseBloomMode s_lastRoseBloomMode;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
