// CFLSubstepGate.h — Courant–Friedrichs–Lewy stability guard for explicit
// PDE solvers running inside the LED render path.
//
// Phase 1 Move 1.6 / Topology_Reconciliation Phase 6 substrate.
//
// Continuum-dynamics effects (heat / wave / advection-diffusion) integrate
// PDEs explicitly inside render(). Explicit schemes are conditionally
// stable: if dt exceeds the CFL bound, energy grows unboundedly and the
// strip blows out into white noise within a few frames. The bounds for the
// three solver components we care about are:
//
//   Advection:   dt ≤ dx / |v_max|              (Courant condition)
//   Diffusion:   dt ≤ dx² / (2 · D)              (von Neumann, 1-D explicit)
//   Wave:        dt ≤ dx / c                    (Courant for hyperbolic)
//
// At 120 FPS the per-frame dt is ~8.33 ms. On a 320-LED strip with dx = 1
// (LED-spaced sampling), even modest velocity / diffusivity / wave-speed
// values exceed the bound. The fix is to subdivide the frame: run N
// half-step solver passes per frame instead of one.
//
// `cflSubstepCount` returns the integer N satisfying dt/N ≤ smallest active
// CFL bound, clamped to [1, max_steps]. The clamp is the load-bearing
// safety: if true N would exceed max_steps the gate returns max_steps and
// the solver runs anyway with violated stability for that frame. The
// observed visual artefact when this happens is a single-frame "snap"
// (energy momentarily diverges, gets re-bounded by downstream clamps next
// frame) rather than a sustained blow-up — acceptable degradation given a
// hard 2.0 ms render ceiling.
//
// Constraints:
//   * No heap allocation.
//   * O(1), branchy but inlinable; ~50 ns on ESP32-S3 @ 240 MHz.
//   * British English in comments.
//
// Usage:
//   const int N = cflSubstepCount(v_max, D, c, dt, dx, /*max_steps=*/16);
//   const float sub_dt = dt / static_cast<float>(N);
//   for (int s = 0; s < N; ++s) advanceSolver(sub_dt);

#pragma once

#include <cfloat>
#include <cmath>

namespace lightwaveos {
namespace math {

/**
 * @brief CFL stability guard — number of substeps the solver must run
 *        per frame to stay within the Courant bound.
 *
 * @param v_max     Peak advection velocity (LEDs / second). Pass 0 if the
 *                  solver has no advection term.
 * @param D         Diffusion coefficient (LEDs² / second). Pass 0 if the
 *                  solver has no diffusion term.
 * @param c         Wave speed (LEDs / second). Pass 0 if the solver has
 *                  no wave / hyperbolic term.
 * @param dt        Frame duration (seconds). Typically 1/120 ≈ 8.33 ms.
 * @param dx        Grid spacing (LEDs). Almost always 1.0 for LED-strip
 *                  sampling.
 * @param max_steps Hard cap on substep count. When the bound says we
 *                  need more, the gate clamps here and accepts a
 *                  single-frame "snap" artefact rather than blowing the
 *                  render budget.
 *
 * @return Integer in [1, max_steps]. Always ≥ 1 (the solver must run
 *         at least once per frame).
 *
 * Visual artefact when clamping kicks in: a frame-level glitch where
 * the solver field briefly violates stability before downstream clamps
 * (saturation, fadeToBlackBy, etc.) re-bound it. Not a per-frame issue;
 * the next frame's CFL gate re-evaluates with the freshly bounded field.
 *
 * If all three coefficients are zero the solver is idle — there is no
 * stability constraint, and the function returns 1 substep (do not
 * subdivide for free).
 */
inline int cflSubstepCount(float v_max,
                           float D,
                           float c,
                           float dt,
                           float dx,
                           int   max_steps) {
    // Defensive lower bound on max_steps. The solver must run at least
    // once; a caller passing 0 or negative gets one substep regardless.
    if (max_steps < 1) max_steps = 1;

    // Track the tightest active constraint. Initialise to FLT_MAX so any
    // active component immediately tightens it. Using FLT_MAX rather than
    // INFINITY avoids -Wnan-infinity-disabled under the project's
    // -ffast-math flag (platformio.ini common.build_flags), which makes
    // the INFINITY macro's value undefined.
    float needed_dt = FLT_MAX;
    bool any_active = false;

    // Advection: dt_v = dx / |v_max|. Skip when v_max == 0 (no advection).
    if (v_max != 0.0f) {
        const float dt_v = dx / std::fabs(v_max);
        if (dt_v < needed_dt) needed_dt = dt_v;
        any_active = true;
    }

    // Diffusion: dt_D = dx² / (2 · D). Skip when D == 0 (no diffusion).
    // The factor of 2 comes from the 1-D explicit-Euler von Neumann
    // analysis; tighter constants apply in higher dimensions, but the
    // LED strip is fundamentally 1-D.
    if (D != 0.0f) {
        const float dt_D = (dx * dx) / (2.0f * D);
        if (dt_D < needed_dt) needed_dt = dt_D;
        any_active = true;
    }

    // Wave: dt_c = dx / c. Skip when c == 0 (no wave term).
    if (c != 0.0f) {
        const float dt_c = dx / c;
        if (dt_c < needed_dt) needed_dt = dt_c;
        any_active = true;
    }

    // No active constraint → solver is idle → one substep is fine.
    if (!any_active) return 1;

    // Pathological frame duration (zero, negative, NaN). Floor to 1.
    if (!(dt > 0.0f) || !(needed_dt > 0.0f)) return 1;

    // Substep count = ceil(dt / needed_dt). ceilf handles the boundary
    // case dt == needed_dt cleanly (returns 1, not 2).
    const int raw = static_cast<int>(std::ceil(dt / needed_dt));
    if (raw < 1) return 1;
    if (raw > max_steps) return max_steps;
    return raw;
}

}  // namespace math
}  // namespace lightwaveos
