/**
 * @file ILedDriver.h
 * @brief Hardware abstraction interface for LED strip control
 *
 * Provides a platform-agnostic interface for WS2812 LED control,
 * supporting both ESP32-S3 (FastLED) and ESP32-P4 (FastLED or RMT5).
 */

#pragma once

#include <cstdint>
#include <FastLED.h>

namespace lightwaveos {
namespace hal {

/**
 * @brief LED strip configuration
 */
struct LedStripConfig {
    uint16_t ledCount = 160;        ///< Number of LEDs in strip
    uint8_t dataPin = 4;            ///< Data GPIO pin
    uint8_t brightness = 128;       ///< Initial brightness (0-255)
    bool reverseOrder = false;      ///< Reverse LED addressing
    CRGB colorCorrection = TypicalLEDStrip; ///< Color correction profile
};

/**
 * @brief LED driver statistics
 */
struct LedDriverStats {
    uint32_t frameCount = 0;        ///< Total frames rendered
    uint32_t lastShowUs = 0;        ///< Last show() duration in microseconds
    uint32_t avgShowUs = 0;         ///< Average show() duration
    uint32_t maxShowUs = 0;         ///< Maximum show() duration
    uint8_t currentBrightness = 0;  ///< Current brightness setting
};

/**
 * @brief Abstract interface for LED strip control
 *
 * Platform-specific implementations:
 * - ESP32-S3: LedDriver_S3 (FastLED RMT4)
 * - ESP32-P4: LedDriver_P4 (FastLED RMT5 or ESP-IDF led_strip)
 */
class ILedDriver {
public:
    virtual ~ILedDriver() = default;

    /**
     * @brief Initialize LED hardware for a single strip
     * @param config Strip configuration
     * @return true if initialization succeeded
     */
    virtual bool init(const LedStripConfig& config) = 0;

    /**
     * @brief Initialize LED hardware for dual strips
     * @param config1 First strip configuration
     * @param config2 Second strip configuration
     * @return true if initialization succeeded
     */
    virtual bool initDual(const LedStripConfig& config1, const LedStripConfig& config2) = 0;

    /**
     * @brief Deinitialize and release hardware resources
     */
    virtual void deinit() = 0;

    /**
     * @brief Get pointer to the LED buffer for strip 0
     * @return Pointer to CRGB buffer
     */
    virtual CRGB* getBuffer() = 0;

    /**
     * @brief Get pointer to LED buffer for specified strip
     * @param stripIndex Strip index (0 or 1)
     * @return Pointer to CRGB buffer, nullptr if invalid
     */
    virtual CRGB* getBuffer(uint8_t stripIndex) = 0;

    /**
     * @brief Get total LED count across all strips
     * @return Total number of LEDs
     */
    virtual uint16_t getTotalLedCount() const = 0;

    /**
     * @brief Get LED count for a specific strip
     * @param stripIndex Strip index (0 or 1)
     * @return Number of LEDs in strip
     */
    virtual uint16_t getLedCount(uint8_t stripIndex) const = 0;

    /**
     * @brief Push buffer contents to LEDs
     */
    virtual void show() = 0;

    /**
     * @brief Set global brightness
     * @param brightness Brightness value (0-255)
     */
    virtual void setBrightness(uint8_t brightness) = 0;

    /**
     * @brief Get current brightness
     * @return Current brightness value (0-255)
     */
    virtual uint8_t getBrightness() const = 0;

    /**
     * @brief Set maximum power consumption
     * @param volts Supply voltage (typically 5)
     * @param milliamps Maximum current in mA
     */
    virtual void setMaxPower(uint8_t volts, uint16_t milliamps) = 0;

    /**
     * @brief Clear all LEDs to black
     * @param show If true, immediately push to LEDs
     */
    virtual void clear(bool show = false) = 0;

    /**
     * @brief Fill all LEDs with a solid color
     * @param color Color to fill
     * @param show If true, immediately push to LEDs
     */
    virtual void fill(CRGB color, bool show = false) = 0;

    /**
     * @brief Set a single pixel color
     * @param index LED index
     * @param color Color to set
     */
    virtual void setPixel(uint16_t index, CRGB color) = 0;

    /**
     * @brief Check if driver is initialized
     * @return true if initialized and ready
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Get driver statistics
     * @return Current statistics
     */
    virtual const LedDriverStats& getStats() const = 0;

    /**
     * @brief Reset statistics counters
     */
    virtual void resetStats() = 0;
};

} // namespace hal
} // namespace lightwaveos
