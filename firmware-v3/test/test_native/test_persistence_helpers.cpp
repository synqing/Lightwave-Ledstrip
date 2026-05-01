// Phase 1 Move 1.1 — PersistenceHelpers substrate test
//
// Per Topology_Reconciliation §5: this is canonical Phase 1 substrate. The
// existing dtDecay() pattern in ChromaUtils.h is hoisted into a central
// PersistenceHelpers.h library and extended with 3-channel + array variants.
// Future Phase 6 PDE work (heatStep1D, velocityAniso1D) will depend on this
// minimum subset.
//
// All helpers are inline static and must:
//   - perform NO heap allocation (called transitively from render path)
//   - be dt-correct (alpha computed via exp(-dt/tau), not per-frame constant)
//   - be cheap (under ~1 µs per call typical) to honour the 2.0 ms ceiling
//   - work native + ESP32-S3 (no platform-specific intrinsics)
//
// Centre origin and British English are not directly applicable to a maths
// library, but spelling in comments/docs/log strings remains British.

#include <unity.h>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "effects/PersistenceHelpers.h"

using lightwaveos::effects::persistence::dtDecay;
using lightwaveos::effects::persistence::dtDecay3;
using lightwaveos::effects::persistence::emaArrayDt;
using lightwaveos::effects::persistence::crossBlendArray;

namespace {

constexpr float kEps = 1e-5f;

// ─── dtDecay (scalar) ──────────────────────────────────────────────────────

// 1 — rate=1.0 is the identity: no decay, regardless of dt.
void test_dtDecay_rate_one_is_identity() {
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, dtDecay(1.0f, 1.0f, 0.016667f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, dtDecay(0.5f, 1.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 42.0f, dtDecay(42.0f, 1.0f, 0.0f));
}

// 2 — At dt = 1/60 s with rate=0.5, exactly one 60 fps frame elapsed → halve.
//     This is the contract the original ChromaUtils.h dtDecay was written for.
void test_dtDecay_rate_half_one_frame_halves() {
    const float result = dtDecay(1.0f, 0.5f, 1.0f / 60.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, result);
}

// 3 — rate=0 collapses any input to zero (one frame is enough).
void test_dtDecay_rate_zero_returns_zero() {
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, dtDecay(123.45f, 0.0f, 1.0f / 60.0f));
}

// ─── dtDecay3 (CRGB in-place) ──────────────────────────────────────────────

// 4 — Per-channel application: each colour byte decays independently with the
//     same rate, and the CRGB struct stays the same shape (no resizing).
void test_dtDecay3_applies_per_channel() {
    CRGB c(200, 100, 50);
    // rate=0.5, dt=1/60 → exact halving per channel; rounding floors uint8.
    dtDecay3(c, 0.5f, 1.0f / 60.0f);
    TEST_ASSERT_EQUAL_UINT8(100, c.r);
    TEST_ASSERT_EQUAL_UINT8(50,  c.g);
    TEST_ASSERT_EQUAL_UINT8(25,  c.b);
}

// 5 — rate=1.0 leaves a CRGB untouched.
void test_dtDecay3_rate_one_preserves_colour() {
    CRGB c(123, 45, 67);
    dtDecay3(c, 1.0f, 0.05f);
    TEST_ASSERT_EQUAL_UINT8(123, c.r);
    TEST_ASSERT_EQUAL_UINT8(45,  c.g);
    TEST_ASSERT_EQUAL_UINT8(67,  c.b);
}

// ─── emaArrayDt (dt-correct array EMA) ─────────────────────────────────────

// 6 — alpha → 0 (tau very large compared to dt) leaves the array unchanged.
//     This is the "infinite memory" edge case.
void test_emaArrayDt_tau_infinite_leaves_unchanged() {
    float arr[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    // tau = 1e9 s, dt = 0.016 s → alpha ≈ 1.6e-11, effectively zero.
    emaArrayDt(arr, 4, 99.0f, 1.0e9f, 0.016f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, arr[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f, arr[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 3.0f, arr[2]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 4.0f, arr[3]);
}

// 7 — alpha → 1 (tau very small compared to dt) replaces the array with src.
void test_emaArrayDt_tau_zero_replaces_with_source() {
    float arr[3] = {1.0f, 2.0f, 3.0f};
    // tau = 1e-9 s, dt = 0.016 s → alpha ≈ 1.0 (exp(-1.6e7) underflows).
    emaArrayDt(arr, 3, 7.5f, 1.0e-9f, 0.016f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 7.5f, arr[0]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 7.5f, arr[1]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 7.5f, arr[2]);
}

// 8 — Convergence: with constant src, the array approaches src after many
//     time-constants. After 5 tau, expect <1% residual error.
void test_emaArrayDt_converges_to_source() {
    float arr[2] = {0.0f, 100.0f};
    const float src = 50.0f;
    const float tau = 0.1f;          // 100 ms
    const float dt  = 1.0f / 125.0f; // ESV11 frame rate, 8 ms
    // 5 tau = 500 ms = 62.5 frames; iterate 65 to be safe.
    for (int i = 0; i < 65; ++i) {
        emaArrayDt(arr, 2, src, tau, dt);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.5f, src, arr[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, src, arr[1]);
}

// ─── crossBlendArray (linear interpolation) ────────────────────────────────

// 9 — alpha=0 yields a verbatim.
void test_crossBlendArray_alpha_zero_returns_a() {
    const float a[3] = {1.0f, 2.0f, 3.0f};
    const float b[3] = {10.0f, 20.0f, 30.0f};
    float dest[3] = {0.0f, 0.0f, 0.0f};
    crossBlendArray(dest, a, b, 3, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, dest[0]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, dest[1]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 3.0f, dest[2]);
}

// 10 — alpha=1 yields b verbatim.
void test_crossBlendArray_alpha_one_returns_b() {
    const float a[3] = {1.0f, 2.0f, 3.0f};
    const float b[3] = {10.0f, 20.0f, 30.0f};
    float dest[3] = {0.0f, 0.0f, 0.0f};
    crossBlendArray(dest, a, b, 3, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 10.0f, dest[0]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 20.0f, dest[1]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 30.0f, dest[2]);
}

// 11 — alpha=0.5 is the elementwise mean.
void test_crossBlendArray_alpha_half_returns_mean() {
    const float a[3] = {0.0f, 4.0f, 10.0f};
    const float b[3] = {2.0f, 8.0f, 20.0f};
    float dest[3] = {0.0f, 0.0f, 0.0f};
    crossBlendArray(dest, a, b, 3, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f,  dest[0]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 6.0f,  dest[1]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 15.0f, dest[2]);
}

}  // namespace

void run_persistence_helpers_tests() {
    RUN_TEST(test_dtDecay_rate_one_is_identity);
    RUN_TEST(test_dtDecay_rate_half_one_frame_halves);
    RUN_TEST(test_dtDecay_rate_zero_returns_zero);
    RUN_TEST(test_dtDecay3_applies_per_channel);
    RUN_TEST(test_dtDecay3_rate_one_preserves_colour);
    RUN_TEST(test_emaArrayDt_tau_infinite_leaves_unchanged);
    RUN_TEST(test_emaArrayDt_tau_zero_replaces_with_source);
    RUN_TEST(test_emaArrayDt_converges_to_source);
    RUN_TEST(test_crossBlendArray_alpha_zero_returns_a);
    RUN_TEST(test_crossBlendArray_alpha_one_returns_b);
    RUN_TEST(test_crossBlendArray_alpha_half_returns_mean);
}
