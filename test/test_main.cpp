#include <unity.h>

// Test basic LED array initialization
// Hardware: 2 strips × 160 LEDs = 320 total
void test_led_array_size() {
    TEST_ASSERT_EQUAL(320, HardwareConfig::NUM_LEDS);
}

// Test palette count
void test_palette_count() {
    TEST_ASSERT_EQUAL(33, PaletteManager::getPaletteCount());
}

void setup() {
    UNITY_BEGIN();
    
    RUN_TEST(test_led_array_size);
    RUN_TEST(test_palette_count);
    
    UNITY_END();
}

void loop() {
    // Empty
}