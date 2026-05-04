// Phase 1 Move 1.7 — PerceptualJND constants.
//
// Captures the Captain-observed K1 LGP brightness floor from 2026-05-05:
// the brightness-floor harness first became visible at test level 4, 8.0%
// perceptual, in both scaler and direct PWM phases.

#include <unity.h>

#include "effects/PerceptualJND.h"

using lightwaveos::effects::perceptual::isBelowK1LgpJnd;
using lightwaveos::effects::perceptual::kFramebufferLpfMaximumTauSeconds;
using lightwaveos::effects::perceptual::kFramebufferLpfMinimumCutoffHz;
using lightwaveos::effects::perceptual::kK1LgpMinimumVisibleChannelDelta;
using lightwaveos::effects::perceptual::kK1LgpPerceptualJndFloor;
using lightwaveos::effects::perceptual::kK1LgpPerceptualJndFloorPercent;
using lightwaveos::effects::perceptual::perceptualPercentToByte;

namespace {

void test_k1_lgp_floor_matches_captain_observation() {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.08f, kK1LgpPerceptualJndFloor);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 8.0f, kK1LgpPerceptualJndFloorPercent);
}

void test_eight_percent_maps_to_first_visible_byte() {
    TEST_ASSERT_EQUAL_UINT8(kK1LgpMinimumVisibleChannelDelta,
                            perceptualPercentToByte(kK1LgpPerceptualJndFloorPercent));
}

void test_lower_steps_remain_below_visible_byte() {
    TEST_ASSERT_EQUAL_UINT8(0, perceptualPercentToByte(1.0f));
    TEST_ASSERT_EQUAL_UINT8(0, perceptualPercentToByte(3.0f));
    TEST_ASSERT_EQUAL_UINT8(0, perceptualPercentToByte(5.0f));
}

void test_jnd_predicate_splits_below_and_at_floor() {
    TEST_ASSERT_TRUE(isBelowK1LgpJnd(0.079f));
    TEST_ASSERT_FALSE(isBelowK1LgpJnd(0.080f));
    TEST_ASSERT_FALSE(isBelowK1LgpJnd(-0.080f));
}

void test_framebuffer_lpf_bound_keeps_es_lower_cutoff() {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, kFramebufferLpfMinimumCutoffHz);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.318f, kFramebufferLpfMaximumTauSeconds);
}

}  // namespace

void run_perceptual_jnd_tests() {
    RUN_TEST(test_k1_lgp_floor_matches_captain_observation);
    RUN_TEST(test_eight_percent_maps_to_first_visible_byte);
    RUN_TEST(test_lower_steps_remain_below_visible_byte);
    RUN_TEST(test_jnd_predicate_splits_below_and_at_floor);
    RUN_TEST(test_framebuffer_lpf_bound_keeps_es_lower_cutoff);
}
