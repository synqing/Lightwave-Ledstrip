/**
 * LightwaveOS v2 - HAL LED Driver Unit Tests
 *
 * Tests for the LED Hardware Abstraction Layer including:
 * - LED buffer operations
 * - Center point calculation
 * - Boundary checking
 * - Color scaling
 * - Strip topology
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#endif

#include "../../src/hal/led/ILedDriver.h"
#include <array>
#include <algorithm>

using namespace lightwaveos::hal;

//==============================================================================
// Mock LED Driver Implementation for Testing
//==============================================================================

class MockLedDriver : public ILedDriver {
public:
    static constexpr uint16_t LED_COUNT = 320;
    static constexpr uint16_t CENTER_POINT = 80;

    MockLedDriver()
        : m_initialized(false)
        , m_brightness(255)
        , m_showCount(0)
        , m_lastShowTime(0)
    {
        clear();
    }

    // Lifecycle
    bool init() override {
        m_initialized = true;
        return true;
    }

    void shutdown() override {
        m_initialized = false;
        clear();
    }

    bool isReady() const override {
        return m_initialized;
    }

    // Configuration
    uint16_t getLedCount() const override {
        return LED_COUNT;
    }

    uint16_t getCenterPoint() const override {
        return CENTER_POINT;
    }

    StripTopology getTopology() const override {
        StripTopology topo;
        topo.totalLeds = LED_COUNT;
        topo.ledsPerStrip = 160;
        topo.stripCount = 2;
        topo.centerPoint = CENTER_POINT;
        topo.halfLength = CENTER_POINT;
        return topo;
    }

    // Buffer operations
    void setLed(uint16_t index, RGB color) override {
        if (index < LED_COUNT) {
            m_buffer[index] = color;
        }
    }

    void setLed(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override {
        setLed(index, RGB(r, g, b));
    }

    RGB getLed(uint16_t index) const override {
        if (index < LED_COUNT) {
            return m_buffer[index];
        }
        return RGB::Black();
    }

    void fill(RGB color) override {
        std::fill(m_buffer.begin(), m_buffer.end(), color);
    }

    void fillRange(uint16_t startIndex, uint16_t count, RGB color) override {
        for (uint16_t i = 0; i < count && (startIndex + i) < LED_COUNT; i++) {
            m_buffer[startIndex + i] = color;
        }
    }

    void clear() override {
        fill(RGB::Black());
    }

    RGB* getBuffer() override {
        return m_buffer.data();
    }

    const RGB* getBuffer() const override {
        return m_buffer.data();
    }

    // Output control
    void show() override {
        m_showCount++;
        m_lastShowTime = 9600;  // Simulate ~9.6ms for 320 LEDs
    }

    void setBrightness(uint8_t brightness) override {
        m_brightness = brightness;
    }

    uint8_t getBrightness() const override {
        return m_brightness;
    }

    void setMaxPower(uint8_t volts, uint32_t milliamps) override {
        (void)volts;
        (void)milliamps;
        // No-op for mock
    }

    // Performance
    uint32_t getLastShowTime() const override {
        return m_lastShowTime;
    }

    float getEstimatedFPS() const override {
        if (m_lastShowTime == 0) return 0.0f;
        return 1000000.0f / static_cast<float>(m_lastShowTime);
    }

    // Test helpers
    uint32_t getShowCount() const { return m_showCount; }
    void resetShowCount() { m_showCount = 0; }

private:
    bool m_initialized;
    uint8_t m_brightness;
    uint32_t m_showCount;
    uint32_t m_lastShowTime;
    std::array<RGB, LED_COUNT> m_buffer;
};

//==============================================================================
// Test Fixtures
//==============================================================================

static MockLedDriver* driver = nullptr;

void setUp() {
    driver = new MockLedDriver();
}

void tearDown() {
    delete driver;
    driver = nullptr;
}

//==============================================================================
// RGB Color Tests
//==============================================================================

void test_rgb_default_constructor() {
    RGB color;
    TEST_ASSERT_EQUAL_UINT8(0, color.r);
    TEST_ASSERT_EQUAL_UINT8(0, color.g);
    TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_rgb_component_constructor() {
    RGB color(255, 128, 64);
    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_EQUAL_UINT8(128, color.g);
    TEST_ASSERT_EQUAL_UINT8(64, color.b);
}

void test_rgb_packed_constructor() {
    RGB color(0xFF8040);  // R=255, G=128, B=64
    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_EQUAL_UINT8(128, color.g);
    TEST_ASSERT_EQUAL_UINT8(64, color.b);
}

void test_rgb_to_packed() {
    RGB color(255, 128, 64);
    uint32_t packed = color.toPacked();
    TEST_ASSERT_EQUAL_UINT32(0xFF8040, packed);
}

void test_rgb_equality() {
    RGB color1(255, 128, 64);
    RGB color2(255, 128, 64);
    RGB color3(255, 128, 63);

    TEST_ASSERT_TRUE(color1 == color2);
    TEST_ASSERT_FALSE(color1 == color3);
    TEST_ASSERT_TRUE(color1 != color3);
}

void test_rgb_scaled() {
    RGB color(200, 100, 50);
    RGB scaled = color.scaled(128);  // Scale to 50%

    // Allow for rounding error of ±1
    TEST_ASSERT_INT_WITHIN(1, 100, scaled.r);
    TEST_ASSERT_INT_WITHIN(1, 50, scaled.g);
    TEST_ASSERT_INT_WITHIN(1, 25, scaled.b);
}

void test_rgb_named_colors() {
    RGB red = RGB::Red();
    TEST_ASSERT_EQUAL_UINT8(255, red.r);
    TEST_ASSERT_EQUAL_UINT8(0, red.g);
    TEST_ASSERT_EQUAL_UINT8(0, red.b);

    RGB white = RGB::White();
    TEST_ASSERT_EQUAL_UINT8(255, white.r);
    TEST_ASSERT_EQUAL_UINT8(255, white.g);
    TEST_ASSERT_EQUAL_UINT8(255, white.b);
}

//==============================================================================
// Strip Topology Tests
//==============================================================================

void test_topology_center_point() {
    StripTopology topo = driver->getTopology();
    TEST_ASSERT_EQUAL_UINT16(80, topo.centerPoint);
    TEST_ASSERT_EQUAL_UINT16(320, topo.totalLeds);
    TEST_ASSERT_EQUAL_UINT16(160, topo.ledsPerStrip);
    TEST_ASSERT_EQUAL_UINT8(2, topo.stripCount);
}

void test_topology_is_left_half() {
    StripTopology topo = driver->getTopology();
    TEST_ASSERT_TRUE(topo.isLeftHalf(0));
    TEST_ASSERT_TRUE(topo.isLeftHalf(79));
    TEST_ASSERT_FALSE(topo.isLeftHalf(80));
    TEST_ASSERT_FALSE(topo.isLeftHalf(319));
}

void test_topology_is_right_half() {
    StripTopology topo = driver->getTopology();
    TEST_ASSERT_FALSE(topo.isRightHalf(0));
    TEST_ASSERT_FALSE(topo.isRightHalf(79));
    TEST_ASSERT_TRUE(topo.isRightHalf(80));
    TEST_ASSERT_TRUE(topo.isRightHalf(319));
}

void test_topology_distance_from_center() {
    StripTopology topo = driver->getTopology();

    // Left half distances
    TEST_ASSERT_EQUAL_UINT16(79, topo.distanceFromCenter(0));    // Farthest left
    TEST_ASSERT_EQUAL_UINT16(40, topo.distanceFromCenter(39));
    TEST_ASSERT_EQUAL_UINT16(0, topo.distanceFromCenter(79));    // Adjacent to center

    // Right half distances
    TEST_ASSERT_EQUAL_UINT16(0, topo.distanceFromCenter(80));    // Center point
    TEST_ASSERT_EQUAL_UINT16(40, topo.distanceFromCenter(120));
    TEST_ASSERT_EQUAL_UINT16(239, topo.distanceFromCenter(319)); // Farthest right
}

//==============================================================================
// LED Driver Lifecycle Tests
//==============================================================================

void test_driver_initialization() {
    TEST_ASSERT_FALSE(driver->isReady());

    bool initSuccess = driver->init();
    TEST_ASSERT_TRUE(initSuccess);
    TEST_ASSERT_TRUE(driver->isReady());
}

void test_driver_shutdown() {
    driver->init();
    TEST_ASSERT_TRUE(driver->isReady());

    driver->shutdown();
    TEST_ASSERT_FALSE(driver->isReady());
}

//==============================================================================
// LED Buffer Operation Tests
//==============================================================================

void test_set_single_led() {
    driver->init();

    driver->setLed(10, RGB::Red());
    RGB color = driver->getLed(10);

    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_EQUAL_UINT8(0, color.g);
    TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_set_led_with_components() {
    driver->init();

    driver->setLed(20, 100, 150, 200);
    RGB color = driver->getLed(20);

    TEST_ASSERT_EQUAL_UINT8(100, color.r);
    TEST_ASSERT_EQUAL_UINT8(150, color.g);
    TEST_ASSERT_EQUAL_UINT8(200, color.b);
}

void test_set_led_out_of_bounds() {
    driver->init();

    // Should not crash on out-of-bounds access
    driver->setLed(9999, RGB::Red());

    // Get should return black for out-of-bounds
    RGB color = driver->getLed(9999);
    TEST_ASSERT_TRUE(color == RGB::Black());
}

void test_fill_all_leds() {
    driver->init();

    driver->fill(RGB::Blue());

    // Check first, middle, and last LEDs
    TEST_ASSERT_TRUE(driver->getLed(0) == RGB::Blue());
    TEST_ASSERT_TRUE(driver->getLed(160) == RGB::Blue());
    TEST_ASSERT_TRUE(driver->getLed(319) == RGB::Blue());
}

void test_fill_range() {
    driver->init();
    driver->clear();

    // Fill LEDs 50-99 with green
    driver->fillRange(50, 50, RGB::Green());

    TEST_ASSERT_TRUE(driver->getLed(49) == RGB::Black());
    TEST_ASSERT_TRUE(driver->getLed(50) == RGB::Green());
    TEST_ASSERT_TRUE(driver->getLed(99) == RGB::Green());
    TEST_ASSERT_TRUE(driver->getLed(100) == RGB::Black());
}

void test_clear_resets_buffer() {
    driver->init();

    // Set some LEDs
    driver->fill(RGB::White());
    TEST_ASSERT_TRUE(driver->getLed(0) == RGB::White());

    // Clear should reset all to black
    driver->clear();
    TEST_ASSERT_TRUE(driver->getLed(0) == RGB::Black());
    TEST_ASSERT_TRUE(driver->getLed(319) == RGB::Black());
}

void test_get_buffer_direct_access() {
    driver->init();

    RGB* buffer = driver->getBuffer();
    TEST_ASSERT_NOT_NULL(buffer);

    // Direct buffer manipulation
    buffer[100] = RGB::Magenta();

    RGB color = driver->getLed(100);
    TEST_ASSERT_TRUE(color == RGB::Magenta());
}

//==============================================================================
// Brightness and Output Tests
//==============================================================================

void test_set_brightness() {
    driver->init();

    driver->setBrightness(128);
    TEST_ASSERT_EQUAL_UINT8(128, driver->getBrightness());

    driver->setBrightness(255);
    TEST_ASSERT_EQUAL_UINT8(255, driver->getBrightness());
}

void test_show_increments_counter() {
    driver->init();
    driver->resetShowCount();

    TEST_ASSERT_EQUAL_UINT32(0, driver->getShowCount());

    driver->show();
    TEST_ASSERT_EQUAL_UINT32(1, driver->getShowCount());

    driver->show();
    TEST_ASSERT_EQUAL_UINT32(2, driver->getShowCount());
}

void test_show_time_tracking() {
    driver->init();

    driver->show();
    uint32_t showTime = driver->getLastShowTime();

    TEST_ASSERT_GREATER_THAN_UINT32(0, showTime);
}

void test_estimated_fps() {
    driver->init();

    driver->show();
    float fps = driver->getEstimatedFPS();

    // At 9.6ms per frame, FPS should be around 104
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 104.0f, fps);
}

//==============================================================================
// CENTER ORIGIN Compliance Tests
//==============================================================================

void test_center_point_is_correct() {
    // Verify CENTER_POINT constant matches driver
    TEST_ASSERT_EQUAL_UINT16(80, driver->getCenterPoint());
    TEST_ASSERT_EQUAL_UINT16(MockLedDriver::CENTER_POINT, driver->getCenterPoint());
}

void test_center_origin_pattern() {
    driver->init();
    driver->clear();

    StripTopology topo = driver->getTopology();

    // Create a CENTER ORIGIN pattern: red at center, fading to black at edges
    for (uint16_t i = 0; i < driver->getLedCount(); i++) {
        uint16_t distance = topo.distanceFromCenter(i);
        uint8_t intensity = static_cast<uint8_t>(255 - (distance * 3));  // Fade
        driver->setLed(i, RGB(intensity, 0, 0));
    }

    // Verify center is brightest
    RGB centerColor = driver->getLed(topo.centerPoint);
    RGB edgeColor = driver->getLed(0);

    TEST_ASSERT_GREATER_THAN_UINT8(edgeColor.r, centerColor.r);
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_hal_led_tests() {
    // RGB color tests
    RUN_TEST(test_rgb_default_constructor);
    RUN_TEST(test_rgb_component_constructor);
    RUN_TEST(test_rgb_packed_constructor);
    RUN_TEST(test_rgb_to_packed);
    RUN_TEST(test_rgb_equality);
    RUN_TEST(test_rgb_scaled);
    RUN_TEST(test_rgb_named_colors);

    // Topology tests
    RUN_TEST(test_topology_center_point);
    RUN_TEST(test_topology_is_left_half);
    RUN_TEST(test_topology_is_right_half);
    RUN_TEST(test_topology_distance_from_center);

    // Lifecycle tests
    RUN_TEST(test_driver_initialization);
    RUN_TEST(test_driver_shutdown);

    // Buffer operation tests
    RUN_TEST(test_set_single_led);
    RUN_TEST(test_set_led_with_components);
    RUN_TEST(test_set_led_out_of_bounds);
    RUN_TEST(test_fill_all_leds);
    RUN_TEST(test_fill_range);
    RUN_TEST(test_clear_resets_buffer);
    RUN_TEST(test_get_buffer_direct_access);

    // Output tests
    RUN_TEST(test_set_brightness);
    RUN_TEST(test_show_increments_counter);
    RUN_TEST(test_show_time_tracking);
    RUN_TEST(test_estimated_fps);

    // CENTER ORIGIN tests
    RUN_TEST(test_center_point_is_correct);
    RUN_TEST(test_center_origin_pattern);
}
