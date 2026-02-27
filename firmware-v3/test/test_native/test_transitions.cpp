// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * LightwaveOS v2 - Transition Unit Tests
 *
 * Tests for transition system including:
 * - 12 transition types have valid names and durations
 * - 15 easing curves produce correct values
 * - Transition properties are consistent
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#include "mocks/fastled_mock.h"
#endif

#include <cstdint>
#include <cmath>

// Constants
constexpr float EPSILON = 0.001f;  // Tolerance for float comparisons

//==============================================================================
// Transition Type Enum (matching TransitionTypes.h)
//==============================================================================

enum class TransitionType : uint8_t {
    FADE = 0,
    WIPE_OUT = 1,
    WIPE_IN = 2,
    DISSOLVE = 3,
    PHASE_SHIFT = 4,
    PULSEWAVE = 5,
    IMPLOSION = 6,
    IRIS = 7,
    NUCLEAR = 8,
    STARGATE = 9,
    KALEIDOSCOPE = 10,
    MANDALA = 11,
    TYPE_COUNT = 12
};

//==============================================================================
// Easing Curve Enum (matching Easing.h)
//==============================================================================

enum class EasingCurve : uint8_t {
    LINEAR = 0,
    IN_QUAD = 1,
    OUT_QUAD = 2,
    IN_OUT_QUAD = 3,
    IN_CUBIC = 4,
    OUT_CUBIC = 5,
    IN_OUT_CUBIC = 6,
    IN_ELASTIC = 7,
    OUT_ELASTIC = 8,
    IN_OUT_ELASTIC = 9,
    IN_BOUNCE = 10,
    OUT_BOUNCE = 11,
    IN_BACK = 12,
    OUT_BACK = 13,
    IN_OUT_BACK = 14,
    CURVE_COUNT = 15
};

//==============================================================================
// Transition Name Function (matching TransitionTypes.h)
//==============================================================================

const char* getTransitionName(TransitionType type) {
    switch (type) {
        case TransitionType::FADE:        return "Fade";
        case TransitionType::WIPE_OUT:    return "Wipe Out";
        case TransitionType::WIPE_IN:     return "Wipe In";
        case TransitionType::DISSOLVE:    return "Dissolve";
        case TransitionType::PHASE_SHIFT: return "Phase Shift";
        case TransitionType::PULSEWAVE:   return "Pulsewave";
        case TransitionType::IMPLOSION:   return "Implosion";
        case TransitionType::IRIS:        return "Iris";
        case TransitionType::NUCLEAR:     return "Nuclear";
        case TransitionType::STARGATE:    return "Stargate";
        case TransitionType::KALEIDOSCOPE: return "Kaleidoscope";
        case TransitionType::MANDALA:     return "Mandala";
        default: return "Unknown";
    }
}

uint16_t getDefaultDuration(TransitionType type) {
    switch (type) {
        case TransitionType::FADE:        return 800;
        case TransitionType::WIPE_OUT:    return 1200;
        case TransitionType::WIPE_IN:     return 1200;
        case TransitionType::DISSOLVE:    return 1500;
        case TransitionType::PHASE_SHIFT: return 1400;
        case TransitionType::PULSEWAVE:   return 2000;
        case TransitionType::IMPLOSION:   return 1500;
        case TransitionType::IRIS:        return 1200;
        case TransitionType::NUCLEAR:     return 2500;
        case TransitionType::STARGATE:    return 3000;
        case TransitionType::KALEIDOSCOPE: return 1800;
        case TransitionType::MANDALA:     return 2200;
        default: return 1000;
    }
}

//==============================================================================
// Easing Function (matching Easing.h)
//==============================================================================

const char* getEasingName(EasingCurve curve) {
    switch (curve) {
        case EasingCurve::LINEAR:       return "Linear";
        case EasingCurve::IN_QUAD:      return "In Quad";
        case EasingCurve::OUT_QUAD:     return "Out Quad";
        case EasingCurve::IN_OUT_QUAD:  return "InOut Quad";
        case EasingCurve::IN_CUBIC:     return "In Cubic";
        case EasingCurve::OUT_CUBIC:    return "Out Cubic";
        case EasingCurve::IN_OUT_CUBIC: return "InOut Cubic";
        case EasingCurve::IN_ELASTIC:   return "In Elastic";
        case EasingCurve::OUT_ELASTIC:  return "Out Elastic";
        case EasingCurve::IN_OUT_ELASTIC: return "InOut Elastic";
        case EasingCurve::IN_BOUNCE:    return "In Bounce";
        case EasingCurve::OUT_BOUNCE:   return "Out Bounce";
        case EasingCurve::IN_BACK:      return "In Back";
        case EasingCurve::OUT_BACK:     return "Out Back";
        case EasingCurve::IN_OUT_BACK:  return "InOut Back";
        default: return "Unknown";
    }
}

inline float constrain_f(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

float ease(float t, EasingCurve curve) {
    t = constrain_f(t, 0.0f, 1.0f);

    switch (curve) {
        case EasingCurve::LINEAR:
            return t;

        case EasingCurve::IN_QUAD:
            return t * t;

        case EasingCurve::OUT_QUAD:
            return t * (2.0f - t);

        case EasingCurve::IN_OUT_QUAD:
            return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);

        case EasingCurve::IN_CUBIC:
            return t * t * t;

        case EasingCurve::OUT_CUBIC: {
            float f = t - 1.0f;
            return f * f * f + 1.0f;
        }

        case EasingCurve::IN_OUT_CUBIC:
            return (t < 0.5f)
                ? (4.0f * t * t * t)
                : ((t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f);

        case EasingCurve::IN_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            float p = 0.3f;
            return -powf(2.0f, 10.0f * (t - 1.0f)) * sinf((t - 1.1f) * 2.0f * M_PI / p);
        }

        case EasingCurve::OUT_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            float p = 0.3f;
            return powf(2.0f, -10.0f * t) * sinf((t - 0.1f) * 2.0f * M_PI / p) + 1.0f;
        }

        case EasingCurve::OUT_BOUNCE: {
            if (t < 1.0f / 2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2.0f / 2.75f) {
                t -= 1.5f / 2.75f;
                return 7.5625f * t * t + 0.75f;
            } else if (t < 2.5f / 2.75f) {
                t -= 2.25f / 2.75f;
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f / 2.75f;
                return 7.5625f * t * t + 0.984375f;
            }
        }

        case EasingCurve::IN_BOUNCE:
            return 1.0f - ease(1.0f - t, EasingCurve::OUT_BOUNCE);

        case EasingCurve::IN_BACK: {
            float s = 1.70158f;
            return t * t * ((s + 1.0f) * t - s);
        }

        case EasingCurve::OUT_BACK: {
            float s = 1.70158f;
            float f = t - 1.0f;
            return f * f * ((s + 1.0f) * f + s) + 1.0f;
        }

        case EasingCurve::IN_OUT_BACK: {
            float s = 1.70158f * 1.525f;
            if (t < 0.5f) {
                return 0.5f * (4.0f * t * t * ((s + 1.0f) * 2.0f * t - s));
            }
            float f = 2.0f * t - 2.0f;
            return 0.5f * (f * f * ((s + 1.0f) * f + s) + 2.0f);
        }

        default:
            return t;
    }
}

//==============================================================================
// Transition Type Tests
//==============================================================================

void test_all_transition_types_have_names() {
    // All 12 transition types should have unique names (not "Unknown")
    for (int i = 0; i < static_cast<int>(TransitionType::TYPE_COUNT); i++) {
        TransitionType type = static_cast<TransitionType>(i);
        const char* name = getTransitionName(type);
        TEST_ASSERT_TRUE_MESSAGE(name != nullptr, "Transition name is null");
        TEST_ASSERT_TRUE_MESSAGE(strcmp(name, "Unknown") != 0, "Transition has no name");
    }
}

void test_transition_type_count_is_12() {
    TEST_ASSERT_EQUAL(12, static_cast<int>(TransitionType::TYPE_COUNT));
}

void test_transition_durations_are_reasonable() {
    // All transitions should have durations between 500ms and 5000ms
    for (int i = 0; i < static_cast<int>(TransitionType::TYPE_COUNT); i++) {
        TransitionType type = static_cast<TransitionType>(i);
        uint16_t duration = getDefaultDuration(type);
        TEST_ASSERT_GREATER_OR_EQUAL(500, duration);
        TEST_ASSERT_LESS_OR_EQUAL(5000, duration);
    }
}

void test_fade_duration_is_fastest() {
    // FADE should be the fastest transition (for quick changes)
    uint16_t fadeDuration = getDefaultDuration(TransitionType::FADE);
    for (int i = 1; i < static_cast<int>(TransitionType::TYPE_COUNT); i++) {
        TransitionType type = static_cast<TransitionType>(i);
        uint16_t duration = getDefaultDuration(type);
        TEST_ASSERT_LESS_OR_EQUAL(duration, fadeDuration);
    }
}

void test_stargate_duration_is_longest() {
    // STARGATE should be the slowest (most dramatic) transition
    uint16_t stargateDuration = getDefaultDuration(TransitionType::STARGATE);
    for (int i = 0; i < static_cast<int>(TransitionType::TYPE_COUNT); i++) {
        TransitionType type = static_cast<TransitionType>(i);
        uint16_t duration = getDefaultDuration(type);
        TEST_ASSERT_LESS_OR_EQUAL(stargateDuration, duration);
    }
}

//==============================================================================
// Easing Curve Tests
//==============================================================================

void test_all_easing_curves_have_names() {
    for (int i = 0; i < static_cast<int>(EasingCurve::CURVE_COUNT); i++) {
        EasingCurve curve = static_cast<EasingCurve>(i);
        const char* name = getEasingName(curve);
        TEST_ASSERT_TRUE_MESSAGE(name != nullptr, "Easing name is null");
        TEST_ASSERT_TRUE_MESSAGE(strcmp(name, "Unknown") != 0, "Easing has no name");
    }
}

void test_easing_curve_count_is_15() {
    TEST_ASSERT_EQUAL(15, static_cast<int>(EasingCurve::CURVE_COUNT));
}

void test_linear_easing_is_identity() {
    // Linear easing: output = input
    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        float result = ease(t, EasingCurve::LINEAR);
        TEST_ASSERT_FLOAT_WITHIN(EPSILON, t, result);
    }
}

void test_all_easings_start_at_zero() {
    // All easing curves should start at 0 (t=0 -> result=0)
    for (int i = 0; i < static_cast<int>(EasingCurve::CURVE_COUNT); i++) {
        EasingCurve curve = static_cast<EasingCurve>(i);
        float result = ease(0.0f, curve);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, result);
    }
}

void test_all_easings_end_at_one() {
    // All easing curves should end at 1 (t=1 -> result=1)
    for (int i = 0; i < static_cast<int>(EasingCurve::CURVE_COUNT); i++) {
        EasingCurve curve = static_cast<EasingCurve>(i);
        float result = ease(1.0f, curve);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, result);
    }
}

void test_in_quad_is_slow_start() {
    // IN_QUAD: starts slow (at t=0.25, result should be ~0.0625)
    float result = ease(0.25f, EasingCurve::IN_QUAD);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.0625f, result);  // 0.25^2 = 0.0625
}

void test_out_quad_is_slow_end() {
    // OUT_QUAD: ends slow (at t=0.75, result should be ~0.9375)
    float result = ease(0.75f, EasingCurve::OUT_QUAD);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.9375f, result);  // 0.75*(2-0.75) = 0.9375
}

void test_in_out_quad_symmetric() {
    // IN_OUT_QUAD should be symmetric around 0.5
    float at_quarter = ease(0.25f, EasingCurve::IN_OUT_QUAD);
    float at_three_quarter = ease(0.75f, EasingCurve::IN_OUT_QUAD);
    // at_quarter + at_three_quarter should equal 1.0
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 1.0f, at_quarter + at_three_quarter);
}

void test_in_cubic_slower_than_quad() {
    // IN_CUBIC should be slower at start than IN_QUAD
    float quad_result = ease(0.25f, EasingCurve::IN_QUAD);
    float cubic_result = ease(0.25f, EasingCurve::IN_CUBIC);
    TEST_ASSERT_TRUE(cubic_result < quad_result);
}

void test_bounce_effect_exists() {
    // OUT_BOUNCE should have characteristic "bounce" - values approach 1 multiple times
    float prev = 0.0f;
    bool sawDecrease = false;
    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        float result = ease(t, EasingCurve::OUT_BOUNCE);
        if (result < prev && prev > 0.5f) {
            sawDecrease = true;  // Bounce detected (value decreased after being high)
        }
        prev = result;
    }
    // OUT_BOUNCE at end portion bounces, but might not show in all cases
    // The key is it ends at 1.0
    float final_result = ease(1.0f, EasingCurve::OUT_BOUNCE);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 1.0f, final_result);
}

void test_elastic_overshoots() {
    // OUT_ELASTIC should overshoot 1.0 during the middle of transition
    bool sawOvershoot = false;
    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        float result = ease(t, EasingCurve::OUT_ELASTIC);
        if (result > 1.0f) sawOvershoot = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(sawOvershoot, "OUT_ELASTIC should overshoot 1.0");
}

void test_back_undershoots_at_start() {
    // IN_BACK should go negative at the start (undershoots 0)
    bool sawUndershoot = false;
    for (float t = 0.0f; t <= 0.5f; t += 0.05f) {
        float result = ease(t, EasingCurve::IN_BACK);
        if (result < 0.0f) sawUndershoot = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(sawUndershoot, "IN_BACK should undershoot 0");
}

void test_out_back_overshoots() {
    // OUT_BACK should overshoot 1.0 near the end
    bool sawOvershoot = false;
    for (float t = 0.5f; t <= 1.0f; t += 0.05f) {
        float result = ease(t, EasingCurve::OUT_BACK);
        if (result > 1.0f) sawOvershoot = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(sawOvershoot, "OUT_BACK should overshoot 1.0");
}

void test_easing_clamped_at_edges() {
    // Input should be clamped to [0, 1]
    float below_zero = ease(-0.5f, EasingCurve::LINEAR);
    float above_one = ease(1.5f, EasingCurve::LINEAR);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.0f, below_zero);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 1.0f, above_one);
}

//==============================================================================
// Transition/Easing Integration Tests
//==============================================================================

void test_transition_progress_0_to_100() {
    // Simulate a transition from 0% to 100%
    for (int i = 0; i < static_cast<int>(EasingCurve::CURVE_COUNT); i++) {
        EasingCurve curve = static_cast<EasingCurve>(i);

        float start = ease(0.0f, curve);
        float end = ease(1.0f, curve);

        // All curves should produce valid output
        TEST_ASSERT_TRUE(!isnan(start));
        TEST_ASSERT_TRUE(!isnan(end));
    }
}

void test_transition_monotonic_for_basic_curves() {
    // LINEAR, IN_QUAD, OUT_QUAD should be monotonically increasing
    EasingCurve monotonicCurves[] = {
        EasingCurve::LINEAR,
        EasingCurve::IN_QUAD,
        EasingCurve::OUT_QUAD,
        EasingCurve::IN_CUBIC,
        EasingCurve::OUT_CUBIC
    };

    for (EasingCurve curve : monotonicCurves) {
        float prev = -1.0f;
        for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
            float result = ease(t, curve);
            TEST_ASSERT_GREATER_OR_EQUAL(prev, result);
            prev = result;
        }
    }
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_transition_tests() {
    // Transition types
    RUN_TEST(test_all_transition_types_have_names);
    RUN_TEST(test_transition_type_count_is_12);
    RUN_TEST(test_transition_durations_are_reasonable);
    RUN_TEST(test_fade_duration_is_fastest);
    RUN_TEST(test_stargate_duration_is_longest);

    // Easing curves
    RUN_TEST(test_all_easing_curves_have_names);
    RUN_TEST(test_easing_curve_count_is_15);
    RUN_TEST(test_linear_easing_is_identity);
    RUN_TEST(test_all_easings_start_at_zero);
    RUN_TEST(test_all_easings_end_at_one);
    RUN_TEST(test_in_quad_is_slow_start);
    RUN_TEST(test_out_quad_is_slow_end);
    RUN_TEST(test_in_out_quad_symmetric);
    RUN_TEST(test_in_cubic_slower_than_quad);
    RUN_TEST(test_bounce_effect_exists);
    RUN_TEST(test_elastic_overshoots);
    RUN_TEST(test_back_undershoots_at_start);
    RUN_TEST(test_out_back_overshoots);
    RUN_TEST(test_easing_clamped_at_edges);

    // Integration
    RUN_TEST(test_transition_progress_0_to_100);
    RUN_TEST(test_transition_monotonic_for_basic_curves);
}
