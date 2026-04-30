/**
 * @file FirstLightIgnitionEffect.h
 * @brief First-Light Ignition — cinematic one-shot boot animation
 *
 * Effect ID: 0x2000 (FAMILY_SYSTEM)
 * Family: SYSTEM / Lifecycle
 * Tags: CENTER_ORIGIN, ONE_SHOT, SELF_TRAILING
 *
 * Topology Reconciliation §5 Phase 4 Move 4.4 (Codex-elevated Divergence 7):
 * a 4–6 s wake-up ritual that lights up the strip from the centre on power-on,
 * then yields to the normal ambient/active mode. Disproportionately important
 * for launch-video and product-ritual value at low engineering cost.
 *
 * Visual signature (so Captain can verify against expectation):
 *   • t = 0 .. 0.5 s   — strip dark, no LEDs lit.
 *   • t = 0.5 .. 1.5 s — single bright warm-white spark forms at LED 79/80
 *                        (centre pair), brightness eases in via easeOutCubic
 *                        from 0 to peak.
 *   • t = 1.5 .. 4.5 s — bloom expands outward symmetrically from the centre
 *                        toward both edges; brightness falls off with distance
 *                        from centre using a smooth radial profile so total
 *                        energy stays bounded (no thermal spike).
 *   • t = 4.5 .. 5.5 s — cross-fade down to a low ambient handoff brightness.
 *   • t ≥ 5.5 s        — `isDone()` returns true; render() becomes a no-op so
 *                        the dispatch path can swap to the configured normal
 *                        mode without flicker.
 *
 * Constraints honoured:
 *   • HW-03 centre origin: spawn point is the LED 79/80 pair, expansion is
 *     mirrored across the centre (and across both physical strips).
 *   • No heap allocation in render() — all state is static class members.
 *   • dt-correct: phase progression is driven by `deltaTimeSeconds` from
 *     EffectContext, not frame counters. Hardware FPS variation does not
 *     change the perceived choreography duration.
 *   • 2.0 ms render ceiling: at most ~640 LED writes per frame (2 strips ×
 *     320 LEDs) with a single multiply-add per LED — well under budget.
 *   • No rainbows: warm-white spark transitions to a single locked palette-
 *     aligned hue band; never sweeps the full hue wheel.
 *   • British English in comments and identifiers.
 *
 * Self-trailing: the effect bakes its own falloff and fade, so it carries
 * `EffectRoleFlags::SELF_TRAILING` per INF-06 — INF-02's framebuffer LPF
 * should not double-trail this one.
 *
 * Instance state (all static / DRAM-friendly, well under the 64-byte member
 * threshold for PSRAM offload):
 *   - m_elapsedSec : float — total wall-time since init() in seconds
 *   - m_done       : bool  — true once the settle phase has completed
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class FirstLightIgnitionEffect : public plugins::IEffect {
public:
#ifndef NATIVE_BUILD
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_FIRST_LIGHT_IGNITION;
#endif

    // Phase boundaries (seconds since init). Exposed so the unit test can
    // verify behaviour at named instants without duplicating constants.
    static constexpr float kDarkEndSec   = 0.50f;
    static constexpr float kSparkEndSec  = 1.50f;
    static constexpr float kBloomEndSec  = 4.50f;
    static constexpr float kSettleEndSec = 5.50f;  // total run length = 5.5 s

    // Brightness anchors (linear 0..1). The settle phase fades from peak down
    // to this floor before yielding to the dispatch path's ambient mode.
    static constexpr float kPeakBrightness    = 1.00f;  // full local energy at spark/bloom
    static constexpr float kAmbientHandoff    = 0.18f;  // soft glow at handoff

    FirstLightIgnitionEffect() = default;
    ~FirstLightIgnitionEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    /**
     * @brief Has the boot ritual finished?
     *
     * Returns true once total elapsed time has passed kSettleEndSec. The
     * dispatch path may poll this each frame and switch to the configured
     * normal mode on the first true reading. Once true, render() becomes a
     * no-op so the framebuffer is left intact for the next effect.
     */
    bool isDone() const { return m_done; }

    /**
     * @brief Compute brightness at (elapsedSec, distanceFromCentre) without
     *        touching framebuffers.
     *
     * Pure function — exposed for unit tests so phase boundaries and the
     * mirror-symmetric falloff curve can be verified without setting up an
     * EffectContext. Returns a linear brightness in [0, 1].
     *
     * @param elapsedSec     Seconds since init.
     * @param normDistance   Normalised distance from the centre pair, 0.0 at
     *                       the spark seed (LED 79/80) and 1.0 at the strip
     *                       edges. Negative values are clamped to 0.
     */
    static float computeBrightness(float elapsedSec, float normDistance);

private:
    float m_elapsedSec = 0.0f;
    bool  m_done       = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
