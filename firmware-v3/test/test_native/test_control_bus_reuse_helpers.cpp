// Phase 1 Move 1.5 — ControlBusReuseHelpers (Phase 1: helper substrate only).
//
// Tests the read-only header helpers that unify the five categories of
// ControlBus consumption used by effects:
//   1. STM temporal modulation (gated by stmReady)
//   2. Saliency *NoveltySmooth passthrough
//   3. Motion-semantic clamps (timing_jitter, syncopation, contour dir)
//   4. Onset band-split flux + trigger booleans
//   5. Audio confidence + silence + gated brightness
//
// Helpers are tested in isolation against synthetic ControlBusFrame
// instances — no effects, no renderer, no FastLED. All helpers are
// header-only inline; this binary verifies semantic contract only.

#include <unity.h>
#include <cmath>
#include <cstring>

#include "../../src/audio/contracts/ControlBus.h"
#include "../../src/effects/audio/ControlBusReuseHelpers.h"

using lightwaveos::audio::ControlBusFrame;
namespace reuse = lightwaveos::effects::audio::reuse;

namespace {

constexpr float kEps = 1e-5f;

// ── Category 1: STM temporal modulation ───────────────────────────────────

void test_stmEnergy_is_zero_when_stmReady_false() {
    ControlBusFrame f{};
    f.stmReady = false;
    f.stmTemporalEnergy = 0.85f;  // Should be ignored.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::stmEnergy(f));
}

void test_stmEnergy_passthrough_when_ready() {
    ControlBusFrame f{};
    f.stmReady = true;
    f.stmTemporalEnergy = 0.7f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.7f, reuse::stmEnergy(f));
}

void test_stmBand_out_of_range_returns_zero() {
    ControlBusFrame f{};
    f.stmReady = true;
    for (uint8_t i = 0; i < ControlBusFrame::STM_MEL_BANDS; ++i) {
        f.stmTemporal[i] = 0.5f;
    }
    // Out-of-range mel index must yield 0.0 — no out-of-bounds read.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::stmBand(f, 255));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f,
                             reuse::stmBand(f, ControlBusFrame::STM_MEL_BANDS));
    // In-range still returns the stored value.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, reuse::stmBand(f, 0));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f,
                             reuse::stmBand(f, ControlBusFrame::STM_MEL_BANDS - 1));
}

void test_stmBand_returns_zero_when_not_ready() {
    ControlBusFrame f{};
    f.stmReady = false;
    f.stmTemporal[3] = 0.9f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::stmBand(f, 3));
}

// ── Category 2: Saliency ──────────────────────────────────────────────────

void test_saliency_helpers_passthrough_correctly() {
    ControlBusFrame f{};
    f.saliency.harmonicNoveltySmooth = 0.10f;
    f.saliency.rhythmicNoveltySmooth = 0.20f;
    f.saliency.timbralNoveltySmooth  = 0.30f;
    f.saliency.dynamicNoveltySmooth  = 0.40f;

    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.10f, reuse::harmonicNovelty(f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.20f, reuse::rhythmicNovelty(f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.30f, reuse::timbralNovelty(f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.40f, reuse::dynamicNovelty(f));
}

void test_dominantNovelty_returns_max_of_four() {
    ControlBusFrame f{};
    f.saliency.harmonicNoveltySmooth = 0.10f;
    f.saliency.rhythmicNoveltySmooth = 0.55f;  // dominant
    f.saliency.timbralNoveltySmooth  = 0.30f;
    f.saliency.dynamicNoveltySmooth  = 0.40f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.55f, reuse::dominantNovelty(f));

    // Swap dominance to dynamic.
    f.saliency.dynamicNoveltySmooth = 0.99f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.99f, reuse::dominantNovelty(f));

    // All zero → zero.
    ControlBusFrame z{};
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::dominantNovelty(z));
}

// ── Category 3: Motion-semantic clamps ───────────────────────────────────

void test_motion_semantic_clamps_to_documented_ranges() {
    ControlBusFrame f{};

    // timing_jitter is documented [0,1]; clamp out-of-range positives.
    f.timing_jitter = 2.5f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, reuse::timingJitter(f));
    f.timing_jitter = -0.3f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::timingJitter(f));
    f.timing_jitter = 0.42f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.42f, reuse::timingJitter(f));

    // syncopation_level documented [0,1].
    f.syncopation_level = 1.5f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, reuse::syncopation(f));
    f.syncopation_level = -0.5f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::syncopation(f));

    // pitch_contour_dir documented [-1,+1].
    f.pitch_contour_dir = -2.0f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, -1.0f, reuse::contourDir(f));
    f.pitch_contour_dir = 1.5f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, reuse::contourDir(f));
    f.pitch_contour_dir = 0.25f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.25f, reuse::contourDir(f));
    f.pitch_contour_dir = -0.7f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, -0.7f, reuse::contourDir(f));
}

// ── Category 4: Onset band-split ─────────────────────────────────────────

void test_onset_anyTrigger_returns_correct_flag() {
    ControlBusFrame f{};

    // All false → false.
    f.kickTrigger = false; f.snareTrigger = false; f.hihatTrigger = false;
    TEST_ASSERT_FALSE(reuse::anyOnsetTrigger(f));

    // Only kick → true.
    f.kickTrigger = true;
    TEST_ASSERT_TRUE(reuse::anyOnsetTrigger(f));
    f.kickTrigger = false;

    // Only snare → true.
    f.snareTrigger = true;
    TEST_ASSERT_TRUE(reuse::anyOnsetTrigger(f));
    f.snareTrigger = false;

    // Only hihat → true.
    f.hihatTrigger = true;
    TEST_ASSERT_TRUE(reuse::anyOnsetTrigger(f));

    // All three → true.
    f.kickTrigger = true; f.snareTrigger = true;
    TEST_ASSERT_TRUE(reuse::anyOnsetTrigger(f));
}

void test_onset_flux_helpers_passthrough() {
    ControlBusFrame f{};
    f.onsetBassFlux = 1.25f;   // Flux is unbounded — no clamp expected.
    f.onsetMidFlux  = 0.5f;
    f.onsetHighFlux = 3.7f;

    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.25f, reuse::bassFlux(f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f,  reuse::midFlux(f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 3.7f,  reuse::highFlux(f));
}

// ── Category 5: Audio confidence + silence ──────────────────────────────

void test_audioConfidence_clamps_negative_and_over_one() {
    ControlBusFrame f{};

    f.audioConfidence = -0.5f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::audioConfidence(f));

    f.audioConfidence = 1.5f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, reuse::audioConfidence(f));

    f.audioConfidence = 0.6f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.6f, reuse::audioConfidence(f));
}

void test_silentScale_clamps_to_unit_interval() {
    ControlBusFrame f{};

    f.silentScale = -1.0f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::silentScale(f));

    f.silentScale = 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, reuse::silentScale(f));

    f.silentScale = 0.33f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.33f, reuse::silentScale(f));
}

void test_isSilent_passthrough() {
    ControlBusFrame f{};
    f.isSilent = false;
    TEST_ASSERT_FALSE(reuse::isSilent(f));
    f.isSilent = true;
    TEST_ASSERT_TRUE(reuse::isSilent(f));
}

void test_gatedBrightness_multiplies_correctly() {
    ControlBusFrame f{};
    f.silentScale     = 0.5f;
    f.audioConfidence = 0.5f;

    // 1.0 × 0.5 × 0.5 = 0.25.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.25f, reuse::gatedBrightness(f, 1.0f));

    // 0.8 × 0.5 × 0.5 = 0.20.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.20f, reuse::gatedBrightness(f, 0.8f));

    // Both at 1.0 → identity.
    f.silentScale     = 1.0f;
    f.audioConfidence = 1.0f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.6f, reuse::gatedBrightness(f, 0.6f));

    // Either at 0.0 → zero.
    f.silentScale     = 0.0f;
    f.audioConfidence = 1.0f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::gatedBrightness(f, 1.0f));
    f.silentScale     = 1.0f;
    f.audioConfidence = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::gatedBrightness(f, 1.0f));

    // Out-of-range bus values are clamped before multiplication.
    f.silentScale     = 1.5f;   // clamps to 1.0
    f.audioConfidence = -0.5f;  // clamps to 0.0
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::gatedBrightness(f, 1.0f));
}

// ── Internal clamp utilities ────────────────────────────────────────────

void test_clamp01_at_boundaries() {
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::clamp01(-0.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::clamp01(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, reuse::clamp01(-0.001f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, reuse::clamp01(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, reuse::clamp01(1.001f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, reuse::clamp01(0.5f));
}

void test_clampPm1_at_boundaries() {
    TEST_ASSERT_FLOAT_WITHIN(kEps, -1.0f, reuse::clampPm1(-1.5f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, -1.0f, reuse::clampPm1(-1.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps,  0.0f, reuse::clampPm1(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps,  1.0f, reuse::clampPm1(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(kEps,  1.0f, reuse::clampPm1(1.5f));
    TEST_ASSERT_FLOAT_WITHIN(kEps, -0.5f, reuse::clampPm1(-0.5f));
    TEST_ASSERT_FLOAT_WITHIN(kEps,  0.5f, reuse::clampPm1(0.5f));
}

}  // namespace

void run_control_bus_reuse_helpers_tests() {
    // Category 1: STM
    RUN_TEST(test_stmEnergy_is_zero_when_stmReady_false);
    RUN_TEST(test_stmEnergy_passthrough_when_ready);
    RUN_TEST(test_stmBand_out_of_range_returns_zero);
    RUN_TEST(test_stmBand_returns_zero_when_not_ready);

    // Category 2: Saliency
    RUN_TEST(test_saliency_helpers_passthrough_correctly);
    RUN_TEST(test_dominantNovelty_returns_max_of_four);

    // Category 3: Motion-semantic clamps
    RUN_TEST(test_motion_semantic_clamps_to_documented_ranges);

    // Category 4: Onset band-split
    RUN_TEST(test_onset_anyTrigger_returns_correct_flag);
    RUN_TEST(test_onset_flux_helpers_passthrough);

    // Category 5: Audio confidence + silence
    RUN_TEST(test_audioConfidence_clamps_negative_and_over_one);
    RUN_TEST(test_silentScale_clamps_to_unit_interval);
    RUN_TEST(test_isSilent_passthrough);
    RUN_TEST(test_gatedBrightness_multiplies_correctly);

    // Internal utilities
    RUN_TEST(test_clamp01_at_boundaries);
    RUN_TEST(test_clampPm1_at_boundaries);
}
