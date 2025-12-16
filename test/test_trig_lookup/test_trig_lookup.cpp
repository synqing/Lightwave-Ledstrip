#include <unity.h>
#include <cmath>

// For native builds, include the implementation directly
#include "TrigLookup.h"
#include "TrigLookup.cpp"

using namespace TrigLookup;

// Test sin8_fast at cardinal angles
// theta 0 = 0°, 64 = 90°, 128 = 180°, 192 = 270°
void test_sin8_fast_cardinal_angles() {
    // 0° -> sin = 0 -> mapped to 128 (midpoint)
    TEST_ASSERT_EQUAL_UINT8(128, sin8_fast(0));

    // 90° (theta=64) -> sin = 1 -> mapped to 255 (max)
    TEST_ASSERT_EQUAL_UINT8(255, sin8_fast(64));

    // 180° (theta=128) -> sin = 0 -> mapped to 128 (midpoint)
    TEST_ASSERT_EQUAL_UINT8(128, sin8_fast(128));

    // 270° (theta=192) -> sin = -1 -> mapped to 0-1 (min)
    TEST_ASSERT_UINT8_WITHIN(1, 0, sin8_fast(192));
}

// Test cos8_fast at cardinal angles (cos is sin shifted by 90°)
void test_cos8_fast_cardinal_angles() {
    // 0° -> cos = 1 -> mapped to 255 (max)
    TEST_ASSERT_EQUAL_UINT8(255, cos8_fast(0));

    // 90° -> cos = 0 -> mapped to 128 (midpoint)
    TEST_ASSERT_EQUAL_UINT8(128, cos8_fast(64));

    // 180° -> cos = -1 -> mapped to 0-1 (min)
    TEST_ASSERT_UINT8_WITHIN(1, 0, cos8_fast(128));

    // 270° -> cos = 0 -> mapped to 128 (midpoint)
    TEST_ASSERT_EQUAL_UINT8(128, cos8_fast(192));
}

// Test signed sine values
void test_sin8_signed_cardinal_angles() {
    // 0° -> sin = 0
    TEST_ASSERT_EQUAL_INT8(0, sin8_signed(0));

    // 90° -> sin = 1 -> mapped to 127 (max)
    TEST_ASSERT_EQUAL_INT8(127, sin8_signed(64));

    // 180° -> sin = 0
    TEST_ASSERT_EQUAL_INT8(0, sin8_signed(128));

    // 270° -> sin = -1 -> mapped to -127 (min)
    TEST_ASSERT_EQUAL_INT8(-127, sin8_signed(192));
}

// Test signed cosine values
void test_cos8_signed_cardinal_angles() {
    // 0° -> cos = 1 -> mapped to 127 (max)
    TEST_ASSERT_EQUAL_INT8(127, cos8_signed(0));

    // 90° -> cos = 0
    TEST_ASSERT_EQUAL_INT8(0, cos8_signed(64));

    // 180° -> cos = -1 -> mapped to -127 (min)
    TEST_ASSERT_EQUAL_INT8(-127, cos8_signed(128));

    // 270° -> cos = 0
    TEST_ASSERT_EQUAL_INT8(0, cos8_signed(192));
}

// Test float sine values against math library
void test_sinf_fast_accuracy() {
    // Check that lookup matches sin() within documented tolerance (1.23%)
    for (uint16_t theta = 0; theta < 256; theta++) {
        float expected = sinf(theta * 2.0f * M_PI / 256.0f);
        float actual = sinf_fast(theta);
        // Allow 1.5% tolerance (slightly above documented 1.23%)
        TEST_ASSERT_FLOAT_WITHIN(0.015f, expected, actual);
    }
}

// Test float cosine values against math library
void test_cosf_fast_accuracy() {
    for (uint16_t theta = 0; theta < 256; theta++) {
        float expected = cosf(theta * 2.0f * M_PI / 256.0f);
        float actual = cosf_fast(theta);
        TEST_ASSERT_FLOAT_WITHIN(0.015f, expected, actual);
    }
}

// Test radians to theta conversion
void test_rad_to_theta_conversion() {
    // 0 radians -> theta 0
    TEST_ASSERT_EQUAL_UINT8(0, rad_to_theta(0.0f));

    // PI/2 radians (90°) -> theta 64
    TEST_ASSERT_UINT8_WITHIN(1, 64, rad_to_theta(M_PI / 2.0f));

    // PI radians (180°) -> theta 128
    TEST_ASSERT_UINT8_WITHIN(1, 128, rad_to_theta(M_PI));

    // 3PI/2 radians (270°) -> theta 192
    TEST_ASSERT_UINT8_WITHIN(1, 192, rad_to_theta(3.0f * M_PI / 2.0f));
}

// Test degrees to theta conversion
void test_deg_to_theta_conversion() {
    // 0° -> theta 0
    TEST_ASSERT_EQUAL_UINT8(0, deg_to_theta(0.0f));

    // 90° -> theta 64
    TEST_ASSERT_UINT8_WITHIN(1, 64, deg_to_theta(90.0f));

    // 180° -> theta 128
    TEST_ASSERT_UINT8_WITHIN(1, 128, deg_to_theta(180.0f));

    // 270° -> theta 192
    TEST_ASSERT_UINT8_WITHIN(1, 192, deg_to_theta(270.0f));
}

// Test position to theta conversion for LED effects
void test_pos_to_theta_conversion() {
    // Position 0 always gives theta 0
    TEST_ASSERT_EQUAL_UINT8(0, pos_to_theta(0, 100));
    TEST_ASSERT_EQUAL_UINT8(0, pos_to_theta(0, 255));

    // Position at half scale gives theta ~128 (half cycle)
    // pos_to_theta(50, 100) = (50 * 256) / 100 = 128
    TEST_ASSERT_UINT8_WITHIN(1, 128, pos_to_theta(50, 100));

    // Position at full scale gives theta ~256 (wraps to 0)
    // pos_to_theta(100, 100) = (100 * 256) / 100 = 256 -> wraps to 0
    TEST_ASSERT_EQUAL_UINT8(0, pos_to_theta(100, 100));
}

// Test that lookup functions handle wrapping correctly
void test_theta_wrapping() {
    // Since theta is uint8_t, 256 wraps to 0
    // Verify continuity at wrap point
    TEST_ASSERT_EQUAL_UINT8(sin8_fast(0), sin8_fast(0));  // Same point

    // Values near wrap should be close
    int diff = abs((int)sin8_fast(255) - (int)sin8_fast(0));
    TEST_ASSERT_LESS_THAN(5, diff);  // Should be within 5 units of each other
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_sin8_fast_cardinal_angles);
    RUN_TEST(test_cos8_fast_cardinal_angles);
    RUN_TEST(test_sin8_signed_cardinal_angles);
    RUN_TEST(test_cos8_signed_cardinal_angles);
    RUN_TEST(test_sinf_fast_accuracy);
    RUN_TEST(test_cosf_fast_accuracy);
    RUN_TEST(test_rad_to_theta_conversion);
    RUN_TEST(test_deg_to_theta_conversion);
    RUN_TEST(test_pos_to_theta_conversion);
    RUN_TEST(test_theta_wrapping);

    return UNITY_END();
}
