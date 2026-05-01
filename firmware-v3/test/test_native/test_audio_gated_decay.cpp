// Phase 4 Move 4.2 PER-18 — AudioGatedDecay substrate test
//
// Silence-aware decay helper: tau varies with current audio RMS so that
// silent passages clear sparkle quickly and loud passages preserve long
// trails. Substrate for V1.0 F3 Liquid Stillness ambient state.
//
// Coverage targets:
//   - currentTau at silentRms / loudRms / mid-range / out-of-range
//   - applyDecay fast at silence (large per-frame reduction)
//   - applyDecay slow at loud (small per-frame reduction)
//   - applyDecayArray same characteristics for scalar buffers
//   - nullptr / zero-length safety
//
// Constraints: no heap, dt-correct via exp(), British English spelling in
// comments and log strings.

#include <unity.h>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "effects/persistence/AudioGatedDecay.h"

using lightwaveos::effects::persistence::AudioGatedDecay;

namespace {

constexpr float kEps = 1e-5f;

// ─── currentTau ────────────────────────────────────────────────────────────

// 1 — At silentRms exactly, currentTau returns silentTau.
void test_currentTau_at_silent_rms_returns_silent_tau() {
    AudioGatedDecay g;
    g.setRmsRange(0.05f, 0.5f);
    g.setTauRange(0.1f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.1f, g.currentTau(0.05f));
}

// 2 — At loudRms exactly, currentTau returns loudTau.
void test_currentTau_at_loud_rms_returns_loud_tau() {
    AudioGatedDecay g;
    g.setRmsRange(0.05f, 0.5f);
    g.setTauRange(0.1f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, g.currentTau(0.5f));
}

// 3 — Midpoint RMS yields the linear midpoint of the tau range.
void test_currentTau_midpoint_is_linear_lerp() {
    AudioGatedDecay g;
    g.setRmsRange(0.0f, 1.0f);
    g.setTauRange(0.0f, 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, g.currentTau(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, g.currentTau(0.25f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 3.0f, g.currentTau(0.75f));
}

// 4 — Out-of-range RMS clamps to the corresponding tau endpoint.
//     Negative RMS (upstream bug) saturates at silentTau; RMS > loudRms
//     saturates at loudTau.
void test_currentTau_out_of_range_clamps() {
    AudioGatedDecay g;
    g.setRmsRange(0.05f, 0.5f);
    g.setTauRange(0.1f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.1f, g.currentTau(-0.5f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.1f, g.currentTau(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, g.currentTau(1.5f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, g.currentTau(99.0f));
}

// 5 — Defaults match the documented sensible values for ESV11 ControlBus.rms.
void test_currentTau_defaults_match_spec() {
    AudioGatedDecay g;  // No setters called.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.1f, g.currentTau(0.05f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, g.currentTau(0.5f));
}

// ─── applyDecay (CRGB) ─────────────────────────────────────────────────────

// 6 — At silentRms, decay is fast: tau = 0.1 s, dt = 8 ms (ESV11 frame),
//     alpha ≈ 1 - exp(-0.08) ≈ 0.0769; one frame should reduce a bright
//     channel by roughly 7-8 percent.
void test_applyDecay_at_silence_is_fast() {
    AudioGatedDecay g;  // Defaults.
    CRGB pixels[4] = {CRGB(255, 255, 255),
                      CRGB(200, 100, 50),
                      CRGB(0, 128, 64),
                      CRGB(10, 10, 10)};
    const float dt = 1.0f / 125.0f;
    g.applyDecay(pixels, 4, 0.05f, dt);
    // Expected keep factor ≈ 0.9231 → 255 * 0.9231 ≈ 235.
    TEST_ASSERT_TRUE(pixels[0].r < 240);
    TEST_ASSERT_TRUE(pixels[0].r > 230);
    TEST_ASSERT_TRUE(pixels[1].r < 195);
    TEST_ASSERT_TRUE(pixels[1].b < 50);
}

// 7 — Repeated decay at silentRms drives values toward zero within a few
//     hundred milliseconds (5 tau ≈ 500 ms ≈ 62 frames at 125 Hz).
void test_applyDecay_at_silence_converges_to_zero() {
    AudioGatedDecay g;
    CRGB pixels[2] = {CRGB(255, 255, 255), CRGB(128, 64, 32)};
    const float dt = 1.0f / 125.0f;
    for (int i = 0; i < 80; ++i) {
        g.applyDecay(pixels, 2, 0.05f, dt);
    }
    // After ~640 ms (>6 tau at silentTau=0.1 s) channels should be near zero.
    TEST_ASSERT_TRUE(pixels[0].r < 5);
    TEST_ASSERT_TRUE(pixels[0].g < 5);
    TEST_ASSERT_TRUE(pixels[1].r < 5);
}

// 8 — At loudRms, decay is slow: tau = 2.0 s, dt = 8 ms,
//     alpha ≈ 1 - exp(-0.004) ≈ 0.00399; one frame should barely change a
//     bright channel (less than 1 LSB for value 255).
void test_applyDecay_at_loud_is_slow() {
    AudioGatedDecay g;  // Defaults.
    CRGB pixels[2] = {CRGB(255, 255, 255), CRGB(200, 100, 50)};
    const float dt = 1.0f / 125.0f;
    g.applyDecay(pixels, 2, 0.5f, dt);
    // Expected keep factor ≈ 0.99601 → 255 * 0.99601 ≈ 254.
    TEST_ASSERT_TRUE(pixels[0].r >= 253);
    TEST_ASSERT_TRUE(pixels[1].r >= 198);
}

// 9 — nullptr / zero-length safety: must not crash.
void test_applyDecay_nullptr_safe() {
    AudioGatedDecay g;
    g.applyDecay(nullptr, 4, 0.1f, 0.008f);
    CRGB pixels[2] = {CRGB(255, 255, 255), CRGB(128, 64, 32)};
    g.applyDecay(pixels, 0, 0.1f, 0.008f);
    // Untouched after zero-length call.
    TEST_ASSERT_EQUAL_UINT8(255, pixels[0].r);
    TEST_ASSERT_EQUAL_UINT8(128, pixels[1].r);
    TEST_PASS();
}

// ─── applyDecayArray (scalar) ──────────────────────────────────────────────

// 10 — Scalar variant exhibits the same fast/slow asymmetry.
void test_applyDecayArray_silence_vs_loud() {
    AudioGatedDecay g;
    float arrSilent[3] = {1.0f, 1.0f, 1.0f};
    float arrLoud[3]   = {1.0f, 1.0f, 1.0f};
    const float dt = 1.0f / 125.0f;
    g.applyDecayArray(arrSilent, 3, 0.05f, dt);
    g.applyDecayArray(arrLoud,   3, 0.5f,  dt);
    // Silence decays substantially; loud barely moves.
    TEST_ASSERT_TRUE(arrSilent[0] < 0.95f);
    TEST_ASSERT_TRUE(arrSilent[0] > 0.90f);
    TEST_ASSERT_TRUE(arrLoud[0]   > 0.99f);
    // Loud must always decay slower than silence at the same dt.
    TEST_ASSERT_TRUE(arrLoud[0] > arrSilent[0]);
}

// 11 — Scalar nullptr / zero-length safety.
void test_applyDecayArray_nullptr_safe() {
    AudioGatedDecay g;
    g.applyDecayArray(nullptr, 4, 0.1f, 0.008f);
    float arr[2] = {1.0f, 2.0f};
    g.applyDecayArray(arr, 0, 0.1f, 0.008f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, arr[0]);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 2.0f, arr[1]);
    TEST_PASS();
}

// 12 — Degenerate RMS range (silentRms == loudRms) collapses to silentTau
//      and does not divide by zero.
void test_currentTau_degenerate_range_no_divide_by_zero() {
    AudioGatedDecay g;
    g.setRmsRange(0.3f, 0.3f);
    g.setTauRange(0.5f, 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, g.currentTau(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, g.currentTau(0.3f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, g.currentTau(1.0f));
}

}  // namespace

void run_audio_gated_decay_tests() {
    RUN_TEST(test_currentTau_at_silent_rms_returns_silent_tau);
    RUN_TEST(test_currentTau_at_loud_rms_returns_loud_tau);
    RUN_TEST(test_currentTau_midpoint_is_linear_lerp);
    RUN_TEST(test_currentTau_out_of_range_clamps);
    RUN_TEST(test_currentTau_defaults_match_spec);
    RUN_TEST(test_applyDecay_at_silence_is_fast);
    RUN_TEST(test_applyDecay_at_silence_converges_to_zero);
    RUN_TEST(test_applyDecay_at_loud_is_slow);
    RUN_TEST(test_applyDecay_nullptr_safe);
    RUN_TEST(test_applyDecayArray_silence_vs_loud);
    RUN_TEST(test_applyDecayArray_nullptr_safe);
    RUN_TEST(test_currentTau_degenerate_range_no_divide_by_zero);
}
