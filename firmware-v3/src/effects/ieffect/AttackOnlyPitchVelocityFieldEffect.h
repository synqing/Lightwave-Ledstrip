/**
 * @file AttackOnlyPitchVelocityFieldEffect.h
 * @brief LIN-08 — Attack-Only Pitch-Class Velocity Field
 *
 * Effect ID: 0x2101 (FAMILY_K1_NATIVE)
 * Family: K1-native — phase 5, Move 5.6 (major rewrite 2026-04-27).
 * Tags: CENTER_ORIGIN, AUDIO_REACTIVE, PITCH_LOCKED_PALETTE, SUBPIXEL
 *
 * Topology Reconciliation §5 Phase 5 Move 5.6 (LIN-08): a velocity-field
 * pitch visualiser. The unique signature kept from the first iteration is
 * the top-3 spatial-frequency selection: only the three strongest chroma
 * classes contribute to the radial sin sum, so dense chord clusters fold
 * into clean interference patterns rather than smearing into noise.
 *
 * Major rewrite (Move 5.6, 2026-04-27) replaced three architectural
 * violations diagnosed on hardware as "twitching at 3 FPS like a dog":
 *   1. Missing fadeToBlackBy + overwrite-instead-of-compose: render() now
 *      fades-to-black on entry and composes with `+=` (qadd8 saturating)
 *      so trails persist and bright peaks add cleanly.
 *   2. 12 fixed CHSV anchors via chromaToAnchorHue(): replaced with
 *      circularChromaHueSmoothed() — the canonical pattern installed on
 *      14+ effects in the Feb 2026 migration.
 *   3. Hand-rolled per-class EMA: replaced with enhancement::AsymmetricFollower
 *      using the canonical {0.0f, 0.06f, 0.25f} construction shared with
 *      BreathingEnhancedEffect / RippleEnhancedEffect.
 * Integer field sampling at centerPairDistance(i) was also replaced with
 * SubpixelRenderer-style integer-position writes (one composed write per
 * radial bin per arm) which eliminates the wagon-wheel aliasing the integer
 * sampler showed when the dominant frequency aligned with the radial step.
 *
 * Visual signature:
 *   • Silence — strip is dark (fast_rms = 0 collapses bed; followers stay 0).
 *   • Hold a pitch — radial standing wave forms with frequency proportional
 *     to (1.0 + c/12)·base; brightness rises with the AsymmetricFollower
 *     attack tau (60 ms), settles, decays on release tau (250 ms).
 *   • Chord — top-3 strongest classes interfere; circular-mean chroma
 *     selects the palette index; bounded ±32 from the dominant pole means
 *     no full hue-wheel sweep.
 *   • Quiet passages — a small rms-driven bed (≤16 % of full strip
 *     brightness) keeps the strip alive without drowning the radial peaks.
 *
 * Constraints honoured:
 *   • HW-03 centre origin: every write radiates from LEDs 79/80; both
 *     strips mirrored. Strip 1 left arm at LED 79-r, right arm at 80+r.
 *   • No heap allocation in render(): velocity field is pre-allocated in
 *     init() (PSRAM if available — see PSRAM policy below).
 *   • dt-correct: AsymmetricFollower / circularChromaHueSmoothed both use
 *     `alpha = 1 - exp(-dt/tau)` — identical at 60 Hz, 120 Hz, or variable.
 *   • 2.0 ms render ceiling: 12 follower updates + top-3 partial select +
 *     80 sin lookups + 320 LED writes ≈ ~50 µs typical.
 *   • No rainbows: hue is the circular-mean of the 12-bin chroma vector;
 *     a single dominant class selects exactly one of 12 angles, so the
 *     palette index is bounded.
 *   • British English in comments and identifiers (centre, colour, behaviour).
 *
 * PSRAM policy (per IEffect.h header policy):
 *   The velocity-field buffer (320 B) is above the 64 B DRAM threshold so
 *   it lives in a PsramData struct allocated from PSRAM in init() and
 *   freed in cleanup(). On native unit-test builds (NATIVE_BUILD) the
 *   same struct is allocated from the host malloc heap.
 *
 * Public surface:
 *   - Standard IEffect lifecycle (init / render / cleanup / getMetadata).
 *   - buildVelocityFieldFromTopK() — pure helper exposed for unit tests.
 *   - debugTickFollowers / debugFollowers / debugSetFollower — test seams
 *     that operate on a local raw-float follower array (not the production
 *     AsymmetricFollower path) so unit tests can drive the original spec's
 *     attack-only / decay-only contract without the production EMA shape.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include "../../config/effect_ids.h"
#include <esp_heap_caps.h>
#endif

#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class AttackOnlyPitchVelocityFieldEffect : public plugins::IEffect {
public:
#ifndef NATIVE_BUILD
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_ATTACK_ONLY_PITCH_VELOCITY;
#endif

    // -----------------------------------------------------------------------
    // Tunables (exposed so unit tests can reference the same constants).
    // -----------------------------------------------------------------------

    static constexpr uint8_t kChromaBins   = 12;     // 12 pitch classes (C..B)
    static constexpr uint16_t kHalfLength  = 80;     // LEDs per half-strip
    static constexpr uint8_t kRenderTopK   = 3;      // sum the 3 strongest classes

    // Test-seam time constants — used ONLY by debugTickFollowers() to
    // preserve the original Move 5.6 contract that the unit suite checks
    // (attack-on-onset only, release decays toward zero with 1.5 s tau).
    // The production render path uses enhancement::AsymmetricFollower with
    // its own canonical {0.06 s rise, 0.25 s fall} pair (see .cpp).
    static constexpr float kAttackTau      = 0.05f;  // 50 ms — fast onset rise
    static constexpr float kReleaseTau     = 1.5f;   // 1.5 s — slow decay between onsets

    static constexpr float kDriftRateRadPerSec = 0.2f;  // gentle phase drift so field is not static

    AttackOnlyPitchVelocityFieldEffect() = default;
    ~AttackOnlyPitchVelocityFieldEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    /**
     * @brief Build the radial velocity field from the current 12-channel
     *        followers, using only the top-K strongest classes.
     *
     * Pure helper — no class state, no allocation, no I/O. Decoupled from
     * hue selection (Move 5.6 spec S5): the top-K spatial-frequency
     * selection is the effect's unique signature; hue is sourced separately
     * via circularChromaHueSmoothed in render().
     *
     * For each radial bin r ∈ [0, kHalfLength):
     *     phase = r / kHalfLength                               // ∈ [0, 1)
     *     v     = Σ_{c ∈ topK} follower[c] * sin(2π·freq[c]·phase + driftPhase)
     *
     * with freq[c] = 1.0 + c/12 (linear spacing — the linear baseline
     * specified in Move 5.6; powf(2, c/12) is reserved for a Captain A/B).
     *
     * @param chromaFollowers Source 12-element follower array (input).
     * @param topK            How many classes to sum (0..kChromaBins).
     * @param driftPhase      Global phase offset in radians (caller-managed).
     * @param field           Destination radial buffer (size kHalfLength).
     */
    static void buildVelocityFieldFromTopK(const float chromaFollowers[kChromaBins],
                                           uint8_t topK,
                                           float driftPhase,
                                           float field[kHalfLength]);

    /**
     * @brief Test seam: pulse one frame into the test-only follower array
     *        so unit tests can drive the original onset-gated contract
     *        without spinning up an EffectContext.
     *
     * Implements the original spec semantics that the unit suite checks:
     *   - When `onsetGate` is true and target > follower[c], rises with
     *     attack tau (50 ms).
     *   - Otherwise decays toward zero with release tau (1.5 s) —
     *     `follower *= (1 - alphaFall)`.
     *
     * NOTE: this is INDEPENDENT of the production AsymmetricFollower path.
     * The production render() uses {0.06, 0.25} continuous-EMA semantics.
     */
    void debugTickFollowers(const float chroma[kChromaBins],
                            bool onsetGate,
                            float dt);

    /// Read-only accessor for tests to inspect the test-only follower array.
    const float* debugFollowers() const { return m_chromaFollower; }

    /// Test seam: directly seed the test-only follower for "follower=1.0;
    /// decay 1.5 s" style scenarios. Production code never calls this.
    void debugSetFollower(uint8_t c, float v) {
        if (c < kChromaBins) m_chromaFollower[c] = v;
    }

private:
    // PSRAM-allocated scratch — total 320 B is above the 64 B DRAM
    // threshold so cannot live in DRAM per IEffect.h policy. Allocated
    // in init(), freed in cleanup(); render() never touches the heap.
    struct PsramData {
        float velocityField[kHalfLength];  // 320 B — radial brightness scratch
    };
    PsramData* m_ps = nullptr;

    // Production hot-path state. AsymmetricFollower replaces the previous
    // hand-rolled per-class EMA (Move 5.6 spec S3). Canonical {rise=0.06 s,
    // fall=0.25 s} pair shared with BreathingEnhancedEffect (line 60) and
    // RippleEnhancedEffect.
    enhancement::AsymmetricFollower m_chromaFollowers[kChromaBins];

    // Smoothed circular-mean chroma angle (radians, 0..2π). Persists across
    // frames so circularChromaHueSmoothed() can EMA the hue continuously.
    // (Move 5.6 spec S4.)
    float m_chromaAngle = 0.0f;

    // Hop-sequence gate (Move 5.6 spec S9). UINT32_MAX sentinel so the
    // first call always captures targets even when bus.hop_seq is still 0.
    uint32_t m_lastHopSeq = 0xFFFFFFFFu;

    // Last captured chroma targets — refreshed only on fresh hops, but the
    // production followers smooth toward them every frame so the visual
    // stays buttery at 120 FPS even though the audio bus only advances at
    // ~125 Hz hop cadence. (Canonical pattern from RippleEnhancedEffect.)
    float m_chromaTargets[kChromaBins] = {0};

    // Cumulative drift phase for the radial sin sum (radians, kept in
    // [0, 2π) by render()).
    float m_phaseAccum = 0.0f;

    // Test-only per-class follower array used by debugTickFollowers /
    // debugFollowers / debugSetFollower. Production code does NOT touch
    // these; render() drives m_chromaFollowers (the AsymmetricFollower
    // array) instead.
    float m_chromaFollower[kChromaBins] = {0};
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
