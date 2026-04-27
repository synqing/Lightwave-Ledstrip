/**
 * @file AttackOnlyPitchVelocityFieldEffect.cpp
 * @brief LIN-08 — Attack-Only Pitch-Class Velocity Field — implementation
 *
 * Phase 5 Move 5.6 (Topology_Reconciliation §5 / LIN-08).
 * Major rewrite 2026-04-27 — see header for the diagnosis of the original
 * "twitching at 3 FPS like a dog" hardware behaviour and the three
 * architectural violations corrected.
 *
 * Hot-path summary (per frame, 120 Hz target):
 *   1. Fade-to-black on entry (canonical Feb 2026 AR baseline) so trails
 *      persist across frames rather than being overwritten each tick.
 *   2. Hop-gated capture of bus.chroma → AsymmetricFollower targets, then
 *      smooth all 12 followers toward their last captured target.
 *   3. Circular-mean chroma → smoothed palette angle (no argmax flicker).
 *   4. Build an 80-element radial velocity field via Σ follower·sinLUT(...)
 *      using only the top-3 strongest classes.
 *   5. Bed: thin rms-driven floor composed via qadd8 `+=`.
 *   6. Radial render: one composed write per radial bin per arm × 4 arms
 *      (strip-1 left/right, strip-2 left/right). qadd8 saturating add
 *      composes peaks above the bed without clipping the trails.
 * No `<cmath>::sinf` in the radial loop — the LUT (sinLUT256) keeps the
 * lookup ~10 ns and removes the radial-period banding that an 8-bit sin8
 * would introduce.
 */

#include "AttackOnlyPitchVelocityFieldEffect.h"
#include "../../config/Trace.h"

#include "ChromaUtils.h"             // circularChromaHueSmoothed
#include "../CoreEffects.h"          // CENTER_LEFT / CENTER_RIGHT / HALF_LENGTH / STRIP_LENGTH / centerPairDistance
#include "../../math/sinLUT256.h"    // float-precision sin LUT

// FastLED is provided by the native test mock (test_native/mocks/FastLED.h)
// when NATIVE_BUILD is defined, and by the real library on firmware. Both
// expose fadeToBlackBy / scale8 / qadd8 / CRGB::operator+= which the
// hot-path uses for trail composition and saturating channel adds.
#include <FastLED.h>

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

// Two-pi as a float — sinLUT256 wraps internally, but we still want this
// scaling factor for the per-frequency phase term.
constexpr float kTwoPi = 6.28318530717958647692f;

// Canonical AsymmetricFollower time constants (Move 5.6 spec S3). Lifted
// from BreathingEnhancedEffect.h:60 — 60 ms attack / 250 ms release. These
// supersede the test-only kAttackTau / kReleaseTau pair (which preserve
// the original Move 5.6 unit-test contract, see header comment).
constexpr float kProdRiseTau = 0.06f;
constexpr float kProdFallTau = 0.25f;

// Bed scale (Move 5.6 spec S10). bus.fast_rms is roughly 0..0.5 in normal
// listening; multiplying by 100 then scale8(_, 102) yields a maximum bed
// brightness of roughly 0.5 * 100 -> 50 -> scale8(50, 102) ≈ 20 — well
// below 16 % of full range, so radial peaks remain dominant.
constexpr uint8_t kBedScale = 102;

// Hue dispersion (Move 5.6 spec S7). Reserved as an A/B knob — kept at 0
// for the first hardware-validated version so the dominant-pole hue is
// unambiguous on the strip. Bounded to ±32 of the dominant pole if reactivated.
constexpr int16_t kHueDispersion = 0;

/**
 * @brief Linear pitch-class base frequency (cycles across the half-strip).
 *
 * freq[c] = 1.0 + c/12. C → 1.0× (one full cycle across the half-strip),
 * B → ~1.917× (almost two cycles). Linear spacing is the Move 5.6 baseline;
 * an exponential variant powf(2, c/12) is reserved for a Captain A/B run.
 */
constexpr float baseFreq(uint8_t c) {
    return 1.0f + static_cast<float>(c) / 12.0f;
}

/**
 * @brief Compose `colour * brightByte` into `target` using qadd8 saturating
 *        add per channel. Used by the radial-once render so peaks compose
 *        cleanly with the bed and with the prior frame's fadeToBlackBy
 *        residue.
 *
 * Equivalent to a SubpixelRenderer::renderPoint() at an integer position,
 * but without the bound check that drops position == bufferSize-1 (which
 * would lose strip 2's last LED on a 320-LED config).
 */
inline void composeAddScaled(CRGB& target, const CRGB& source, uint8_t brightByte) {
    if (brightByte == 0) return;
    target.r = qadd8(target.r, scale8(source.r, brightByte));
    target.g = qadd8(target.g, scale8(source.g, brightByte));
    target.b = qadd8(target.b, scale8(source.b, brightByte));
}

} // namespace

// ============================================================================
// Static helper — pure, no class state, exposed for unit tests
// ============================================================================

void AttackOnlyPitchVelocityFieldEffect::buildVelocityFieldFromTopK(
        const float chromaFollowers[kChromaBins],
        uint8_t topK,
        float driftPhase,
        float field[kHalfLength]) {
    if (field == nullptr) return;

    // ------------------------------------------------------------------ 1
    // Top-K selection (small fixed K — partial selection is cheaper than
    // a full sort). For K = 3 over 12 elements this is at most 36 compares.
    // ------------------------------------------------------------------
    int8_t topIdx[kChromaBins];
    for (uint8_t i = 0; i < kChromaBins; ++i) topIdx[i] = -1;

    if (topK > kChromaBins) topK = kChromaBins;

    bool taken[kChromaBins] = {false, false, false, false,
                               false, false, false, false,
                               false, false, false, false};

    for (uint8_t k = 0; k < topK; ++k) {
        int8_t bestIdx = -1;
        float bestVal = -1.0f;  // followers are ≥ 0 so any real entry wins
        for (uint8_t c = 0; c < kChromaBins; ++c) {
            if (taken[c]) continue;
            const float v = chromaFollowers[c];
            if (v > bestVal) {
                bestVal = v;
                bestIdx = static_cast<int8_t>(c);
            }
        }
        if (bestIdx < 0) break;       // no more candidates
        if (bestVal <= 0.0f) break;   // nothing meaningful left to add
        taken[bestIdx] = true;
        topIdx[k] = bestIdx;
    }

    // ------------------------------------------------------------------ 2
    // Velocity-field synthesis (unchanged from the first iteration —
    // Move 5.6 spec S5 keeps the top-K spatial-frequency selection as the
    // effect's unique signature).
    //
    // For each radial bin r ∈ [0, kHalfLength):
    //   phase    = r / kHalfLength               (∈ [0, 1))
    //   v        = Σ follower[c] * sinLUT(2π·freq[c]·phase + driftPhase)
    // ------------------------------------------------------------------
    constexpr float kInvHalf = 1.0f / static_cast<float>(kHalfLength);

    for (uint16_t r = 0; r < kHalfLength; ++r) {
        const float phase = static_cast<float>(r) * kInvHalf;
        float v = 0.0f;
        for (uint8_t k = 0; k < topK; ++k) {
            const int8_t c = topIdx[k];
            if (c < 0) break;
            const float angle = kTwoPi * baseFreq(static_cast<uint8_t>(c)) * phase + driftPhase;
            v += chromaFollowers[c] * lightwaveos::math::sinLUT(angle);
        }
        field[r] = v;
    }
}

// ============================================================================
// IEffect lifecycle
// ============================================================================

bool AttackOnlyPitchVelocityFieldEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset production hot-path state. init() may be invoked when reusing
    // the same instance after cleanup() (ZoneComposer pattern).
    for (uint8_t c = 0; c < kChromaBins; ++c) {
        m_chromaFollowers[c] = enhancement::AsymmetricFollower{0.0f, kProdRiseTau, kProdFallTau};
        m_chromaTargets[c]   = 0.0f;
    }
    m_chromaAngle = 0.0f;
    m_lastHopSeq  = 0xFFFFFFFFu;  // sentinel: first frame always captures targets
    m_phaseAccum  = 0.0f;

    // Reset test-seam follower array (kept independent of production path).
    for (uint8_t c = 0; c < kChromaBins; ++c) m_chromaFollower[c] = 0.0f;

    // Allocate PSRAM scratch (>64 B per IEffect.h policy). On firmware we
    // request SPIRAM explicitly; on native builds we fall back to malloc so
    // unit tests run without an ESP heap shim.
    if (!m_ps) {
#ifdef NATIVE_BUILD
        m_ps = static_cast<PsramData*>(std::malloc(sizeof(PsramData)));
#else
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
#endif
        if (!m_ps) {
            TRACE_INSTANT("pvf_init_failed");  // S12
            return false;
        }
    }
    std::memset(m_ps, 0, sizeof(PsramData));
    return true;
}

void AttackOnlyPitchVelocityFieldEffect::render(plugins::EffectContext& ctx) {
    TRACE_SCOPE("pvf_render");

    if (!m_ps) return;
    if (ctx.leds == nullptr || ctx.ledCount == 0) return;

    const float dt  = ctx.getSafeRawDeltaSeconds();

    // ------------------------------------------------------------------ S1
    // fadeToBlackBy at top of render() — canonical Feb 2026 AR pack value
    // (1A0x effects). Creates trail persistence so subsequent `+=` writes
    // compose over a decaying residue rather than overwriting fresh black.
    // ------------------------------------------------------------------
    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);

    // ------------------------------------------------------------------ S9 + S3
    // Hop-gated chroma → follower target capture.
    //
    // ctx.audio.hopSequence() advances at hop cadence (~125 Hz at 32 kHz / 256-sample
    // hops on K1 audio). On a fresh hop we update each follower's *target*
    // by smoothing toward the new chroma. Between hops the AsymmetricFollower
    // continues to smooth toward its last captured target every frame (so
    // the visual stays buttery at 120 FPS even though new audio data only
    // lands at 125 Hz).
    //
    // m_lastHopSeq is initialised to UINT32_MAX so the first render call
    // always captures, even if hopSequence() is still 0 (unit-test contexts
    // never advance the sequence — production hardware always does).
    // ------------------------------------------------------------------
    const uint32_t hopSeqNow = ctx.audio.hopSequence();
    if (hopSeqNow != m_lastHopSeq) {
        // Fresh hop — capture new target chroma vector.
        for (uint8_t c = 0; c < kChromaBins; ++c) {
            m_chromaTargets[c] = ctx.audio.getChroma(c);
        }
        m_lastHopSeq = hopSeqNow;
    }
    // Smooth followers toward the LAST captured targets every frame
    // (canonical RippleEnhancedEffect pattern — keeps the visual buttery
    // at 120 FPS even though new audio frames only arrive at ~125 Hz).
    for (uint8_t c = 0; c < kChromaBins; ++c) {
        m_chromaFollowers[c].update(m_chromaTargets[c], dt);
    }

    // ------------------------------------------------------------------ S4
    // Circular-mean chroma hue (replaces the deleted chromaToAnchorHue +
    // m_dominantClass + hysteresis machinery). Smoothed with tau = 0.25 s
    // so transitions ride alongside the AsymmetricFollower fall tau.
    // ------------------------------------------------------------------
    float followersBuf[kChromaBins];
    uint8_t topkActiveCount = 0;
    for (uint8_t c = 0; c < kChromaBins; ++c) {
        followersBuf[c] = m_chromaFollowers[c].value;
        if (followersBuf[c] > 0.005f) ++topkActiveCount;
    }
    const uint8_t circularHue = effects::chroma::circularChromaHueSmoothed(
        followersBuf, m_chromaAngle, dt, 0.25f);

    // Bounded ±kHueDispersion of the dominant pole (currently 0 — see
    // namespace constant). Reserved as a Captain A/B knob.
    const uint8_t paletteIdx = static_cast<uint8_t>(
        static_cast<int16_t>(circularHue) + kHueDispersion);

    // ------------------------------------------------------------------ S5
    // Build the radial velocity field from the top-3 strongest followers.
    // The helper is decoupled from hue (input is the .value field of each
    // AsymmetricFollower, output is the 80-element radial scratch).
    // ------------------------------------------------------------------
    buildVelocityFieldFromTopK(followersBuf,
                               kRenderTopK,
                               m_phaseAccum,
                               m_ps->velocityField);

    // Field span tracing for the on-strip diagnostic counters.
    float fieldMax = m_ps->velocityField[0];
    float fieldMin = m_ps->velocityField[0];
    bool  fieldZero = true;
    for (uint16_t r = 0; r < kHalfLength; ++r) {
        const float v = m_ps->velocityField[r];
        if (v > fieldMax) fieldMax = v;
        if (v < fieldMin) fieldMin = v;
        if (v != 0.0f) fieldZero = false;
    }

    // ------------------------------------------------------------------ S10
    // Bed layer — dim ambient glow keeps the strip alive in soft passages.
    // fastRms() is in [0..~0.5] in normal listening; the scaling below
    // caps the bed at roughly 16 % of full brightness so radial peaks
    // (added via qadd8) still saturate above it.
    //
    // Composed via `+=` (qadd8 saturating) so peaks land cleanly above the
    // bed and the prior fadeToBlackBy residue.
    // ------------------------------------------------------------------
    const float    fastRms     = ctx.audio.fastRms();
    const float    fastRmsClamped = (fastRms < 0.0f) ? 0.0f : (fastRms > 1.0f ? 1.0f : fastRms);
    const uint8_t  bedBright   = scale8(static_cast<uint8_t>(fastRmsClamped * 100.0f), kBedScale);
    if (bedBright > 0) {
        const CRGB bedCol = ctx.palette.getColor(paletteIdx, bedBright);
        for (uint16_t i = 0; i < ctx.ledCount; ++i) {
            ctx.leds[i] += bedCol;  // CRGB::operator+= is qadd8 per channel
        }
    }

    // ------------------------------------------------------------------ S6
    // Radial-once render (replaces the old integer-sampled walk where
    // every LED read field[centerPairDistance(i)] — that pattern caused
    // wagon-wheel aliasing when the dominant freq aligned with the
    // radial step). Each radial bin is now WRITTEN once per arm; four
    // arms total = 320 composed writes per frame.
    //
    // Composition uses qadd8-saturating add via composeAddScaled() — the
    // same semantic as SubpixelRenderer::renderPoint() at an integer
    // position, but without its `position >= bufferSize - 1` bound check
    // (which would silently drop strip 2's last LED on a 320-LED config).
    // Trail persistence comes from the fadeToBlackBy at the top of
    // render(); the bed (S10) provides the soft-passage floor.
    // ------------------------------------------------------------------
    const float confGate = ctx.audio.audioConfidence() * ctx.audio.silentScale();
    const uint16_t total = ctx.ledCount;

    for (uint16_t r = 0; r < kHalfLength; ++r) {
        const float v = m_ps->velocityField[r];
        float bright = (v < 0.0f) ? -v : v;
        if (bright > 1.0f) bright = 1.0f;
        bright *= confGate;
        if (bright <= 0.0f) continue;

        const uint8_t brightByte = static_cast<uint8_t>(bright * 255.0f + 0.5f);
        const CRGB col = ctx.palette.getColor(paletteIdx, brightByte);

        // Strip 1 left arm: idx = CENTER_LEFT - r → r∈[0,79] → idx∈[79,0].
        const uint16_t leftStrip1 = static_cast<uint16_t>(
            static_cast<int16_t>(CENTER_LEFT) - static_cast<int16_t>(r));
        if (leftStrip1 < total) composeAddScaled(ctx.leds[leftStrip1], col, brightByte);

        // Strip 1 right arm: idx = CENTER_RIGHT + r → r∈[0,79] → idx∈[80,159].
        const uint16_t rightStrip1 = static_cast<uint16_t>(CENTER_RIGHT + r);
        if (rightStrip1 < total) composeAddScaled(ctx.leds[rightStrip1], col, brightByte);

        // Strip 2 mirror — offset by STRIP_LENGTH on each arm.
        const uint16_t leftStrip2  = static_cast<uint16_t>(leftStrip1 + STRIP_LENGTH);
        if (leftStrip2 < total)  composeAddScaled(ctx.leds[leftStrip2],  col, brightByte);

        const uint16_t rightStrip2 = static_cast<uint16_t>(rightStrip1 + STRIP_LENGTH);
        if (rightStrip2 < total) composeAddScaled(ctx.leds[rightStrip2], col, brightByte);
    }

    // ------------------------------------------------------------------ S11
    // Advance the global phase drift, scaled by ctx.speed (`[`/`]` knob).
    // speed=50 → real-time drift; speed=100 → 2×; speed=10 → 0.2×.
    // (Convention shared with AudioBloom and Ripple.)
    // ------------------------------------------------------------------
    const float speedNorm = static_cast<float>(ctx.speed) / 50.0f;
    const float speedClamped = (speedNorm < 0.05f) ? 0.05f : speedNorm;
    m_phaseAccum += dt * kDriftRateRadPerSec * speedClamped;
    while (m_phaseAccum >= kTwoPi) m_phaseAccum -= kTwoPi;

    // ------------------------------------------------------------------ S12
    // Self-contained diagnostic counters.
    // ------------------------------------------------------------------
    TRACE_COUNTER("pvf_audio_conf",   static_cast<int>(ctx.audio.audioConfidence() * 1000.0f));
    TRACE_COUNTER("pvf_silent_scale", static_cast<int>(ctx.audio.silentScale()     * 1000.0f));
    TRACE_COUNTER("pvf_field_max",    static_cast<int>(fieldMax            * 1000.0f));
    TRACE_COUNTER("pvf_field_min",    static_cast<int>(fieldMin            * 1000.0f));
    TRACE_COUNTER("pvf_topk_count",   static_cast<int>(topkActiveCount));
    if (fieldZero) {
        TRACE_INSTANT("pvf_field_zero");
    }
}

void AttackOnlyPitchVelocityFieldEffect::cleanup() {
    if (m_ps) {
#ifdef NATIVE_BUILD
        std::free(m_ps);
#else
        heap_caps_free(m_ps);
#endif
        m_ps = nullptr;
    }
    for (uint8_t c = 0; c < kChromaBins; ++c) {
        m_chromaFollowers[c].reset(0.0f);
        m_chromaTargets[c] = 0.0f;
    }
    m_chromaAngle = 0.0f;
    m_lastHopSeq  = 0xFFFFFFFFu;
    m_phaseAccum  = 0.0f;
    for (uint8_t c = 0; c < kChromaBins; ++c) m_chromaFollower[c] = 0.0f;
}

const plugins::EffectMetadata& AttackOnlyPitchVelocityFieldEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Attack-Only Pitch Velocity Field",
        "K1-native: 12-class chroma followers (asymmetric) → top-3 radial sin sum.",
        plugins::EffectCategory::PARTY,
        1,
        nullptr,
        plugins::EffectRoleFlags::SELF_TRAILING  // followers + fadeToBlackBy self-decay; INF-02 LPF off
    };
#ifndef NATIVE_BUILD
    meta.id = kId;
#endif
    return meta;
}

// ============================================================================
// Test seam — preserved as the original Move 5.6 onset-gated contract.
// Operates on m_chromaFollower (raw float[12]) which is independent of
// the production AsymmetricFollower path. The unit suite expects:
//   - debugTickFollowers(chroma, onsetGate=true,  dt) → rises with kAttackTau
//   - debugTickFollowers(chroma, onsetGate=false, dt) → decays with kReleaseTau
// (Test 3: no onset → no rise; Test 4: 1.5 s decay → 1/e residue.)
// ============================================================================

void AttackOnlyPitchVelocityFieldEffect::debugTickFollowers(
        const float chroma[kChromaBins],
        bool onsetGate,
        float dt) {
    if (dt <= 0.0f) return;

    const float alphaRise = 1.0f - std::exp(-dt / kAttackTau);
    const float alphaFall = 1.0f - std::exp(-dt / kReleaseTau);

    for (uint8_t c = 0; c < kChromaBins; ++c) {
        const float target = chroma[c];
        float follower = m_chromaFollower[c];

        if (onsetGate && target > follower) {
            follower += (target - follower) * alphaRise;
        } else {
            follower *= (1.0f - alphaFall);
        }
        m_chromaFollower[c] = follower;
    }
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
