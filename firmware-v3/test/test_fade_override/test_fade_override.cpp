#include <unity.h>

#include "effects/FadeOverride.h"
#include "utils/BenchRegistry.h"

using lightwaveos::effects::persistence::fadeToBlackByDt;
using lightwaveos::effects::persistence::fadeToBlackByGated;

namespace {

void resetFadeToggle() {
    lightwaveos::bench::g_bench_effect_fade_to_black = true;
}

void assertPixelEquals(const CRGB& expected, const CRGB& actual) {
    TEST_ASSERT_EQUAL_UINT8(expected.r, actual.r);
    TEST_ASSERT_EQUAL_UINT8(expected.g, actual.g);
    TEST_ASSERT_EQUAL_UINT8(expected.b, actual.b);
}

void test_fade_to_black_gated_preserves_pixels_when_toggle_disabled() {
    CRGB leds[2] = {
        CRGB(160, 80, 40),
        CRGB(15, 120, 240),
    };
    const CRGB before[2] = {leds[0], leds[1]};

    lightwaveos::bench::g_bench_effect_fade_to_black = false;
    fadeToBlackByGated(leds, 2, 128, 1.0f / 60.0f);

    assertPixelEquals(before[0], leds[0]);
    assertPixelEquals(before[1], leds[1]);
}

void test_fade_to_black_gated_matches_dt_fade_when_toggle_enabled() {
    CRGB actual[2] = {
        CRGB(160, 80, 40),
        CRGB(15, 120, 240),
    };
    CRGB expected[2] = {actual[0], actual[1]};

    lightwaveos::bench::g_bench_effect_fade_to_black = true;
    fadeToBlackByDt(expected, 2, 64, 1.0f / 60.0f);
    fadeToBlackByGated(actual, 2, 64, 1.0f / 60.0f);

    assertPixelEquals(expected[0], actual[0]);
    assertPixelEquals(expected[1], actual[1]);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fade_to_black_gated_preserves_pixels_when_toggle_disabled);
    resetFadeToggle();
    RUN_TEST(test_fade_to_black_gated_matches_dt_fade_when_toggle_enabled);
    resetFadeToggle();
    return UNITY_END();
}
