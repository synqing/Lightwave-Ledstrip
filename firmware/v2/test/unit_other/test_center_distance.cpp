/**
 * @file test_center_distance.cpp
 * @brief Unit tests for center distance calculation functions
 * 
 * Tests centerPairDistance() and centerPairSignedPosition() functions
 * to ensure symmetric center treatment and correct distance calculations.
 */

#include <unity.h>
#include "../../src/effects/CoreEffects.h"

using namespace lightwaveos::effects;

void setUp(void) {
    // Set up test environment
}

void tearDown(void) {
    // Clean up after test
}

// Test centerPairDistance() function
void test_centerPairDistance_symmetric_center(void) {
    // LEDs 79 and 80 should both have distance 0 (symmetric center treatment)
    TEST_ASSERT_EQUAL(0, centerPairDistance(79));
    TEST_ASSERT_EQUAL(0, centerPairDistance(80));
}

void test_centerPairDistance_edge_cases(void) {
    // Edge LEDs should have maximum distance
    TEST_ASSERT_EQUAL(79, centerPairDistance(0));   // Left edge
    TEST_ASSERT_EQUAL(79, centerPairDistance(159)); // Right edge
}

void test_centerPairDistance_progression(void) {
    // Distance should increase symmetrically from center
    TEST_ASSERT_EQUAL(1, centerPairDistance(78));  // One LED left of center
    TEST_ASSERT_EQUAL(1, centerPairDistance(81)); // One LED right of center
    TEST_ASSERT_EQUAL(2, centerPairDistance(77));  // Two LEDs left of center
    TEST_ASSERT_EQUAL(2, centerPairDistance(82));  // Two LEDs right of center
}

void test_centerPairDistance_midpoints(void) {
    // Test some midpoints
    TEST_ASSERT_EQUAL(40, centerPairDistance(39));  // Left half midpoint
    TEST_ASSERT_EQUAL(40, centerPairDistance(120)); // Right half midpoint
}

// Test centerPairSignedPosition() function
void test_centerPairSignedPosition_symmetric_center(void) {
    // LEDs 79 and 80 should have symmetric but opposite signed positions
    float pos79 = centerPairSignedPosition(79);
    float pos80 = centerPairSignedPosition(80);
    
    // Should be symmetric around 0
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -0.5f, pos79);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.5f, pos80);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, (pos79 + pos80) / 2.0f);
}

void test_centerPairSignedPosition_edge_cases(void) {
    // Edge LEDs should have maximum signed distance
    float pos0 = centerPairSignedPosition(0);
    float pos159 = centerPairSignedPosition(159);
    
    // Left edge should be negative, right edge should be positive
    TEST_ASSERT(pos0 < 0);
    TEST_ASSERT(pos159 > 0);
    
    // Should be symmetric in magnitude
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -pos0, pos159);
}

void test_centerPairSignedPosition_progression(void) {
    // Signed position should increase monotonically
    float pos78 = centerPairSignedPosition(78);
    float pos79 = centerPairSignedPosition(79);
    float pos80 = centerPairSignedPosition(80);
    float pos81 = centerPairSignedPosition(81);
    
    TEST_ASSERT(pos78 < pos79);
    TEST_ASSERT(pos79 < pos80);
    TEST_ASSERT(pos80 < pos81);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_centerPairDistance_symmetric_center);
    RUN_TEST(test_centerPairDistance_edge_cases);
    RUN_TEST(test_centerPairDistance_progression);
    RUN_TEST(test_centerPairDistance_midpoints);
    RUN_TEST(test_centerPairSignedPosition_symmetric_center);
    RUN_TEST(test_centerPairSignedPosition_edge_cases);
    RUN_TEST(test_centerPairSignedPosition_progression);
    
    return UNITY_END();
}

