// Phase 1 Move 1.3 — FramebufferLPF (INF-02) substrate test
//
// Per Topology_Reconciliation §5: FramebufferLPF is the second-highest
// leverage hub in the K1 visual pipeline. It is the dt-correct global image
// LPF lineage from Emotiscope (apply_image_lpf in gpu_core.h:87) — a per-LED
// elementwise EMA with a softness-driven cutoff.
//
// Contract being tested:
//   - dt-correct via 1 - exp(-2π · cutoffHz · dt)
//   - softness 0 → cutoff 15 Hz (sharp); softness 1 → cutoff 0.5 Hz (fluid)
//   - setCutoffHz overrides the softness curve (per-layer τ exposure)
//   - first-call-after-reset behaves as if prevFrame was zero (initial state)
//   - convergence to a constant target frame within 1% after 100 iterations
//   - cutoff = 0 freezes prev frame; very high cutoff yields the new frame
//     verbatim
//   - reset() zeroes prev-frame state
//
// All operations must be heap-free and run inside the 2 ms render budget.
// British English spelling in comments.

#include <unity.h>
#include <cmath>
#include <cstdint>

#include "effects/persistence/FramebufferLPF.h"

using lightwaveos::effects::persistence::FramebufferLPF;
using lightwaveos::effects::persistence::kFramebufferLen;

namespace {

constexpr float kEps = 1e-4f;

// Helper: fill a CRGB buffer with a uniform colour.
void fillBuffer(CRGB* buf, int n, uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < n; ++i) {
        buf[i].r = r;
        buf[i].g = g;
        buf[i].b = b;
    }
}

// 1 — Softness = 0 maps to cutoff = 15 Hz per the ES curve
//     0.5 + (1 - sqrt(0)) · 14.5 = 0.5 + 14.5 = 15.0
void test_softness_zero_yields_fifteen_hz_cutoff() {
    FramebufferLPF lpf;
    lpf.setSoftness(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 15.0f, lpf.cutoffHz());
}

// 2 — Softness = 1 maps to cutoff = 0.5 Hz per the ES curve
//     0.5 + (1 - sqrt(1)) · 14.5 = 0.5 + 0 = 0.5
void test_softness_one_yields_half_hz_cutoff() {
    FramebufferLPF lpf;
    lpf.setSoftness(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, lpf.cutoffHz());
}

// 3 — Softness clamps below 0 and above 1 (defensive boundary check).
void test_softness_clamps_to_unit_range() {
    FramebufferLPF lpf;
    lpf.setSoftness(-1.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 15.0f, lpf.cutoffHz());
    lpf.setSoftness(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, lpf.cutoffHz());
}

// 4 — setCutoffHz overrides the softness curve. This is the per-layer τ
//     exposure mandated by Topology_Reconciliation §3 C-2.
void test_setCutoffHz_overrides_softness() {
    FramebufferLPF lpf;
    lpf.setSoftness(0.5f);          // would yield ~5.24 Hz
    lpf.setCutoffHz(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, lpf.cutoffHz());
}

// 5 — setCutoffHz clamps negative values to zero (no negative cutoff).
void test_setCutoffHz_clamps_negative() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(-3.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, lpf.cutoffHz());
}

// 6 — First call after construction: prev frame is zero, alpha is in (0, 1).
//     A new constant frame is darkened by (1 - alpha) in one application.
//     With cutoff = 15 Hz and dt = 1/120 s:
//       alpha = 1 - exp(-2π · 15 · 1/120) = 1 - exp(-π/4) ≈ 0.5441
//     200 · 0.5441 ≈ 108. We allow ±2 to absorb float→uint8 truncation.
void test_first_apply_darkens_new_frame_toward_alpha_blend() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(15.0f);
    CRGB buf[kFramebufferLen];
    fillBuffer(buf, kFramebufferLen, 200, 200, 200);
    lpf.apply(buf, 1.0f / 120.0f);
    // alpha ≈ 0.5441 → expected ≈ 108, allow tolerance for truncation.
    TEST_ASSERT_INT_WITHIN(2, 108, buf[0].r);
    TEST_ASSERT_INT_WITHIN(2, 108, buf[160].g);
    TEST_ASSERT_INT_WITHIN(2, 108, buf[319].b);
}

// 7 — Cutoff = 0 freezes the prev frame: alpha = 0, output equals prev.
//     Starting from a reset (prev = 0), output stays at 0 regardless of input.
void test_cutoff_zero_freezes_prev_frame() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(0.0f);
    CRGB buf[kFramebufferLen];
    fillBuffer(buf, kFramebufferLen, 255, 128, 64);
    lpf.apply(buf, 1.0f / 60.0f);
    // prev was zero, alpha = 0, so output = prev = 0.
    TEST_ASSERT_EQUAL_UINT8(0, buf[0].r);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0].g);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0].b);
    TEST_ASSERT_EQUAL_UINT8(0, buf[200].r);
}

// 8 — Very high cutoff (or large dt): alpha ≈ 1, output ≈ new frame verbatim.
void test_high_cutoff_passes_new_frame_verbatim() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(1000.0f);  // alpha = 1 - exp(-2π · 1000 · 1/60) ≈ 1
    CRGB buf[kFramebufferLen];
    fillBuffer(buf, kFramebufferLen, 200, 100, 50);
    lpf.apply(buf, 1.0f / 60.0f);
    // Tolerance ±2 for float→uint8 truncation.
    TEST_ASSERT_INT_WITHIN(2, 200, buf[0].r);
    TEST_ASSERT_INT_WITHIN(2, 100, buf[0].g);
    TEST_ASSERT_INT_WITHIN(2, 50,  buf[0].b);
}

// 9 — Convergence: with a constant input frame, the filter approaches the
//     input value within 1% after 100 iterations at a typical cutoff.
//     5 τ = 5 / (2π · 8) ≈ 0.0995 s ≈ 12 frames at 120 fps; 100 iterations
//     is far past convergence.
void test_convergence_to_constant_input() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(8.0f);
    CRGB buf[kFramebufferLen];
    const uint8_t target = 180;
    for (int i = 0; i < 100; ++i) {
        fillBuffer(buf, kFramebufferLen, target, target, target);
        lpf.apply(buf, 1.0f / 120.0f);
    }
    // After 100 iterations residual error should be << 1%.
    TEST_ASSERT_INT_WITHIN(2, target, buf[0].r);
    TEST_ASSERT_INT_WITHIN(2, target, buf[160].g);
    TEST_ASSERT_INT_WITHIN(2, target, buf[319].b);
}

// 10 — reset() zeroes the prev frame so a subsequent apply behaves as if
//      the LPF were freshly constructed.
void test_reset_zeroes_prev_frame() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(8.0f);
    CRGB buf[kFramebufferLen];
    // Drive the filter to steady state at red = 200.
    for (int i = 0; i < 100; ++i) {
        fillBuffer(buf, kFramebufferLen, 200, 0, 0);
        lpf.apply(buf, 1.0f / 120.0f);
    }
    // After reset(), prev should be zero — feeding red=0 produces black.
    lpf.reset();
    fillBuffer(buf, kFramebufferLen, 0, 0, 0);
    lpf.apply(buf, 1.0f / 120.0f);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0].r);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0].g);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0].b);
}

// 11 — Negative dt is clamped to zero (defensive). Output equals prev frame.
void test_negative_dt_holds_prev_frame() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(15.0f);
    CRGB buf[kFramebufferLen];
    // First apply at sane dt to load prev.
    fillBuffer(buf, kFramebufferLen, 100, 50, 25);
    lpf.apply(buf, 1.0f / 120.0f);
    const uint8_t prevR = buf[0].r;
    const uint8_t prevG = buf[0].g;
    const uint8_t prevB = buf[0].b;
    // Now feed garbage with dt < 0 — output should equal the previously
    // stored value, not the new garbage.
    fillBuffer(buf, kFramebufferLen, 255, 255, 255);
    lpf.apply(buf, -0.5f);
    TEST_ASSERT_EQUAL_UINT8(prevR, buf[0].r);
    TEST_ASSERT_EQUAL_UINT8(prevG, buf[0].g);
    TEST_ASSERT_EQUAL_UINT8(prevB, buf[0].b);
}

// 12 — apply(nullptr, dt) is a no-op (no crash). Sanity boundary.
void test_apply_null_pointer_is_noop() {
    FramebufferLPF lpf;
    lpf.setCutoffHz(8.0f);
    lpf.apply(nullptr, 1.0f / 120.0f);
    // If we reach here without segfault the contract holds.
    TEST_ASSERT_TRUE(true);
}

}  // namespace

void run_framebuffer_lpf_tests() {
    RUN_TEST(test_softness_zero_yields_fifteen_hz_cutoff);
    RUN_TEST(test_softness_one_yields_half_hz_cutoff);
    RUN_TEST(test_softness_clamps_to_unit_range);
    RUN_TEST(test_setCutoffHz_overrides_softness);
    RUN_TEST(test_setCutoffHz_clamps_negative);
    RUN_TEST(test_first_apply_darkens_new_frame_toward_alpha_blend);
    RUN_TEST(test_cutoff_zero_freezes_prev_frame);
    RUN_TEST(test_high_cutoff_passes_new_frame_verbatim);
    RUN_TEST(test_convergence_to_constant_input);
    RUN_TEST(test_reset_zeroes_prev_frame);
    RUN_TEST(test_negative_dt_holds_prev_frame);
    RUN_TEST(test_apply_null_pointer_is_noop);
}
