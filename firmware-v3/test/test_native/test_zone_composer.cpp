/**
 * LightwaveOS v2 - Zone Composition Unit Tests
 *
 * Tests for zone system including:
 * - Zone boundary definitions (no overlap, full coverage)
 * - Blend mode correctness
 * - Buffer isolation between zones
 * - CENTER ORIGIN compliance of zone layouts
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#include "mocks/fastled_mock.h"
#endif

#include <cstdint>
#include <algorithm>

// Zone constants (matching ZoneDefinition.h)
constexpr uint8_t MAX_ZONES = 4;
constexpr uint16_t STRIP_LENGTH = 160;
constexpr uint16_t TOTAL_LEDS = 320;
constexpr uint16_t CENTER_LEFT = 79;
constexpr uint16_t CENTER_RIGHT = 80;

//==============================================================================
// Zone Segment Definition (simplified for testing)
//==============================================================================

struct ZoneSegment {
    uint8_t zoneId;
    uint8_t s1LeftStart;
    uint8_t s1LeftEnd;
    uint8_t s1RightStart;
    uint8_t s1RightEnd;
    uint8_t totalLeds;
};

// 3-Zone Layout
constexpr ZoneSegment ZONE_3_CONFIG[3] = {
    { .zoneId = 0, .s1LeftStart = 65, .s1LeftEnd = 79,
      .s1RightStart = 80, .s1RightEnd = 94, .totalLeds = 30 },
    { .zoneId = 1, .s1LeftStart = 20, .s1LeftEnd = 64,
      .s1RightStart = 95, .s1RightEnd = 139, .totalLeds = 90 },
    { .zoneId = 2, .s1LeftStart = 0, .s1LeftEnd = 19,
      .s1RightStart = 140, .s1RightEnd = 159, .totalLeds = 40 }
};

// 4-Zone Layout
constexpr ZoneSegment ZONE_4_CONFIG[4] = {
    { .zoneId = 0, .s1LeftStart = 60, .s1LeftEnd = 79,
      .s1RightStart = 80, .s1RightEnd = 99, .totalLeds = 40 },
    { .zoneId = 1, .s1LeftStart = 40, .s1LeftEnd = 59,
      .s1RightStart = 100, .s1RightEnd = 119, .totalLeds = 40 },
    { .zoneId = 2, .s1LeftStart = 20, .s1LeftEnd = 39,
      .s1RightStart = 120, .s1RightEnd = 139, .totalLeds = 40 },
    { .zoneId = 3, .s1LeftStart = 0, .s1LeftEnd = 19,
      .s1RightStart = 140, .s1RightEnd = 159, .totalLeds = 40 }
};

//==============================================================================
// Blend Mode Enum
//==============================================================================

enum class BlendMode : uint8_t {
    OVERWRITE = 0,
    ADDITIVE = 1,
    MULTIPLY = 2,
    SCREEN = 3,
    OVERLAY = 4,
    ALPHA = 5,
    LIGHTEN = 6,
    DARKEN = 7,
    MODE_COUNT = 8
};

//==============================================================================
// Blend Functions (matching BlendMode.h)
//==============================================================================

inline uint8_t qadd8(uint8_t a, uint8_t b) {
    uint16_t sum = a + b;
    return sum > 255 ? 255 : sum;
}

inline uint8_t scale8(uint8_t a, uint8_t b) {
    return (uint8_t)((uint16_t)a * b / 255);
}

CRGB blendPixels(const CRGB& base, const CRGB& blend, BlendMode mode) {
    switch (mode) {
        case BlendMode::OVERWRITE:
            return blend;

        case BlendMode::ADDITIVE:
            return CRGB(
                qadd8(base.r, blend.r),
                qadd8(base.g, blend.g),
                qadd8(base.b, blend.b)
            );

        case BlendMode::MULTIPLY:
            return CRGB(
                scale8(base.r, blend.r),
                scale8(base.g, blend.g),
                scale8(base.b, blend.b)
            );

        case BlendMode::SCREEN:
            return CRGB(
                255 - scale8(255 - base.r, 255 - blend.r),
                255 - scale8(255 - base.g, 255 - blend.g),
                255 - scale8(255 - base.b, 255 - blend.b)
            );

        case BlendMode::ALPHA:
            return CRGB(
                (base.r + blend.r) / 2,
                (base.g + blend.g) / 2,
                (base.b + blend.b) / 2
            );

        case BlendMode::LIGHTEN:
            return CRGB(
                std::max(base.r, blend.r),
                std::max(base.g, blend.g),
                std::max(base.b, blend.b)
            );

        case BlendMode::DARKEN:
            return CRGB(
                std::min(base.r, blend.r),
                std::min(base.g, blend.g),
                std::min(base.b, blend.b)
            );

        default:
            return blend;
    }
}

//==============================================================================
// Helper Functions
//==============================================================================

// Check if an LED index is in a zone (strip 1 only)
bool isInZone(uint16_t led, const ZoneSegment& zone) {
    return (led >= zone.s1LeftStart && led <= zone.s1LeftEnd) ||
           (led >= zone.s1RightStart && led <= zone.s1RightEnd);
}

// Get zone ID for an LED (returns -1 if not found)
int getZoneForLed(uint16_t led, const ZoneSegment* config, uint8_t zoneCount) {
    for (uint8_t z = 0; z < zoneCount; z++) {
        if (isInZone(led, config[z])) {
            return config[z].zoneId;
        }
    }
    return -1;
}

// Count LEDs in a zone
uint16_t countZoneLeds(const ZoneSegment& zone) {
    uint16_t leftCount = zone.s1LeftEnd - zone.s1LeftStart + 1;
    uint16_t rightCount = zone.s1RightEnd - zone.s1RightStart + 1;
    return leftCount + rightCount;
}

//==============================================================================
// Zone Definition Tests
//==============================================================================

void test_zone_3_center_contains_center_pair() {
    // Zone 0 should contain the center LEDs (79, 80)
    const ZoneSegment& zone0 = ZONE_3_CONFIG[0];
    TEST_ASSERT_TRUE(isInZone(CENTER_LEFT, zone0));
    TEST_ASSERT_TRUE(isInZone(CENTER_RIGHT, zone0));
}

void test_zone_4_center_contains_center_pair() {
    const ZoneSegment& zone0 = ZONE_4_CONFIG[0];
    TEST_ASSERT_TRUE(isInZone(CENTER_LEFT, zone0));
    TEST_ASSERT_TRUE(isInZone(CENTER_RIGHT, zone0));
}

void test_zone_3_full_coverage() {
    // All 160 LEDs should be in exactly one zone
    for (uint16_t led = 0; led < STRIP_LENGTH; led++) {
        int zoneId = getZoneForLed(led, ZONE_3_CONFIG, 3);
        TEST_ASSERT_TRUE_MESSAGE(zoneId >= 0,
            "LED not covered by any zone");
    }
}

void test_zone_4_full_coverage() {
    for (uint16_t led = 0; led < STRIP_LENGTH; led++) {
        int zoneId = getZoneForLed(led, ZONE_4_CONFIG, 4);
        TEST_ASSERT_TRUE_MESSAGE(zoneId >= 0,
            "LED not covered by any zone");
    }
}

void test_zone_3_no_overlap() {
    // Each LED should be in exactly one zone
    for (uint16_t led = 0; led < STRIP_LENGTH; led++) {
        int count = 0;
        for (int z = 0; z < 3; z++) {
            if (isInZone(led, ZONE_3_CONFIG[z])) count++;
        }
        TEST_ASSERT_EQUAL_MESSAGE(1, count,
            "LED is in multiple zones or no zone");
    }
}

void test_zone_4_no_overlap() {
    for (uint16_t led = 0; led < STRIP_LENGTH; led++) {
        int count = 0;
        for (int z = 0; z < 4; z++) {
            if (isInZone(led, ZONE_4_CONFIG[z])) count++;
        }
        TEST_ASSERT_EQUAL_MESSAGE(1, count,
            "LED is in multiple zones or no zone");
    }
}

void test_zone_3_total_led_count() {
    uint16_t total = 0;
    for (int z = 0; z < 3; z++) {
        total += countZoneLeds(ZONE_3_CONFIG[z]);
    }
    TEST_ASSERT_EQUAL(STRIP_LENGTH, total);
}

void test_zone_4_total_led_count() {
    uint16_t total = 0;
    for (int z = 0; z < 4; z++) {
        total += countZoneLeds(ZONE_4_CONFIG[z]);
    }
    TEST_ASSERT_EQUAL(STRIP_LENGTH, total);
}

void test_zone_3_symmetric_around_center() {
    // Zone 0 should be symmetric: left end at 79, right start at 80
    const ZoneSegment& zone0 = ZONE_3_CONFIG[0];
    TEST_ASSERT_EQUAL(CENTER_LEFT, zone0.s1LeftEnd);
    TEST_ASSERT_EQUAL(CENTER_RIGHT, zone0.s1RightStart);
}

void test_zone_4_symmetric_around_center() {
    const ZoneSegment& zone0 = ZONE_4_CONFIG[0];
    TEST_ASSERT_EQUAL(CENTER_LEFT, zone0.s1LeftEnd);
    TEST_ASSERT_EQUAL(CENTER_RIGHT, zone0.s1RightStart);
}

void test_zone_4_equal_distribution() {
    // All 4 zones should have 40 LEDs
    for (int z = 0; z < 4; z++) {
        TEST_ASSERT_EQUAL(40, ZONE_4_CONFIG[z].totalLeds);
    }
}

//==============================================================================
// Blend Mode Tests
//==============================================================================

void test_blend_overwrite() {
    CRGB base(100, 100, 100);
    CRGB blend(200, 50, 150);
    CRGB result = blendPixels(base, blend, BlendMode::OVERWRITE);
    TEST_ASSERT_EQUAL(200, result.r);
    TEST_ASSERT_EQUAL(50, result.g);
    TEST_ASSERT_EQUAL(150, result.b);
}

void test_blend_additive_no_overflow() {
    CRGB base(50, 100, 150);
    CRGB blend(50, 50, 50);
    CRGB result = blendPixels(base, blend, BlendMode::ADDITIVE);
    TEST_ASSERT_EQUAL(100, result.r);
    TEST_ASSERT_EQUAL(150, result.g);
    TEST_ASSERT_EQUAL(200, result.b);
}

void test_blend_additive_saturates() {
    CRGB base(200, 200, 200);
    CRGB blend(100, 100, 100);
    CRGB result = blendPixels(base, blend, BlendMode::ADDITIVE);
    // Should saturate at 255
    TEST_ASSERT_EQUAL(255, result.r);
    TEST_ASSERT_EQUAL(255, result.g);
    TEST_ASSERT_EQUAL(255, result.b);
}

void test_blend_multiply() {
    CRGB base(255, 128, 64);
    CRGB blend(255, 255, 255);
    CRGB result = blendPixels(base, blend, BlendMode::MULTIPLY);
    // Multiplying by white (255) should keep original
    TEST_ASSERT_EQUAL(255, result.r);
    TEST_ASSERT_EQUAL(128, result.g);
    TEST_ASSERT_EQUAL(64, result.b);
}

void test_blend_multiply_with_black() {
    CRGB base(255, 128, 64);
    CRGB blend(0, 0, 0);
    CRGB result = blendPixels(base, blend, BlendMode::MULTIPLY);
    // Multiplying by black should give black
    TEST_ASSERT_EQUAL(0, result.r);
    TEST_ASSERT_EQUAL(0, result.g);
    TEST_ASSERT_EQUAL(0, result.b);
}

void test_blend_lighten() {
    CRGB base(100, 150, 200);
    CRGB blend(200, 100, 150);
    CRGB result = blendPixels(base, blend, BlendMode::LIGHTEN);
    // Takes brighter of each channel
    TEST_ASSERT_EQUAL(200, result.r);  // blend.r > base.r
    TEST_ASSERT_EQUAL(150, result.g);  // base.g > blend.g
    TEST_ASSERT_EQUAL(200, result.b);  // base.b > blend.b
}

void test_blend_darken() {
    CRGB base(100, 150, 200);
    CRGB blend(200, 100, 150);
    CRGB result = blendPixels(base, blend, BlendMode::DARKEN);
    // Takes darker of each channel
    TEST_ASSERT_EQUAL(100, result.r);  // base.r < blend.r
    TEST_ASSERT_EQUAL(100, result.g);  // blend.g < base.g
    TEST_ASSERT_EQUAL(150, result.b);  // blend.b < base.b
}

void test_blend_alpha() {
    CRGB base(0, 100, 200);
    CRGB blend(100, 200, 0);
    CRGB result = blendPixels(base, blend, BlendMode::ALPHA);
    // 50/50 mix
    TEST_ASSERT_EQUAL(50, result.r);
    TEST_ASSERT_EQUAL(150, result.g);
    TEST_ASSERT_EQUAL(100, result.b);
}

void test_blend_screen_with_white() {
    CRGB base(100, 100, 100);
    CRGB blend(255, 255, 255);
    CRGB result = blendPixels(base, blend, BlendMode::SCREEN);
    // Screening with white gives white
    TEST_ASSERT_EQUAL(255, result.r);
    TEST_ASSERT_EQUAL(255, result.g);
    TEST_ASSERT_EQUAL(255, result.b);
}

void test_blend_screen_with_black() {
    CRGB base(100, 150, 200);
    CRGB blend(0, 0, 0);
    CRGB result = blendPixels(base, blend, BlendMode::SCREEN);
    // Screening with black keeps original
    TEST_ASSERT_EQUAL(100, result.r);
    TEST_ASSERT_EQUAL(150, result.g);
    TEST_ASSERT_EQUAL(200, result.b);
}

//==============================================================================
// Buffer Isolation Tests
//==============================================================================

void test_zone_buffer_isolation() {
    // Simulate writing to zone 0 and verify zone 1 is not affected
    CRGB buffer[STRIP_LENGTH] = {};

    // Fill zone 0 with red
    const ZoneSegment& zone0 = ZONE_3_CONFIG[0];
    for (uint16_t i = zone0.s1LeftStart; i <= zone0.s1LeftEnd; i++) {
        buffer[i] = CRGB::Red;
    }
    for (uint16_t i = zone0.s1RightStart; i <= zone0.s1RightEnd; i++) {
        buffer[i] = CRGB::Red;
    }

    // Verify zone 1 is still black
    const ZoneSegment& zone1 = ZONE_3_CONFIG[1];
    bool zone1Clean = true;
    for (uint16_t i = zone1.s1LeftStart; i <= zone1.s1LeftEnd; i++) {
        if (buffer[i] != CRGB::Black) zone1Clean = false;
    }
    for (uint16_t i = zone1.s1RightStart; i <= zone1.s1RightEnd; i++) {
        if (buffer[i] != CRGB::Black) zone1Clean = false;
    }

    TEST_ASSERT_TRUE(zone1Clean);
}

void test_zone_independent_colors() {
    // Each zone can have a different color
    CRGB buffer[STRIP_LENGTH] = {};
    CRGB zoneColors[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };

    // Fill each zone with its color
    for (int z = 0; z < 3; z++) {
        const ZoneSegment& zone = ZONE_3_CONFIG[z];
        for (uint16_t i = zone.s1LeftStart; i <= zone.s1LeftEnd; i++) {
            buffer[i] = zoneColors[z];
        }
        for (uint16_t i = zone.s1RightStart; i <= zone.s1RightEnd; i++) {
            buffer[i] = zoneColors[z];
        }
    }

    // Verify each zone has correct color
    for (int z = 0; z < 3; z++) {
        const ZoneSegment& zone = ZONE_3_CONFIG[z];
        for (uint16_t i = zone.s1LeftStart; i <= zone.s1LeftEnd; i++) {
            TEST_ASSERT_TRUE(buffer[i] == zoneColors[z]);
        }
    }
}

void test_zone_boundaries_are_correct() {
    // Verify boundary LEDs are in the correct zone
    // Zone 0 should end at LED 94 (right side)
    int zone = getZoneForLed(94, ZONE_3_CONFIG, 3);
    TEST_ASSERT_EQUAL(0, zone);

    // Zone 1 should start at LED 95 (right side)
    zone = getZoneForLed(95, ZONE_3_CONFIG, 3);
    TEST_ASSERT_EQUAL(1, zone);

    // Zone 1 should end at LED 139 (right side)
    zone = getZoneForLed(139, ZONE_3_CONFIG, 3);
    TEST_ASSERT_EQUAL(1, zone);

    // Zone 2 should start at LED 140 (right side)
    zone = getZoneForLed(140, ZONE_3_CONFIG, 3);
    TEST_ASSERT_EQUAL(2, zone);
}

void test_zones_are_concentric() {
    // Zone 0 is innermost (closest to center)
    // Zone 2 is outermost (at edges)
    const ZoneSegment& zone0 = ZONE_3_CONFIG[0];
    const ZoneSegment& zone2 = ZONE_3_CONFIG[2];

    // Zone 0 left segment should be closer to center than zone 2
    TEST_ASSERT_GREATER_THAN(zone2.s1LeftStart, zone0.s1LeftStart);

    // Zone 0 right segment should be closer to center than zone 2
    TEST_ASSERT_LESS_THAN(zone2.s1RightEnd, zone0.s1RightEnd);
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_zone_composer_tests() {
    // Zone definitions
    RUN_TEST(test_zone_3_center_contains_center_pair);
    RUN_TEST(test_zone_4_center_contains_center_pair);
    RUN_TEST(test_zone_3_full_coverage);
    RUN_TEST(test_zone_4_full_coverage);
    RUN_TEST(test_zone_3_no_overlap);
    RUN_TEST(test_zone_4_no_overlap);
    RUN_TEST(test_zone_3_total_led_count);
    RUN_TEST(test_zone_4_total_led_count);
    RUN_TEST(test_zone_3_symmetric_around_center);
    RUN_TEST(test_zone_4_symmetric_around_center);
    RUN_TEST(test_zone_4_equal_distribution);

    // Blend modes
    RUN_TEST(test_blend_overwrite);
    RUN_TEST(test_blend_additive_no_overflow);
    RUN_TEST(test_blend_additive_saturates);
    RUN_TEST(test_blend_multiply);
    RUN_TEST(test_blend_multiply_with_black);
    RUN_TEST(test_blend_lighten);
    RUN_TEST(test_blend_darken);
    RUN_TEST(test_blend_alpha);
    RUN_TEST(test_blend_screen_with_white);
    RUN_TEST(test_blend_screen_with_black);

    // Buffer isolation
    RUN_TEST(test_zone_buffer_isolation);
    RUN_TEST(test_zone_independent_colors);
    RUN_TEST(test_zone_boundaries_are_correct);
    RUN_TEST(test_zones_are_concentric);
}
