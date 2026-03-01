/**
 * @file LedDriverConfig.h
 * @brief Configuration structures for LED drivers
 *
 * LightwaveOS v2 HAL - LED Driver Configuration
 *
 * This file contains configuration structures used to initialize
 * LED drivers. Configuration is separate from the driver interface
 * to allow compile-time constants and PROGMEM storage.
 *
 * @copyright 2024 LightwaveOS Project
 */

#ifndef LIGHTWAVEOS_HAL_LED_DRIVER_CONFIG_H
#define LIGHTWAVEOS_HAL_LED_DRIVER_CONFIG_H

#include <cstdint>
#include "config/chip_config.h"

namespace lightwaveos {
namespace hal {

/**
 * @brief Color byte ordering for LED protocols
 *
 * Different LED chips use different color orderings.
 * WS2812/WS2812B use GRB, APA102 uses BGR, etc.
 */
enum class ColorOrder : uint8_t {
    RGB = 0,    ///< Standard RGB ordering
    RBG = 1,    ///< Red, Blue, Green
    GRB = 2,    ///< Green, Red, Blue (WS2812, WS2812B)
    GBR = 3,    ///< Green, Blue, Red
    BRG = 4,    ///< Blue, Red, Green (APA102)
    BGR = 5     ///< Blue, Green, Red
};

/**
 * @brief LED strip type/protocol
 */
enum class LedType : uint8_t {
    WS2812 = 0,     ///< WS2812/WS2812B (800kHz, GRB)
    WS2811 = 1,     ///< WS2811 (400kHz, RGB)
    SK6812 = 2,     ///< SK6812 RGB (800kHz, GRB)
    SK6812_RGBW = 3, ///< SK6812 RGBW (4 bytes per LED)
    APA102 = 4,     ///< APA102/SK9822 (SPI, 2-wire)
    NEOPIXEL = 5    ///< Generic NeoPixel (alias for WS2812)
};

/**
 * @brief Physical strip configuration
 *
 * Describes a single physical LED strip connected to the ESP32.
 * Each strip has its own data pin and LED count.
 */
struct StripConfig {
    uint8_t dataPin;        ///< GPIO pin for data line
    uint8_t clockPin;       ///< GPIO pin for clock (0 if not used, e.g., WS2812)
    uint16_t ledCount;      ///< Number of LEDs on this strip
    ColorOrder colorOrder;  ///< Color byte ordering
    LedType ledType;        ///< LED protocol/type
    bool reversed;          ///< True if strip is wired in reverse direction

    /**
     * @brief Default constructor for WS2812 strip
     */
    constexpr StripConfig()
        : dataPin(0)
        , clockPin(0)
        , ledCount(0)
        , colorOrder(ColorOrder::GRB)
        , ledType(LedType::WS2812)
        , reversed(false)
    {}

    /**
     * @brief Constructor for WS2812-type strip (single data wire)
     * @param pin Data GPIO pin
     * @param count LED count
     * @param order Color ordering (default GRB for WS2812)
     */
    constexpr StripConfig(uint8_t pin, uint16_t count,
                          ColorOrder order = ColorOrder::GRB)
        : dataPin(pin)
        , clockPin(0)
        , ledCount(count)
        , colorOrder(order)
        , ledType(LedType::WS2812)
        , reversed(false)
    {}

    /**
     * @brief Constructor with full parameters
     */
    constexpr StripConfig(uint8_t dPin, uint8_t cPin, uint16_t count,
                          ColorOrder order, LedType type, bool rev = false)
        : dataPin(dPin)
        , clockPin(cPin)
        , ledCount(count)
        , colorOrder(order)
        , ledType(type)
        , reversed(rev)
    {}
};

/**
 * @brief Maximum number of physical strips supported
 */
constexpr uint8_t MAX_STRIPS = 4;

/**
 * @brief Complete LED driver configuration
 *
 * This structure contains all configuration needed to initialize
 * an LED driver for the LightwaveOS dual-strip system.
 */
struct LedDriverConfig {
    // Strip configurations
    StripConfig strips[MAX_STRIPS]; ///< Individual strip configs
    uint8_t stripCount;             ///< Number of strips in use (1-4)

    // Center origin configuration
    uint16_t centerPoint;           ///< LED index for CENTER ORIGIN (79 for v1 compat, 80 for v2)
    bool centerOriginEnabled;       ///< Whether CENTER ORIGIN mode is active

    // Brightness limits
    uint8_t defaultBrightness;      ///< Initial brightness (0-255)
    uint8_t maxBrightness;          ///< Maximum allowed brightness

    // Power management
    uint8_t powerVoltage;           ///< Supply voltage (typically 5V)
    uint16_t powerMilliamps;        ///< Max current per strip (mA)
    uint16_t totalPowerBudget;      ///< Total power budget in mA

    // Timing
    uint16_t targetFPS;             ///< Target frame rate
    bool enableDithering;           ///< Enable temporal dithering

    /**
     * @brief Default constructor with LightwaveOS v1 defaults
     *
     * Creates a dual-strip WS2812 configuration matching the
     * existing v1 hardware setup.
     */
    constexpr LedDriverConfig()
        : strips{
            StripConfig(chip::gpio::LED_STRIP1_DATA, 160, ColorOrder::GRB),   // Strip 1 (top)
            StripConfig(chip::gpio::LED_STRIP2_DATA, 160, ColorOrder::GRB),   // Strip 2 (bottom)
            StripConfig(),                          // Unused
            StripConfig()                           // Unused
          }
        , stripCount(2)
        , centerPoint(79)                           // LED 79/80 split (v1 compatible)
        , centerOriginEnabled(true)
        , defaultBrightness(96)                     // Safe default
        , maxBrightness(255)                        // No power clamping; full range
        , powerVoltage(5)
        , powerMilliamps(1500)                      // 1.5A per strip
        , totalPowerBudget(3000)                    // 3A total
        , targetFPS(120)
        , enableDithering(true)
    {}

    /**
     * @brief Get total LED count across all strips
     */
    constexpr uint16_t getTotalLedCount() const {
        uint16_t total = 0;
        for (uint8_t i = 0; i < stripCount; ++i) {
            total += strips[i].ledCount;
        }
        return total;
    }

    /**
     * @brief Get LED count for a specific strip
     * @param stripIndex Strip index (0 to stripCount-1)
     * @return LED count, or 0 if invalid index
     */
    constexpr uint16_t getStripLedCount(uint8_t stripIndex) const {
        if (stripIndex >= stripCount) return 0;
        return strips[stripIndex].ledCount;
    }

    /**
     * @brief Get starting index for a strip in the unified buffer
     * @param stripIndex Strip index (0 to stripCount-1)
     * @return Starting LED index, or 0 if invalid
     */
    constexpr uint16_t getStripStartIndex(uint8_t stripIndex) const {
        if (stripIndex >= stripCount) return 0;
        uint16_t start = 0;
        for (uint8_t i = 0; i < stripIndex; ++i) {
            start += strips[i].ledCount;
        }
        return start;
    }

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    constexpr bool isValid() const {
        if (stripCount == 0 || stripCount > MAX_STRIPS) return false;
        if (getTotalLedCount() == 0) return false;
        if (centerPoint > getTotalLedCount()) return false;
        if (defaultBrightness > maxBrightness) return false;
        return true;
    }
};

/**
 * @brief Predefined configuration for LightwaveOS v1 hardware
 *
 * Dual WS2812 strips on GPIO4 and GPIO5, 160 LEDs each.
 * CENTER_POINT at 80 (where the two strips conceptually meet).
 */
constexpr LedDriverConfig LIGHTWAVEOS_V1_CONFIG = LedDriverConfig();

/**
 * @brief Single strip configuration for testing
 */
constexpr LedDriverConfig createSingleStripConfig(uint8_t pin, uint16_t ledCount) {
    LedDriverConfig config;
    config.strips[0] = StripConfig(pin, ledCount, ColorOrder::GRB);
    config.stripCount = 1;
    config.centerPoint = ledCount / 2;
    return config;
}

} // namespace hal
} // namespace lightwaveos

#endif // LIGHTWAVEOS_HAL_LED_DRIVER_CONFIG_H
