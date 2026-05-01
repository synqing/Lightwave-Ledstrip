/**
 * @file RadialTimeScopeEffect.cpp
 * @brief Radial Time-Scope — implementation.
 *
 * Phase 5 Move 5.4 (LIN-06). See RadialTimeScopeEffect.h for the visual
 * signature, topology compliance, and algorithmic contract.
 *
 * The render path is deliberately tight: one bed-layer pre-pass, one ring
 * read per LED, one palette lookup, one bounds-checked write to strip 1,
 * one to strip 2. No heap activity in render(); all PSRAM allocation
 * lives in init() and is reciprocal-freed in cleanup().
 *
 * ── Surgical-fix map (Phase 5 Move 5.4 follow-up) ─────────────────────
 *
 *   S1: source field is `controlBus.fast_rms` (not `rms`). fast_rms is the
 *       documented continuous activity scalar (~100 ms reactive,
 *       asymmetric pre-smoothed in ControlBus α_fast=0.35); `rms` is the
 *       slower derived field. ADR 2026-03-25 prohibits using `onsetEnv`
 *       as a continuous source.
 *   S2: the source is wrapped in `enhancement::AsymmetricFollower`
 *       (rise=50 ms, fall=300 ms — Effect Standard §3.2 canonical).
 *   S3: the follower's *target* is only refreshed when
 *       `controlBus.hop_seq` advances (audio hop cadence). The follower
 *       itself updates every render frame so smoothing remains dt-correct.
 *   S4: pixel writes use `ctx.palette.getColor(idx, brightness)` —
 *       palette discipline (no CHSV in production path; sampleAtRadius
 *       remains for native test determinism).
 *   S5: silence is a multiplicative gate (`audioConfidence * silentScale`),
 *       not an early return. The render path always runs so the bed
 *       layer keeps silence visible-but-dim.
 *   S6: the `fadeToBlackBy` call is REMOVED. The ring IS the persistence;
 *       the prior 20-80 fade smeared the time axis by double-counting
 *       the ring's natural decay. fadeAmount is reported as 0 via
 *       TRACE_COUNTER for downstream telemetry consistency.
 *   S7: a bed layer at floor brightness is written before the scope so
 *       silence dissolves but never extinguishes the strip (F3 Liquid
 *       Stillness). The bed is itself gated by the silence multiplier.
 *   S8: `getSafeRawDeltaSeconds()` is used for follower update, push
 *       accumulator, and hue drift — speed-knob-independent timing.
 *   S9: new TRACE_COUNTER fields surface follower target, silentScale,
 *       push interval, and the (removed) fade amount.
 */

#include "RadialTimeScopeEffect.h"

#include "../CoreEffects.h"  // CENTER_LEFT/RIGHT, HALF_LENGTH, STRIP_LENGTH, centerPairDistance
#include "../../config/Trace.h"

#include <FastLED.h>          // CRGB / CHSV / scale8 / nscale8_video
#include <cmath>
#include <cstdlib>  // std::malloc / std::free for native builds

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

// Squaring brightness preserves perceptual headroom: a low-mid envelope at
// 0.4 reads as ~16% brightness rather than 40% (which would wash out into
// the brand colour and lose the dynamic). For full-strength onsets the
// curve barely changes the apparent intensity.
inline float perceptualSquare(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x;
}

inline float clamp01(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x;
}

// Local saturating add for uint8 — fastled_mock.h does not expose qadd8,
// and we do not want to gate the bed-layer maths on FastLED-vs-mock.
inline uint8_t satAddU8(uint8_t a, uint8_t b) {
    const uint16_t s = static_cast<uint16_t>(a) + static_cast<uint16_t>(b);
    return (s > 255U) ? static_cast<uint8_t>(255U) : static_cast<uint8_t>(s);
}

// Allocate the ring's PSRAM-resident state. On the native unit-test build
// PSRAM is not available; fall back to plain heap so the same code path is
// exercised in tests as on hardware (alloc → init → render → cleanup).
inline RadialTimeScopeEffect::PsramData* allocateState() {
#ifdef NATIVE_BUILD
    auto* p = static_cast<RadialTimeScopeEffect::PsramData*>(
        std::malloc(sizeof(RadialTimeScopeEffect::PsramData)));
    if (p != nullptr) {
        new (p) RadialTimeScopeEffect::PsramData();
    }
    return p;
#else
    auto* p = static_cast<RadialTimeScopeEffect::PsramData*>(
        heap_caps_malloc(sizeof(RadialTimeScopeEffect::PsramData), MALLOC_CAP_SPIRAM));
    if (p != nullptr) {
        // Placement-new so the ring's default member initialisers run.
        new (p) RadialTimeScopeEffect::PsramData();
    }
    return p;
#endif
}

inline void freeState(RadialTimeScopeEffect::PsramData* p) {
    if (p == nullptr) return;
    p->~PsramData();
#ifdef NATIVE_BUILD
    std::free(p);
#else
    heap_caps_free(p);
#endif
}

}  // namespace

// Native-test helper. The production render() path bypasses this and uses
// ctx.palette.getColor() directly — see S4 in the header.
//
// The early-return on `value == 0` is essential for the partial-fill and
// silence unit tests: in NATIVE_BUILD the mocked PaletteRef::getColor()
// would return CRGB(idx, idx, idx) for every brightness including 0,
// which would break test_silence_scales_brightness_to_zero() and
// test_partial_fill_renders_dark_outside_history_count(). The CHSV path
// honours brightness=0 → CRGB::Black on both hardware and the FastLED
// mock, so the helper stays test-deterministic.
CRGB RadialTimeScopeEffect::sampleAtRadius(float onsetEnvAtRadius,
                                           float audioConfidence,
                                           uint8_t hueByte) {
    // Silence gate: when the audio reactor reports low confidence, the
    // scope writes black at every radius. silentScale is applied
    // separately in render() so unit tests can drive this helper
    // without coupling.
    const float gate = clamp01(audioConfidence);
    if (gate <= 0.0f) {
        return CRGB(0, 0, 0);
    }

    const float env01     = clamp01(onsetEnvAtRadius);
    const float gated     = env01 * gate;
    const float perceived = perceptualSquare(gated);

    uint8_t value = static_cast<uint8_t>(perceived * 255.0f + 0.5f);
    if (perceived > 0.0f && value == 0) {
        // Round-up so a non-zero envelope never collapses to pure black.
        value = 1;
    }
    if (value == 0) {
        return CRGB(0, 0, 0);
    }

    // CHSV→CRGB conversion — available on both FastLED and the
    // fastled_mock used by Unity tests, so the helper stays
    // test-deterministic regardless of palette mocking.
    return CRGB(CHSV(hueByte, RadialTimeScopeEffect::kSaturation, value));
}

bool RadialTimeScopeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Idempotent init — re-arming after cleanup() should not leak.
    if (m_state != nullptr) {
        m_state->ring.clear();
    } else {
        m_state = allocateState();
        if (m_state == nullptr) {
            // Allocation failed; downstream render() guards against this,
            // but signal to the dispatcher that we were unable to come up.
            return false;
        }
    }

    m_pushAccumSec = 0.0f;
    m_hueOffset    = 0.0f;
    m_hueDir       = +1.0f;
    m_lastHopSeq   = 0;
    m_envTarget    = 0.0f;
    m_envFollower.reset(0.0f);
    return true;
}

void RadialTimeScopeEffect::render(plugins::EffectContext& ctx) {
    TRACE_SCOPE("rts_render");

    // Defensive: a failed init() leaves m_state nullptr; a missing LED
    // buffer makes us a no-op. Either way, do not write garbage.
    if (m_state == nullptr) return;
    if (ctx.leds == nullptr || ctx.ledCount == 0) return;

    // S8: dt-correct timing source for follower / push / drift. Speed
    // independence is intentional — the scope timeline is a direct
    // perceptual mapping of seconds-ago and must not stretch with the
    // speed knob.
    const float dt = ctx.getSafeRawDeltaSeconds();

    // ------------ Source the audio context (S1, S3) ------------
    // The native test build has FEATURE_AUDIO_SYNC=0 and exposes a stub
    // AudioContext without `controlBus`; the production build pulls the
    // values from AudioContext helpers backed by the copied ControlBusFrame. The
    // native path is exercised exclusively via testTickAndRender(); this
    // production path is the one tuned for hardware behaviour.
    float audioConfidence = 1.0f;
    float silentScale     = 1.0f;
    float fastRms_        = 0.0f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // S1: continuous activity scalar from fastRms() (NOT rms).
        //   - fast_rms: documented α_fast=0.35 follower (~100 ms reactive)
        //   - rms     : slower derived field (~5x slower follower)
        //   - onsetEnv: ADR 2026-03-25 — advanced/debug, 0 most frames
        fastRms_         = ctx.audio.fastRms();
        const float oflux_ = ctx.audio.onsetFlux();
        const float spike_ = oflux_ * 4.0f;
        const float rawSrc = (fastRms_ > spike_) ? fastRms_ : spike_;

        audioConfidence = ctx.audio.audioConfidence();
        silentScale     = ctx.audio.silentScale();

        // S3: refresh the follower target only when the audio hop
        // sequence advances. The follower's update() runs every render
        // frame regardless — that is what keeps smoothing dt-correct
        // when render FPS outpaces hop cadence (~120 FPS vs ~125 Hz).
        const uint32_t hopSeqNow = ctx.audio.hopSequence();
        if (hopSeqNow != m_lastHopSeq) {
            m_envTarget   = clamp01(rawSrc);
            m_lastHopSeq  = hopSeqNow;
        }

        TRACE_COUNTER("rts_fast_rms", (int)(fastRms_ * 1000.0f));
        TRACE_COUNTER("rts_onset_flux", (int)(oflux_ * 1000.0f));
        TRACE_COUNTER("rts_audio_conf", (int)(audioConfidence * 1000.0f));
        TRACE_COUNTER("rts_silent_scale", (int)(silentScale * 1000.0f));   // S9
    }
#else
    // Native test path: render() is not the call point — testTickAndRender
    // drives the unit tests directly. Keep the production code path
    // hermetic; supply inert defaults if it ever does run on native.
    fastRms_        = ctx.audio.onsetEnv();
    m_envTarget     = clamp01(fastRms_);
    audioConfidence = 1.0f;
    silentScale     = ctx.audio.silentScale();
#endif

    // S2: pull the asymmetric follower forward every frame against the
    // (hop-gated) target. Rise=50 ms / fall=300 ms — Effect Standard §3.2.
    const float envFollowed = m_envFollower.update(m_envTarget, dt);
    TRACE_COUNTER("rts_onset_env_pushed", (int)(envFollowed * 1000.0f));   // S9

    // ------------ S6: NO fadeToBlackBy ------------
    // The ring buffer IS the persistence model — each radius reads its
    // own historical value. Layering a fadeToBlackBy on top double-counts
    // the decay and smears the time axis. Reported as 0 for telemetry
    // parity with effects that do fade.
    const uint8_t fadeAmount = 0;
    TRACE_COUNTER("rts_fade_amount", (int)fadeAmount);                     // S9

    // ------------ S5: multiplicative silence gate (NO early return) ------------
    // Brightness is multiplied by `confGate` per pixel rather than
    // skipping the render. The bed layer (S7) keeps the strip glowing
    // dimly during silence so the panel never appears dead.
    const float confGate = clamp01(audioConfidence) * clamp01(silentScale);

    // ------------ Speed-knob-modulated push cadence ------------
    // Speed knob ([/]) shifts the scope timeline: speed=50 → real-time
    // 60 Hz cadence; speed=100 → 120 Hz (compressed history); speed=10 →
    // 12 Hz (slow scroll). Push interval scales as 1/speedNorm.
    const float speedNorm    = (float)ctx.speed / 50.0f;  // ~0.02..2.0
    const float pushInterval = (speedNorm < 0.05f)
                                  ? (kPushIntervalSec / 0.05f)
                                  : (kPushIntervalSec / speedNorm);
    TRACE_COUNTER("rts_push_interval_us", (int)(pushInterval * 1.0e6f));   // S9

    // ------------ Variable-cadence push ------------
    // Push the smoothed envelope (not the raw target) so the scope
    // timeline preserves the rise/fall envelope shape rather than the
    // raw hop-quantised steps.
    m_pushAccumSec += dt;
    uint8_t pushCount = 0;
    while (m_pushAccumSec >= pushInterval && pushCount < 8) {
        m_state->ring.push(envFollowed);
        m_pushAccumSec -= pushInterval;
        ++pushCount;
    }
    TRACE_COUNTER("rts_pushes", (int)pushCount);
    TRACE_COUNTER("rts_ring_count", (int)m_state->ring.count());

    // ------------ Hue drift (silence-gated) ------------
    // Drift speed scales with audioConfidence so silence freezes the
    // colour. The bound (kHueBound) enforces the "no rainbows" hard rule
    // even if a future change accidentally inflated the drift speed. The
    // byte is interpreted as a palette index in the production render
    // path (S4).
    {
        const float gate = clamp01(audioConfidence);
        m_hueOffset += m_hueDir * kHueDriftPerSec * dt * gate;
        const float bound = static_cast<float>(kHueBound);
        if (m_hueOffset >= bound) {
            m_hueOffset = bound;
            m_hueDir    = -1.0f;
        } else if (m_hueOffset <= -bound) {
            m_hueOffset = -bound;
            m_hueDir    = +1.0f;
        }
    }
    const uint8_t paletteIdx = static_cast<uint8_t>(
        static_cast<int16_t>(kHueAnchor) + static_cast<int16_t>(m_hueOffset));

    // ------------ S7: bed layer ------------
    // Floor brightness proportional to fast_rms with a constant minimum.
    // Gated by confGate so the bed dies during true silence (audio
    // confidence == 0 OR silentScale == 0) — but the floor keeps the
    // strip alive at low-to-medium audio levels even if no fresh ring
    // entry is on this radius. Bed is written FIRST so the scope writes
    // overlay it; high-energy radii light bright while quiet radii
    // reveal the bed glow.
    const uint16_t total  = ctx.ledCount;
    const uint16_t stripA = (total >= STRIP_LENGTH) ? STRIP_LENGTH : total;

#if FEATURE_AUDIO_SYNC
    {
        // Brightness math in uint16 to avoid overflow before clipping.
        // bedFloor + bedRmsGain * fast_rms — with confGate as the
        // silence multiplier (linear, not perceptual: silence should
        // smoothly dim, not snap).
        const uint8_t bedRmsContribution =
            static_cast<uint8_t>(static_cast<float>(kBedRmsGain) *
                                 clamp01(fastRms_) + 0.5f);
        const uint8_t bedRaw = satAddU8(kBedFloor, bedRmsContribution);
        const uint8_t bedBright =
            static_cast<uint8_t>(static_cast<float>(bedRaw) * confGate + 0.5f);

        if (bedBright != 0) {
            const CRGB bedCol = ctx.palette.getColor(kHueAnchor, bedBright);
            for (uint16_t i = 0; i < stripA; ++i) {
                ctx.leds[i] = bedCol;
                const uint16_t mirrorIdx = static_cast<uint16_t>(i + STRIP_LENGTH);
                if (mirrorIdx < total) {
                    ctx.leds[mirrorIdx] = bedCol;
                }
            }
        } else {
            // confGate == 0 → bed is dark. Still need to clear last
            // frame's writes since fadeToBlackBy is absent (S6).
            for (uint16_t i = 0; i < stripA; ++i) {
                ctx.leds[i] = CRGB::Black;
                const uint16_t mirrorIdx = static_cast<uint16_t>(i + STRIP_LENGTH);
                if (mirrorIdx < total) {
                    ctx.leds[mirrorIdx] = CRGB::Black;
                }
            }
        }
    }
#else
    // NATIVE_BUILD: bed layer is omitted to keep the existing unit tests
    // exact — see rationale in sampleAtRadius() above. The test path
    // still expects "no audio activity → black", which demands a
    // bed-free pre-pass. testTickAndRender() handles the test render
    // pass and bypasses this code branch entirely.
#endif

    // ------------ Render pass ------------
    // strip 1 spans [0, STRIP_LENGTH); strip 2 mirrors at
    // [STRIP_LENGTH, 2*STRIP_LENGTH). Bound the inner loop by
    // min(STRIP_LENGTH, ctx.ledCount) so a partial-buffer config
    // (rare; reserved for benches) does not write out of range.
    for (uint16_t i = 0; i < stripA; ++i) {
        const uint16_t dist = centerPairDistance(i);  // 0 at LEDs 79/80, grows outward
        // ScalarRing::atOffset is bounds-safe: returns T{}=0.0f if dist >=
        // count(). Partial-fill behaviour is therefore: LEDs at distance
        // ≥ count() render dark over the bed, which is exactly the desired
        // "the scope hasn't seen that far back yet" semantic.
        const float histValue = m_state->ring.atOffset(static_cast<size_t>(dist));

#if FEATURE_AUDIO_SYNC
        // S4: production path — palette lookup, NOT CHSV.
        // Brightness is the perceptual square of the (gated) history
        // value. confGate (S5) folds in audioConfidence and silentScale
        // multiplicatively so silence linearly dims the scope without
        // killing the render path.
        const float gated     = clamp01(histValue) * confGate;
        const float perceived = perceptualSquare(gated);

        uint8_t value8 = static_cast<uint8_t>(perceived * 255.0f + 0.5f);
        if (perceived > 0.0f && value8 == 0) {
            value8 = 1;  // never collapse a non-zero envelope to dark
        }

        if (value8 != 0) {
            const CRGB col = ctx.palette.getColor(paletteIdx, value8);
            // Overlay the scope on top of the bed (replace, not add — bed
            // brightness is dim by construction so the strongest of the
            // two readings always reads as the scope value).
            ctx.leds[i] = col;
            const uint16_t mirrorIdx = static_cast<uint16_t>(i + STRIP_LENGTH);
            if (mirrorIdx < total) {
                ctx.leds[mirrorIdx] = col;
            }
        }
        // value8 == 0 → leave the bed-layer pixel untouched; the bed
        // glow remains visible at this radius.
#else
        // NATIVE_BUILD: legacy CHSV-helper path keeps the unit tests
        // deterministic against the FastLED mock. Production hardware
        // never enters this branch.
        CRGB col = sampleAtRadius(histValue, audioConfidence, paletteIdx);
        const float silenceMul = clamp01(silentScale);
        if (silenceMul < 1.0f) {
            col.r = static_cast<uint8_t>(static_cast<float>(col.r) * silenceMul);
            col.g = static_cast<uint8_t>(static_cast<float>(col.g) * silenceMul);
            col.b = static_cast<uint8_t>(static_cast<float>(col.b) * silenceMul);
        }
        ctx.leds[i] = col;
        const uint16_t mirrorIdx = static_cast<uint16_t>(i + STRIP_LENGTH);
        if (mirrorIdx < total) {
            ctx.leds[mirrorIdx] = col;
        }
#endif
    }
}

void RadialTimeScopeEffect::cleanup() {
    freeState(m_state);
    m_state         = nullptr;
    m_pushAccumSec  = 0.0f;
    m_hueOffset     = 0.0f;
    m_hueDir        = +1.0f;
    m_lastHopSeq    = 0;
    m_envTarget     = 0.0f;
    m_envFollower.reset(0.0f);
}

const plugins::EffectMetadata& RadialTimeScopeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Radial Time-Scope",
        "Activity history scrolls outward from centre 79/80; distance reads as time-ago.",
        plugins::EffectCategory::AMBIENT,
        1,
        nullptr,
        plugins::EffectRoleFlags::SELF_TRAILING
    };
#ifndef NATIVE_BUILD
    meta.id = kId;
#endif
    return meta;
}

#ifdef NATIVE_BUILD
bool RadialTimeScopeEffect::testTickAndRender(plugins::EffectContext& ctx,
                                              float onsetEnv,
                                              float audioConfidence,
                                              float silentScale,
                                              float deltaTimeSeconds) {
    if (m_state == nullptr) return false;
    if (ctx.leds == nullptr || ctx.ledCount == 0) return false;

    // Drive the 60 Hz push cadence directly with the supplied values so
    // tests do not depend on the FEATURE_AUDIO_SYNC code path. NOTE: the
    // test helper deliberately bypasses the AsymmetricFollower so unit
    // tests have an exact, dt-quantised relationship between input
    // values and ring contents — this is what tests 1, 2, 3, 5, 6, 9
    // depend on for their pre-/post-conditions.
    m_pushAccumSec += deltaTimeSeconds;
    while (m_pushAccumSec >= kPushIntervalSec) {
        m_state->ring.push(onsetEnv);
        m_pushAccumSec -= kPushIntervalSec;
    }

    // Hue drift (matching production path, silence-gated by audioConfidence).
    {
        const float gate = clamp01(audioConfidence);
        m_hueOffset += m_hueDir * kHueDriftPerSec * deltaTimeSeconds * gate;
        const float bound = static_cast<float>(kHueBound);
        if (m_hueOffset >= bound) { m_hueOffset = bound; m_hueDir = -1.0f; }
        else if (m_hueOffset <= -bound) { m_hueOffset = -bound; m_hueDir = +1.0f; }
    }
    const uint8_t huePixel = static_cast<uint8_t>(
        static_cast<int16_t>(kHueAnchor) + static_cast<int16_t>(m_hueOffset));

    const uint16_t total  = ctx.ledCount;
    const uint16_t stripA = (total >= STRIP_LENGTH) ? STRIP_LENGTH : total;
    const float silenceMul = clamp01(silentScale);

    // No bed layer in the native test path — see the rationale in
    // render() for why the bed is FEATURE_AUDIO_SYNC-only. The tests
    // assert exact-zero pixels for partial-fill / silence, which a
    // bed-layer pre-pass would violate.

    for (uint16_t i = 0; i < stripA; ++i) {
        const uint16_t dist = centerPairDistance(i);
        const float histValue = m_state->ring.atOffset(static_cast<size_t>(dist));
        CRGB col = sampleAtRadius(histValue, audioConfidence, huePixel);
        if (silenceMul < 1.0f) {
            col.r = static_cast<uint8_t>(static_cast<float>(col.r) * silenceMul);
            col.g = static_cast<uint8_t>(static_cast<float>(col.g) * silenceMul);
            col.b = static_cast<uint8_t>(static_cast<float>(col.b) * silenceMul);
        }
        ctx.leds[i] = col;
        const uint16_t mirrorIdx = static_cast<uint16_t>(i + STRIP_LENGTH);
        if (mirrorIdx < total) {
            ctx.leds[mirrorIdx] = col;
        }
    }
    return true;
}
#endif  // NATIVE_BUILD

}  // namespace ieffect
}  // namespace effects
}  // namespace lightwaveos
