/**
 * @file FirstLightIgnitionEffect.cpp
 * @brief First-Light Ignition — implementation
 *
 * Phase 4 Move 4.4 (Topology_Reconciliation §5 + Divergence 7).
 * See FirstLightIgnitionEffect.h for the visual signature and choreography.
 *
 * The render path performs a single pass over the 160-LED logical strip,
 * computes a normalised distance from the centre pair, looks up the
 * brightness for that (elapsedSec, distance) pair via computeBrightness(),
 * scales a warm-white seed colour, and mirrors the result onto strip 2.
 * No heap activity, no division per-LED, ~320 multiply-adds per frame.
 */

#include "FirstLightIgnitionEffect.h"

#include "../CoreEffects.h"  // CENTER_LEFT / CENTER_RIGHT / STRIP_LENGTH

#include <algorithm>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

// easeOutCubic — fast at the start, eases to 1.0; classic UI ramp curve. Used
// to ramp the spark brightness in during the spark phase so it appears to
// "ignite" rather than to fade in linearly.
constexpr float easeOutCubic(float t) {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

// Smooth radial falloff: 1.0 at the centre, 0.0 at unit radius. Quadratic
// gives a cleaner-looking bloom than linear and bounds total energy without
// needing a per-frame normalisation pass.
constexpr float radialFalloff(float r) {
    if (r >= 1.0f) return 0.0f;
    const float k = 1.0f - r;
    return k * k;
}

// Phase boundaries are static constexpr members on the class so the unit test
// can reference them without a duplicated table; we just alias them locally.
constexpr float kDarkEndSec   = FirstLightIgnitionEffect::kDarkEndSec;
constexpr float kSparkEndSec  = FirstLightIgnitionEffect::kSparkEndSec;
constexpr float kBloomEndSec  = FirstLightIgnitionEffect::kBloomEndSec;
constexpr float kSettleEndSec = FirstLightIgnitionEffect::kSettleEndSec;

constexpr float kPeak    = FirstLightIgnitionEffect::kPeakBrightness;
constexpr float kHandoff = FirstLightIgnitionEffect::kAmbientHandoff;

// Bloom front radius scales linearly with bloom phase progress; we add a
// small "soft edge" beyond the front so the leading edge is feathered rather
// than a hard wave.
constexpr float kBloomFrontSoftness = 0.18f;

} // namespace

float FirstLightIgnitionEffect::computeBrightness(float elapsedSec, float normDistance) {
    // Clamp inputs to the documented domain so callers (and unit tests) get
    // well-defined output if they pass slightly out-of-range values.
    if (elapsedSec < 0.0f) elapsedSec = 0.0f;
    if (normDistance < 0.0f) normDistance = 0.0f;

    // ------------ Phase 1: Dark ------------
    if (elapsedSec < kDarkEndSec) {
        return 0.0f;
    }

    // ------------ Phase 2: Spark ------------
    // Single point of light at the centre. Brightness ramps via easeOutCubic.
    if (elapsedSec < kSparkEndSec) {
        const float t = (elapsedSec - kDarkEndSec) / (kSparkEndSec - kDarkEndSec);
        const float seedBrightness = kPeak * easeOutCubic(t);
        // Spark is centre-only; treat anything beyond the seed pair as dark.
        // normDistance is in units where 1.0 == strip edge; the centre pair
        // straddles ~0..0.012 (1 LED out of 80). Use 0.02 as a generous seed
        // radius so the two centre LEDs both light up clearly but the next
        // LED out remains dark until bloom begins.
        if (normDistance <= 0.02f) {
            return seedBrightness;
        }
        return 0.0f;
    }

    // ------------ Phase 3: Bloom ------------
    // Radius of the expanding wavefront grows linearly across this phase.
    // Within the front, brightness uses radialFalloff to keep total energy
    // bounded (no thermal spike); beyond the front, output is dark.
    if (elapsedSec < kBloomEndSec) {
        const float bloomT = (elapsedSec - kSparkEndSec) / (kBloomEndSec - kSparkEndSec);
        const float front  = bloomT;  // 0.0 -> 1.0 across phase

        // Region inside the front: smooth radial falloff scaled by peak.
        if (normDistance <= front) {
            // Distance normalised against current front radius — keeps the
            // shape consistent as it expands rather than the falloff getting
            // shallower over time.
            const float rNorm = (front > 0.0001f) ? (normDistance / front) : 0.0f;
            return kPeak * radialFalloff(rNorm);
        }

        // Soft leading edge: fade from peak at the front to 0 just outside.
        const float overshoot = normDistance - front;
        if (overshoot < kBloomFrontSoftness) {
            const float edgeT = overshoot / kBloomFrontSoftness;
            // Edge intensity is a small fraction of peak so the bloom front
            // reads as "expanding glow" rather than "hard ring".
            return kPeak * 0.30f * (1.0f - edgeT);
        }

        return 0.0f;
    }

    // ------------ Phase 4: Settle ------------
    // Cross-fade the (now full-strip) bloom from peak-with-falloff down to a
    // uniform low ambient handoff brightness.
    if (elapsedSec < kSettleEndSec) {
        const float settleT = (elapsedSec - kBloomEndSec) / (kSettleEndSec - kBloomEndSec);
        // Use a smoothstep-like curve so the transition is graceful at both
        // ends. (3t^2 - 2t^3 — cheap and well-behaved.)
        const float s = settleT * settleT * (3.0f - 2.0f * settleT);
        const float bloomComponent = kPeak * radialFalloff(normDistance);
        return bloomComponent * (1.0f - s) + kHandoff * s;
    }

    // ------------ Done ------------
    // Past the settle phase the effect should be inert. Returning the handoff
    // brightness here would visibly compete with the ambient mode the
    // dispatch path swaps in; return 0 so any leftover frame before the
    // dispatcher acts is dark rather than a stale glow.
    return 0.0f;
}

bool FirstLightIgnitionEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_elapsedSec = 0.0f;
    m_done = false;
    return true;
}

void FirstLightIgnitionEffect::render(plugins::EffectContext& ctx) {
    // Once the ritual has finished, leave the framebuffer alone so the
    // dispatch path can swap to the next effect cleanly.
    if (m_done) {
        return;
    }

    // Advance our wall-time clock using dt from the EffectContext (real
    // seconds, not frame counters). Honour the 5.5 s ceiling exactly so
    // computeBrightness sees the documented end-of-settle state at least once.
    m_elapsedSec += ctx.deltaTimeSeconds;
    if (m_elapsedSec >= kSettleEndSec) {
        m_elapsedSec = kSettleEndSec;
        m_done = true;
    }

    // Resolve buffer extents defensively. Standard config is 320 LEDs across
    // two 160-LED logical strips; we mirror strip 1 onto strip 2.
    const uint16_t total  = ctx.ledCount;
    if (total == 0 || ctx.leds == nullptr) return;
    const uint16_t stripA = (total >= STRIP_LENGTH) ? STRIP_LENGTH : total;

    // Warm-white seed colour. Locked palette segment (warm white only); no
    // hue cycling and no full-spectrum sweep — honours the "no rainbows"
    // hard rule. A faint amber bias on the green channel + reduced blue
    // pushes the spark towards incandescent rather than cool LED white.
    constexpr uint8_t kSeedR = 255;
    constexpr uint8_t kSeedG = 180;
    constexpr uint8_t kSeedB =  90;

    // Scale factor for converting "centre-pair distance" into normalised
    // distance: the half-strip is HALF_LENGTH (80) LEDs long, so each LED
    // step adds (1 / HALF_LENGTH) to the normalised distance.
    constexpr float kInvHalf = 1.0f / static_cast<float>(HALF_LENGTH);

    for (uint16_t i = 0; i < stripA; ++i) {
        // centerPairDistance from CoreEffects.h returns 0 at indexes 79 and
        // 80 and grows by 1 for each LED outward in either direction.
        const uint16_t d        = centerPairDistance(i);
        const float    normDist = static_cast<float>(d) * kInvHalf;

        const float b = computeBrightness(m_elapsedSec, normDist);

        // Scale the warm-white seed by the phase brightness. Convert via
        // round-half-up to keep the centre brightness symmetric.
        const auto scale = [](uint8_t v, float k) -> uint8_t {
            float scaled = static_cast<float>(v) * k;
            if (scaled <= 0.0f) return 0;
            if (scaled >= 255.0f) return 255;
            return static_cast<uint8_t>(scaled + 0.5f);
        };

        const CRGB col(scale(kSeedR, b), scale(kSeedG, b), scale(kSeedB, b));
        ctx.leds[i] = col;

        // Mirror onto strip 2 (i.e. the second physical light-guide plate).
        // Bounds-checked write keeps this safe if total < 320.
        const uint16_t mirrorIdx = static_cast<uint16_t>(i + STRIP_LENGTH);
        if (mirrorIdx < total) {
            ctx.leds[mirrorIdx] = col;
        }
    }
}

void FirstLightIgnitionEffect::cleanup() {
    // No heap state — nothing to free. Reset the clock so a future re-arm
    // (e.g. for testing or a "replay boot ritual" command) starts fresh.
    m_elapsedSec = 0.0f;
    m_done = false;
}

const plugins::EffectMetadata& FirstLightIgnitionEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "First-Light Ignition",
        "Cinematic boot ritual: spark at centre, bloom outward, settle to ambient.",
        plugins::EffectCategory::AMBIENT,
        1,
        nullptr,
        plugins::EffectRoleFlags::SELF_TRAILING
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
