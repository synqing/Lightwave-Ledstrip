// Phase 1 Move 1.6 — Math substrate test (sinLUT256 + CFLSubstepGate)
//
// Per Topology_Reconciliation Phase 6 (continuum-dynamics): solver-class
// effects (heat / wave / advection-diffusion) need two cheap primitives that
// today's render path does not provide:
//
//   1. sinLUT(angle) — float-precision sin lookup with linear interp. FastLED
//      ships sin8 (8-bit, ±1/256 quantisation), which is fine for hue cycling
//      but coarse for phase-coherent oscillators where banding shows up as
//      visible quantisation steps. A 256-entry float table costs ~1 KB flash
//      and gives <0.01 absolute error vs std::sin across the full circle.
//
//   2. cflSubstepCount(...) — Courant–Friedrichs–Lewy stability gate. Explicit
//      PDE solvers blow up if dt > dx / |v_max| (advection), dt > dx²/(2D)
//      (diffusion), or dt > dx / c (wave). At 120 FPS the per-frame dt is
//      ~8.3 ms; on a 320-LED strip with dx = 1, even mild velocities exceed
//      the Courant limit. The gate returns the number of substeps the solver
//      must run per frame to stay stable, clamped at max_steps so we degrade
//      gracefully rather than livelock the renderer. When the cap is hit the
//      visual artefact is a frame-level "snap" (stability is violated for
//      that frame only); per-frame cost stays bounded.
//
// Both helpers are header-only, no heap, O(1) — sinLUT ~10 ns, CFL gate ~50 ns
// on ESP32-S3 @ 240 MHz. They live in lightwaveos::math so future modules
// (continuum effects, fluid sim, FDTD) can pull just the headers without
// dragging in effect framework dependencies.

#include <unity.h>
#include <cmath>
#include <cstdint>

#include "math/sinLUT256.h"
#include "math/CFLSubstepGate.h"

using lightwaveos::math::sinLUT;
using lightwaveos::math::cflSubstepCount;

namespace {

// Tolerance for sinLUT vs std::sin. 256 entries with linear interp gives
// theoretical max error ~6e-5 at the curve's inflection points; we set the
// gate at 0.01 to leave headroom for compiler float folding differences.
constexpr float kSinTolerance = 0.01f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

// 1 — Cardinal points: 0, π/2, π, 3π/2, 2π must match standard sin to
// well within tolerance. These are the points effects hit most often when
// driving phase-aligned oscillators.
void test_sinLUT_cardinal_points() {
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, 0.0f,  sinLUT(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, 1.0f,  sinLUT(kPi * 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, 0.0f,  sinLUT(kPi));
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, -1.0f, sinLUT(kPi * 1.5f));
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, 0.0f,  sinLUT(kTwoPi));
}

// 2 — Negative angles. Effects with phase offsets routinely pass negative
// arguments; the LUT must wrap them onto [0, 2π) before lookup.
void test_sinLUT_negative_angles() {
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, -1.0f, sinLUT(-kPi * 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance,  0.0f, sinLUT(-kPi));
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance,  1.0f, sinLUT(-kPi * 1.5f));
}

// 3 — Large angles must wrap correctly. Phase accumulators in long-running
// effects drift to large positive values; sinLUT must remain stable.
void test_sinLUT_large_angle_wrap() {
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, 0.0f, sinLUT(100.0f * kPi));
    TEST_ASSERT_FLOAT_WITHIN(kSinTolerance, 1.0f, sinLUT(100.0f * kPi + kPi * 0.5f));
}

// 4 — Sweep accuracy: 1024 evenly spaced samples across [0, 2π] must each
// be within tolerance of std::sin. This catches table-build errors and
// interpolation bugs that point tests would miss.
void test_sinLUT_sweep_accuracy() {
    constexpr int kSamples = 1024;
    float maxErr = 0.0f;
    for (int i = 0; i < kSamples; ++i) {
        const float angle = (kTwoPi * static_cast<float>(i)) / static_cast<float>(kSamples);
        const float expected = std::sin(angle);
        const float actual = sinLUT(angle);
        const float err = std::fabs(expected - actual);
        if (err > maxErr) maxErr = err;
    }
    TEST_ASSERT_TRUE_MESSAGE(maxErr < kSinTolerance, "sinLUT max error exceeded 0.01 across 1024 samples");
}

// 5 — Trivial dt: when dt is much smaller than every CFL constraint, one
// substep suffices. Solver does NOT subdivide for free.
void test_cfl_trivial_dt_returns_one() {
    // v_max=1, D=1, c=1, dx=1. Limits: dt_v=1, dt_D=0.5, dt_c=1. dt=1e-4 ≪ 0.5.
    const int n = cflSubstepCount(1.0f, 1.0f, 1.0f, 1e-4f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(1, n);
}

// 6 — Extreme v_max forces clamp at max_steps. When advection demands more
// substeps than the cap, the gate returns max_steps; the caller knows the
// stability bound has been violated for this frame (visual "snap" risk).
void test_cfl_extreme_velocity_clamps_at_max() {
    // dx=1, v_max=1e6 → needed_dt_v = 1e-6. dt=1/120 → ratio = 8333. Cap=16.
    const int n = cflSubstepCount(1e6f, 0.0f, 0.0f, 1.0f / 120.0f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(16, n);
}

// 7 — All zero coefficients: no active solver components → no constraint →
// 1 substep. The solver is presumably idle; do not pay for substeps that
// constrain nothing.
void test_cfl_zero_coefficients_returns_one() {
    const int n = cflSubstepCount(0.0f, 0.0f, 0.0f, 1.0f / 120.0f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(1, n);
}

// 8 — Advection only: needed_dt = dx / |v_max|. dx=1, v_max=4, dt=1/120.
// needed = 0.25. ratio = (1/120)/0.25 ≈ 0.0333 → ceil → 1.
void test_cfl_advection_only() {
    const int n = cflSubstepCount(4.0f, 0.0f, 0.0f, 1.0f / 120.0f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(1, n);

    // Push velocity high enough to need >1 substep but stay under the cap.
    // dx=1, v_max=600, dt=1/120 → needed=1/600, ratio=5 → 5 substeps.
    const int n2 = cflSubstepCount(600.0f, 0.0f, 0.0f, 1.0f / 120.0f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(5, n2);
}

// 9 — Diffusion only: needed_dt = dx² / (2D). dx=1, D=10, dt=1/120.
// needed = 0.05. ratio = (1/120)/0.05 ≈ 0.166 → ceil → 1. Push D up:
// D=1000 → needed = 5e-4, ratio ≈ 16.67 → ceil → 17 → clamped to 16.
void test_cfl_diffusion_only() {
    const int n_low = cflSubstepCount(0.0f, 10.0f, 0.0f, 1.0f / 120.0f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(1, n_low);

    const int n_high = cflSubstepCount(0.0f, 1000.0f, 0.0f, 1.0f / 120.0f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(16, n_high);  // clamped
}

// 10 — Wave only: needed_dt = dx / c. dx=1, c=240 → needed = 1/240, dt=1/120
// → ratio = 2 → 2 substeps. Standard wave-equation scenario.
void test_cfl_wave_only() {
    const int n = cflSubstepCount(0.0f, 0.0f, 240.0f, 1.0f / 120.0f, 1.0f, 16);
    TEST_ASSERT_EQUAL_INT(2, n);
}

// 11 — Result must always be ≥ 1, even on degenerate input. The solver
// expects to run at least once per frame; returning 0 would skip the frame
// entirely.
void test_cfl_minimum_one_substep() {
    // Negative dt should still floor to 1 — defensive against caller bugs.
    const int n = cflSubstepCount(0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 16);
    TEST_ASSERT_TRUE(n >= 1);
}

}  // namespace

void run_math_substrate_tests() {
    RUN_TEST(test_sinLUT_cardinal_points);
    RUN_TEST(test_sinLUT_negative_angles);
    RUN_TEST(test_sinLUT_large_angle_wrap);
    RUN_TEST(test_sinLUT_sweep_accuracy);
    RUN_TEST(test_cfl_trivial_dt_returns_one);
    RUN_TEST(test_cfl_extreme_velocity_clamps_at_max);
    RUN_TEST(test_cfl_zero_coefficients_returns_one);
    RUN_TEST(test_cfl_advection_only);
    RUN_TEST(test_cfl_diffusion_only);
    RUN_TEST(test_cfl_wave_only);
    RUN_TEST(test_cfl_minimum_one_substep);
}
