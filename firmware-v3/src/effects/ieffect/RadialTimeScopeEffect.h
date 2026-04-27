/**
 * @file RadialTimeScopeEffect.h
 * @brief Radial Time-Scope — continuous activity scalar written into a
 *        centre-anchored history ring so distance from LED 79/80 reads as
 *        "seconds ago".
 *
 * Effect ID: 0x2100 (FAMILY_K1_NATIVE)
 * Family: K1-Native exemplars
 * Tags: CENTER_ORIGIN, SELF_TRAILING, AUDIO_REACTIVE, K1-NATIVE
 *
 * Topology Reconciliation Phase 5 Move 5.4 (LIN-06). One of the K1-native
 * exemplars promised by Pass 2 §3 Pillar D — geometry-from-time effects that
 * exploit the strict centre-pair topology rather than treating it as an
 * obstacle. The dual-strip light guide plate is uniquely suited to a
 * scope-shaped reading of recent history because the centre seam *is* "now"
 * and every step outward maps to a specific moment in the past.
 *
 * ── Visual signature ─────────────────────────────────────────────────────
 *
 *   - At every frame the freshest activity sample is the brightest point at
 *     LEDs 79/80 (centre pair).
 *   - Each LED outward in either direction shows the activity envelope from
 *     N/60 seconds ago, where N is centerPairDistance(i). LED 0 / LED 159
 *     therefore display the envelope from ~1.33 s ago.
 *   - Strip 2 mirrors strip 1 (PS-05 reflective twin contract). The whole
 *     pattern is symmetric across the centre seam.
 *   - During silence the scope writes near-black at the scope layer but a
 *     dim bed-layer floor remains so the strip never fully dies (F3 Liquid
 *     Stillness contract — silence dissolves but does not extinguish).
 *   - Hue is bounded within ±32 (~±45°) of an anchor; never sweeps the
 *     full hue wheel. Drift is gated by audioConfidence so silence freezes
 *     the colour rather than sliding it.
 *
 * ── Topology compliance ──────────────────────────────────────────────────
 *
 *   - Centre origin: LEDs 79/80 receive atOffset(0) — the freshest sample.
 *     There is no linear sweep from edge to edge; energy emanates from the
 *     centre by construction.
 *   - Strict mirror symmetry: leds[79-k] and leds[80+k] are written from
 *     the same history value (centerPairDistance is the index).
 *   - Reflective twin: strip 2 (160..319) receives the same writes as
 *     strip 1 (0..159).
 *
 * ── Algorithm ────────────────────────────────────────────────────────────
 *
 *   - Substrate: persistence::ScalarRing<float, 80>, 320 bytes. Per IEffect
 *     PSRAM policy (header threshold = 64 B), the ring is allocated from
 *     PSRAM in init() via heap_caps_malloc(MALLOC_CAP_SPIRAM) and freed in
 *     cleanup(). The class member is a pointer, not the ring by value.
 *   - Source field: continuous activity scalar built from
 *     `controlBus.fast_rms` (~100 ms reactive, asymmetric pre-smoothed in
 *     ControlBus α_fast=0.35), with onset-flux spikes superimposed via
 *     max(fast_rms, onsetFlux*4). Wrapped in an `AsymmetricFollower`
 *     (rise=50 ms, fall=300 ms, Effect Standard §3.2 canonical) so the
 *     scope timeline reads punchy on attacks and decays gracefully.
 *     `onsetEnv` is NOT used as a continuous source (ADR 2026-03-25:
 *     advanced/debug-oriented, 0 most frames).
 *   - Hop gating: the follower's *target* is only updated when
 *     `controlBus.hop_seq` advances (audio cadence ~125 Hz). The follower
 *     itself updates every render frame (~120 Hz) so smoothing stays
 *     dt-correct under variable render FPS.
 *   - Push cadence: a fixed 60 Hz sample rate decoupled from render FPS.
 *     getSafeRawDeltaSeconds() is accumulated; while accumulator >= 1/60 s
 *     a push is performed and the accumulator is decremented. This
 *     guarantees consistent "seconds-ago" semantics regardless of whether
 *     the renderer runs at 60, 100, or 120 FPS.
 *   - Render: one pass over [0, STRIP_LENGTH) computing
 *       dist     = centerPairDistance(i)
 *       value    = ring.atOffset(dist)
 *       gated    = value * audioConfidence * silentScale
 *       brightness = perceptualSquare(gated) * 255
 *       col      = ctx.palette.getColor(paletteIdx, brightness)
 *     and mirroring onto strip 2. A bed layer at 10-20% floor is written
 *     first so silence dissolves without killing the strip.
 *
 * ── Constraints honoured ────────────────────────────────────────────────
 *
 *   - HW-03 centre origin (writes emanate from LEDs 79/80 outward).
 *   - No heap allocation in render(); PSRAM allocation only in init().
 *   - dt-correct: 60 Hz push cadence is invariant under render FPS; the
 *     AsymmetricFollower uses true exponential decay (1 - exp(-dt/tau)).
 *   - 2.0 ms render ceiling: 320 LED writes + 1 ring read each + 1
 *     palette lookup + 1 follower update + 1 bed-layer pre-pass.
 *     Comfortably under budget.
 *   - No rainbows / no full hue-wheel sweep. Palette index stays within
 *     ±32 of an anchor; drift is silence-gated.
 *   - Multiplicative silence gate (no early return): the render path
 *     always runs so the bed layer keeps silence visible-but-dim.
 *   - British English in comments, identifiers, and metadata strings.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include "../persistence/PSRAMScalarRing.h"

#ifndef NATIVE_BUILD
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class RadialTimeScopeEffect : public plugins::IEffect {
public:
#ifndef NATIVE_BUILD
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_RADIAL_TIME_SCOPE;
#endif

    // Fixed 60 Hz history sample rate. Decouples the scope timeline from the
    // renderer FPS so a 120 FPS frame and a 60 FPS frame both tick the ring
    // at the same rate. 60 Hz × 80-slot ring = ~1.333 s of recent history,
    // which pleasantly matches the perceptual "recent past" window for
    // music-reactive feedback.
    static constexpr float kPushIntervalSec = 1.0f / 60.0f;

    // Ring depth (must equal HALF_LENGTH; one sample per centre-pair distance
    // step, so the edge LEDs (distance 79) display the oldest sample).
    static constexpr size_t kRingLength = 80;

    // Palette-index anchor and bound. The drift is gated by audioConfidence
    // so silence freezes the colour rather than sliding it; the bound
    // enforces the "no rainbows" hard rule even if drift were to run
    // unbounded. The byte is interpreted as a palette index (0-255), not a
    // hue; the actual colour is whatever the current K1 palette has at that
    // entry. A±32 swing therefore explores a controlled neighbourhood of
    // the palette rather than sweeping it end-to-end.
    static constexpr uint8_t kHueAnchor = 160;
    static constexpr uint8_t kHueBound  = 32;   // ±32 of anchor (~±45°)

    // Hue drift speed in palette-index units/second, scaled by audioConfidence.
    // At confidence 1.0 the index wanders the bound in ~64 s — slow enough
    // that the eye reads "the colour is breathing" rather than "the colour
    // is changing".
    static constexpr float kHueDriftPerSec = 0.5f;

    // Saturation kept for the native-test fallback CHSV path only. The
    // production code path uses the palette and ignores saturation here.
    static constexpr uint8_t kSaturation = 220;

    // Asymmetric follower time constants (Effect Standard §3.2 canonical).
    // 50 ms attack reads as "punchy on transients"; 300 ms release reads
    // as "graceful decay between hits". Tuned against BeatPulseBloomEffect
    // and RippleEnhancedEffect for cross-effect family coherence.
    static constexpr float kEnvRiseTauSec = 0.05f;
    static constexpr float kEnvFallTauSec = 0.30f;

    // Bed-layer floor. Even at total silence the strip keeps a faint glow
    // proportional to fast_rms with a constant floor — F3 Liquid Stillness
    // contract: dissolve, do not extinguish.
    static constexpr uint8_t kBedFloor = 16;          // Brightness at silence
    static constexpr uint8_t kBedRmsGain = 35;        // Adds up to ~14% on top of floor

    RadialTimeScopeEffect() = default;
    ~RadialTimeScopeEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    /**
     * @brief Convert a single ring sample into a CRGB pixel (native-test
     *        helper).
     *
     * Pure function — exposed for unit tests so the brightness curve and
     * silence-gate can be verified without setting up an EffectContext or
     * the PSRAM ring. Returns black (0,0,0) when audioConfidence is zero
     * regardless of the history value, AND when the computed brightness
     * byte is zero (so partial-fill / silence test assertions remain
     * exact).
     *
     * Brightness mapping: `onsetEnvAtRadius * audioConfidence`, squared
     * to preserve perceptual headroom. Saturation is locked at
     * kSaturation; hue is the caller-supplied byte. The production
     * render() path bypasses this helper and uses ctx.palette.getColor()
     * for hardware colour parity — this helper exists for test
     * determinism (CHSV→CRGB is identical on the FastLED mock and on
     * hardware, whereas the palette is mocked-out in NATIVE_BUILD).
     *
     * Note: `silentScale` is NOT applied here — render() multiplies it in
     * after the call so unit tests can drive the helper independently of
     * the silence-fade contract.
     *
     * @param onsetEnvAtRadius  Activity envelope sample at this radius, [0, 1].
     * @param audioConfidence   Audio liveliness gate, [0, 1].
     * @param hueByte           Hue (FastLED 0..255 byte).
     * @return CRGB pixel.
     */
    static CRGB sampleAtRadius(float onsetEnvAtRadius, float audioConfidence, uint8_t hueByte);

    // Private state exposed for unit tests via friend-style accessors.
    // Only the orchestrator (and this effect's tests) should poke these.
    float currentHue()      const { return m_hueOffset; }
    size_t pushedSamples()  const { return (m_state != nullptr) ? m_state->ring.count() : 0; }

    // Test-only helper: drive a push and a render in lockstep without
    // requiring a fully-populated EffectContext. Returns true on success.
    // Native-build-only; not compiled into firmware.
#ifdef NATIVE_BUILD
    bool testTickAndRender(plugins::EffectContext& ctx,
                           float onsetEnv,
                           float audioConfidence,
                           float silentScale,
                           float deltaTimeSeconds);
#endif

    // PSRAM-resident state. Per IEffect.h policy, a ~320 B buffer must not
    // sit in DRAM via a value member. Allocated in init(), freed in cleanup().
    // Public so the translation-unit-local allocateState/freeState helpers
    // (and unit tests) can name it; users are expected to treat m_state as
    // an implementation detail.
    struct PsramData {
        persistence::ScalarRing<float, kRingLength> ring;
    };

private:
    PsramData* m_state         = nullptr;
    float      m_pushAccumSec  = 0.0f;
    float      m_hueOffset     = 0.0f;  // signed offset from kHueAnchor, |x| ≤ kHueBound
    float      m_hueDir        = +1.0f; // direction of next drift step

    // Asymmetric follower for the continuous activity scalar (S2). Updated
    // every render frame for dt-correct smoothing; the *target* is only
    // refreshed when controlBus.hop_seq advances (S3) so dt outpacing the
    // audio hop cadence does not jitter the scope.
    enhancement::AsymmetricFollower m_envFollower{0.0f, kEnvRiseTauSec, kEnvFallTauSec};

    // Last seen audio hop sequence; used to gate follower-target refreshes
    // (S3). uint32_t matches ControlBusFrame::hop_seq.
    uint32_t   m_lastHopSeq    = 0;

    // Cached follower target across render calls — re-applied each frame so
    // the follower keeps converging even on render frames where no fresh
    // audio hop arrived (rare but possible at 120 FPS render / 125 Hz hop).
    float      m_envTarget     = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
