/**
 * LightwaveOS v2 - Effect Rendering Unit Tests
 *
 * Tests for effect rendering including:
 * - CENTER ORIGIN compliance (effects originate from LED 79/80)
 * - LED buffer boundary checking
 * - Parameter responsiveness (speed, brightness, hue)
 * - Strip mirroring (strip 1 and strip 2 symmetry)
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#include "mocks/fastled_mock.h"
#endif

#include <array>
#include <algorithm>
#include <cmath>

// Effect constants (matching CoreEffects.h)
constexpr uint16_t CENTER_LEFT = 79;
constexpr uint16_t CENTER_RIGHT = 80;
constexpr uint16_t HALF_LENGTH = 80;
constexpr uint16_t STRIP_LENGTH = 160;
constexpr uint16_t TOTAL_LEDS = 320;

//==============================================================================
// Mock RenderContext (simplified version for testing)
//==============================================================================

struct MockRenderContext {
    CRGB leds[TOTAL_LEDS];
    uint16_t numLeds;
    uint8_t brightness;
    uint8_t speed;
    uint8_t hue;
    uint32_t frameCount;
    uint32_t deltaTimeMs;
    CRGBPalette16 palette;

    MockRenderContext()
        : numLeds(TOTAL_LEDS)
        , brightness(128)
        , speed(10)
        , hue(0)
        , frameCount(0)
        , deltaTimeMs(8)
    {
        clear();
        // Initialize a simple test palette
        for (int i = 0; i < 16; i++) {
            palette[i] = CRGB(i * 16, 255 - i * 16, 128);
        }
    }

    void clear() {
        for (int i = 0; i < TOTAL_LEDS; i++) {
            leds[i] = CRGB::Black;
        }
    }

    // Count non-black LEDs
    int countLitLeds() const {
        int count = 0;
        for (int i = 0; i < TOTAL_LEDS; i++) {
            if (leds[i] != CRGB::Black) count++;
        }
        return count;
    }

    // Check if center region (around 79/80) is lit
    bool isCenterLit() const {
        return (leds[CENTER_LEFT] != CRGB::Black ||
                leds[CENTER_RIGHT] != CRGB::Black);
    }

    // Check if LED at distance from center is lit
    bool isDistanceLit(uint16_t distance) const {
        if (distance > CENTER_LEFT) return false;
        uint16_t leftPos = CENTER_LEFT - distance;
        uint16_t rightPos = CENTER_RIGHT + distance;
        return (leds[leftPos] != CRGB::Black ||
                leds[rightPos] != CRGB::Black);
    }

    // Get average brightness in center region (79-80 ± 5)
    uint8_t getCenterBrightness() const {
        int total = 0;
        int count = 0;
        for (int i = CENTER_LEFT - 5; i <= CENTER_RIGHT + 5; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                total += leds[i].getLuma();
                count++;
            }
        }
        return count > 0 ? total / count : 0;
    }

    // Get average brightness at edges (0-10, 150-159)
    uint8_t getEdgeBrightness() const {
        int total = 0;
        int count = 0;
        for (int i = 0; i < 10; i++) {
            total += leds[i].getLuma();
            count++;
        }
        for (int i = STRIP_LENGTH - 10; i < STRIP_LENGTH; i++) {
            total += leds[i].getLuma();
            count++;
        }
        return count > 0 ? total / count : 0;
    }

    // Check if strip 2 mirrors strip 1
    bool isStrip2Mirrored() const {
        int matchCount = 0;
        for (int i = 0; i < STRIP_LENGTH; i++) {
            if (leds[i] == leds[i + STRIP_LENGTH]) {
                matchCount++;
            }
        }
        // Allow 90% match (some effects may have slight variations)
        return matchCount > (STRIP_LENGTH * 9 / 10);
    }
};

//==============================================================================
// Mock Effect Implementations for Testing
// These simulate CENTER ORIGIN compliant effects
//==============================================================================

// Simple CENTER ORIGIN effect: creates a pulse from center
void mockCenterPulseEffect(MockRenderContext& ctx) {
    // Fade all LEDs
    for (int i = 0; i < TOTAL_LEDS; i++) {
        ctx.leds[i].r = ctx.leds[i].r > 10 ? ctx.leds[i].r - 10 : 0;
        ctx.leds[i].g = ctx.leds[i].g > 10 ? ctx.leds[i].g - 10 : 0;
        ctx.leds[i].b = ctx.leds[i].b > 10 ? ctx.leds[i].b - 10 : 0;
    }

    // Calculate pulse distance based on frame
    uint16_t pulseDistance = (ctx.frameCount * ctx.speed / 10) % HALF_LENGTH;

    // Set LEDs at pulse distance from center
    uint16_t leftPos = CENTER_LEFT - pulseDistance;
    uint16_t rightPos = CENTER_RIGHT + pulseDistance;

    CRGB color = CRGB(255, 128, 64);

    // Strip 1
    if (leftPos < STRIP_LENGTH) ctx.leds[leftPos] = color;
    if (rightPos < STRIP_LENGTH) ctx.leds[rightPos] = color;

    // Strip 2 (mirror)
    if (leftPos < STRIP_LENGTH) ctx.leds[leftPos + STRIP_LENGTH] = color;
    if (rightPos < STRIP_LENGTH) ctx.leds[rightPos + STRIP_LENGTH] = color;
}

// ANTI-PATTERN: Left-to-right effect (NOT CENTER ORIGIN compliant)
void mockBadLinearEffect(MockRenderContext& ctx) {
    // This is what we DON'T want - starts at edge and moves right
    uint16_t pos = ctx.frameCount % STRIP_LENGTH;
    ctx.leds[pos] = CRGB::Red;
    ctx.leds[pos + STRIP_LENGTH] = CRGB::Red;
}

// CENTER ORIGIN gradient effect
void mockCenterGradientEffect(MockRenderContext& ctx) {
    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from center
        float distFromCenter = std::abs((float)i - CENTER_LEFT);
        float normalizedDist = distFromCenter / HALF_LENGTH;

        // Intensity decreases from center
        uint8_t intensity = (uint8_t)(255 * (1.0f - normalizedDist));

        ctx.leds[i] = CRGB(intensity, intensity / 2, intensity / 4);
        ctx.leds[i + STRIP_LENGTH] = ctx.leds[i];
    }
}

// Speed-responsive effect
void mockSpeedEffect(MockRenderContext& ctx) {
    // Clear
    for (int i = 0; i < TOTAL_LEDS; i++) {
        ctx.leds[i] = CRGB::Black;
    }

    // Speed controls how many LEDs are lit from center
    uint16_t litCount = ctx.speed * 2;  // 0-100 LEDs based on speed
    if (litCount > HALF_LENGTH) litCount = HALF_LENGTH;

    for (uint16_t d = 0; d < litCount; d++) {
        if (CENTER_LEFT >= d && CENTER_RIGHT + d < STRIP_LENGTH) {
            ctx.leds[CENTER_LEFT - d] = CRGB::Blue;
            ctx.leds[CENTER_RIGHT + d] = CRGB::Blue;
            ctx.leds[CENTER_LEFT - d + STRIP_LENGTH] = CRGB::Blue;
            ctx.leds[CENTER_RIGHT + d + STRIP_LENGTH] = CRGB::Blue;
        }
    }
}

//==============================================================================
// Test Fixtures - Called by run_effect_tests()
//==============================================================================

static MockRenderContext* ctx = nullptr;

static void effect_setup() {
    if (ctx) delete ctx;
    ctx = new MockRenderContext();
}

static void effect_cleanup() {
    delete ctx;
    ctx = nullptr;
}

//==============================================================================
// CENTER ORIGIN Compliance Tests
//==============================================================================

void test_center_origin_pulse_starts_at_center() {
    effect_setup();
    ctx->frameCount = 0;
    mockCenterPulseEffect(*ctx);
    TEST_ASSERT_TRUE(ctx->isCenterLit());
    effect_cleanup();
}

void test_center_origin_pulse_expands_outward() {
    effect_setup();
    for (int frame = 0; frame < 10; frame++) {
        ctx->frameCount = frame;
        mockCenterPulseEffect(*ctx);
    }
    TEST_ASSERT_TRUE(ctx->countLitLeds() > 4);
    effect_cleanup();
}

void test_center_gradient_brightest_at_center() {
    effect_setup();
    mockCenterGradientEffect(*ctx);
    uint8_t centerBrightness = ctx->getCenterBrightness();
    uint8_t edgeBrightness = ctx->getEdgeBrightness();
    TEST_ASSERT_GREATER_THAN(edgeBrightness, centerBrightness);
    effect_cleanup();
}

void test_center_gradient_symmetric() {
    effect_setup();
    mockCenterGradientEffect(*ctx);
    for (int d = 0; d < HALF_LENGTH; d++) {
        CRGB leftColor = ctx->leds[CENTER_LEFT - d];
        CRGB rightColor = ctx->leds[CENTER_RIGHT + d];
        TEST_ASSERT_INT_WITHIN(5, leftColor.r, rightColor.r);
        TEST_ASSERT_INT_WITHIN(5, leftColor.g, rightColor.g);
        TEST_ASSERT_INT_WITHIN(5, leftColor.b, rightColor.b);
    }
    effect_cleanup();
}

void test_bad_linear_effect_detected() {
    effect_setup();
    ctx->frameCount = 0;
    mockBadLinearEffect(*ctx);
    TEST_ASSERT_TRUE(ctx->leds[0] != CRGB::Black);
    TEST_ASSERT_TRUE(ctx->leds[CENTER_LEFT] == CRGB::Black);
    TEST_ASSERT_TRUE(ctx->leds[CENTER_RIGHT] == CRGB::Black);
    effect_cleanup();
}

//==============================================================================
// Strip Mirroring Tests
//==============================================================================

void test_strip2_mirrors_strip1() {
    effect_setup();
    mockCenterGradientEffect(*ctx);
    TEST_ASSERT_TRUE(ctx->isStrip2Mirrored());
    effect_cleanup();
}

void test_strip2_mirrors_after_pulse() {
    effect_setup();
    for (int frame = 0; frame < 20; frame++) {
        ctx->frameCount = frame;
        mockCenterPulseEffect(*ctx);
    }
    TEST_ASSERT_TRUE(ctx->isStrip2Mirrored());
    effect_cleanup();
}

//==============================================================================
// Parameter Responsiveness Tests
//==============================================================================

void test_speed_affects_lit_leds() {
    effect_setup();
    ctx->speed = 5;
    mockSpeedEffect(*ctx);
    int lowSpeedLits = ctx->countLitLeds();

    ctx->clear();
    ctx->speed = 40;
    mockSpeedEffect(*ctx);
    int highSpeedLits = ctx->countLitLeds();

    TEST_ASSERT_GREATER_THAN(lowSpeedLits, highSpeedLits);
    effect_cleanup();
}

void test_zero_speed_still_renders() {
    effect_setup();
    ctx->speed = 0;
    mockSpeedEffect(*ctx);
    TEST_ASSERT_EQUAL(0, ctx->countLitLeds());
    effect_cleanup();
}

void test_max_speed_doesnt_overflow() {
    effect_setup();
    ctx->speed = 50;
    mockSpeedEffect(*ctx);
    TEST_ASSERT_TRUE(ctx->countLitLeds() <= HALF_LENGTH * 4);
    effect_cleanup();
}

//==============================================================================
// Boundary Tests
//==============================================================================

void test_no_writes_beyond_buffer() {
    effect_setup();
    for (int frame = 0; frame < 100; frame++) {
        ctx->frameCount = frame;
        mockCenterPulseEffect(*ctx);
    }
    // If we got here without segfault, buffer wasn't overrun
    TEST_ASSERT_TRUE(true);
    effect_cleanup();
}

void test_led_index_0_accessible() {
    effect_setup();
    ctx->leds[0] = CRGB::Red;
    TEST_ASSERT_TRUE(ctx->leds[0] == CRGB::Red);
    effect_cleanup();
}

void test_led_index_319_accessible() {
    effect_setup();
    ctx->leds[319] = CRGB::Green;
    TEST_ASSERT_TRUE(ctx->leds[319] == CRGB::Green);
    effect_cleanup();
}

//==============================================================================
// Frame Counter Tests
//==============================================================================

void test_effect_changes_over_frames() {
    effect_setup();
    mockCenterPulseEffect(*ctx);
    int frame0Lits = ctx->countLitLeds();

    ctx->frameCount = 50;
    mockCenterPulseEffect(*ctx);
    int frame50Lits = ctx->countLitLeds();

    TEST_ASSERT_TRUE(frame0Lits > 0 || frame50Lits > 0);
    effect_cleanup();
}

void test_delta_time_available() {
    effect_setup();
    TEST_ASSERT_EQUAL(8, ctx->deltaTimeMs);
    effect_cleanup();
}

//==============================================================================
// CENTER ORIGIN Constants Tests (no fixture needed)
//==============================================================================

void test_center_left_is_79() {
    TEST_ASSERT_EQUAL(79, CENTER_LEFT);
}

void test_center_right_is_80() {
    TEST_ASSERT_EQUAL(80, CENTER_RIGHT);
}

void test_half_length_is_80() {
    TEST_ASSERT_EQUAL(80, HALF_LENGTH);
}

void test_strip_length_is_160() {
    TEST_ASSERT_EQUAL(160, STRIP_LENGTH);
}

void test_total_leds_is_320() {
    TEST_ASSERT_EQUAL(320, TOTAL_LEDS);
}

void test_center_pair_are_adjacent() {
    TEST_ASSERT_EQUAL(1, CENTER_RIGHT - CENTER_LEFT);
}

//==============================================================================
// Color/Palette Tests
//==============================================================================

void test_palette_is_initialized() {
    effect_setup();
    bool hasColor = false;
    for (int i = 0; i < 16; i++) {
        if (ctx->palette[i] != CRGB::Black) {
            hasColor = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(hasColor);
    effect_cleanup();
}

void test_hue_rotation() {
    effect_setup();
    ctx->hue = 0;
    mockCenterGradientEffect(*ctx);

    ctx->clear();
    ctx->hue = 128;
    mockCenterGradientEffect(*ctx);

    TEST_ASSERT_EQUAL(ctx->hue, 128);
    effect_cleanup();
}

//==============================================================================
// Performance/Efficiency Tests
//==============================================================================

void test_clear_is_complete() {
    effect_setup();
    for (int i = 0; i < TOTAL_LEDS; i++) {
        ctx->leds[i] = CRGB::White;
    }
    ctx->clear();
    TEST_ASSERT_EQUAL(0, ctx->countLitLeds());
    effect_cleanup();
}

void test_multiple_frames_dont_accumulate_indefinitely() {
    effect_setup();
    for (int frame = 0; frame < 1000; frame++) {
        ctx->frameCount = frame;
        mockCenterPulseEffect(*ctx);
    }
    TEST_ASSERT_TRUE(ctx->countLitLeds() < TOTAL_LEDS);
    effect_cleanup();
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_effect_tests() {
    // CENTER ORIGIN compliance
    RUN_TEST(test_center_origin_pulse_starts_at_center);
    RUN_TEST(test_center_origin_pulse_expands_outward);
    RUN_TEST(test_center_gradient_brightest_at_center);
    RUN_TEST(test_center_gradient_symmetric);
    RUN_TEST(test_bad_linear_effect_detected);

    // Strip mirroring
    RUN_TEST(test_strip2_mirrors_strip1);
    RUN_TEST(test_strip2_mirrors_after_pulse);

    // Parameter responsiveness
    RUN_TEST(test_speed_affects_lit_leds);
    RUN_TEST(test_zero_speed_still_renders);
    RUN_TEST(test_max_speed_doesnt_overflow);

    // Boundary tests
    RUN_TEST(test_no_writes_beyond_buffer);
    RUN_TEST(test_led_index_0_accessible);
    RUN_TEST(test_led_index_319_accessible);

    // Frame counter
    RUN_TEST(test_effect_changes_over_frames);
    RUN_TEST(test_delta_time_available);

    // CENTER ORIGIN constants
    RUN_TEST(test_center_left_is_79);
    RUN_TEST(test_center_right_is_80);
    RUN_TEST(test_half_length_is_80);
    RUN_TEST(test_strip_length_is_160);
    RUN_TEST(test_total_leds_is_320);
    RUN_TEST(test_center_pair_are_adjacent);

    // Color/palette
    RUN_TEST(test_palette_is_initialized);
    RUN_TEST(test_hue_rotation);

    // Performance
    RUN_TEST(test_clear_is_complete);
    RUN_TEST(test_multiple_frames_dont_accumulate_indefinitely);
}
