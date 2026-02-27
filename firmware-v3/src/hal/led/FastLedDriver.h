// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file FastLedDriver.h
 * @brief FastLED-based implementation of ILedDriver for WS2812 strips
 *
 * LightwaveOS v2 HAL - FastLED Driver
 *
 * This driver provides WS2812/WS2812B support using the FastLED library.
 * It manages dual strip buffers and maps the logical LED index (0-319)
 * to physical strips.
 *
 * Hardware configuration:
 * - Strip 1: GPIO4, 160 LEDs (logical indices 0-159)
 * - Strip 2: GPIO5, 160 LEDs (logical indices 160-319)
 * - CENTER_POINT: 80 (where strips conceptually meet)
 *
 * Thread safety:
 * - All public methods are safe to call from any core
 * - show() uses internal mutex when FREERTOS is available
 * - Buffer modifications are atomic at the LED level
 *
 * Memory usage:
 * - ~960 bytes for LED buffer (320 * 3 bytes)
 * - ~100 bytes for driver state
 * - Total: ~1KB RAM
 *
 * @copyright 2024 LightwaveOS Project
 */

#ifndef LIGHTWAVEOS_HAL_FASTLED_DRIVER_H
#define LIGHTWAVEOS_HAL_FASTLED_DRIVER_H

#include "ILedDriver.h"
#include "LedDriverConfig.h"

// Forward declarations to avoid including FastLED.h in header
// This allows native testing without FastLED dependency
#ifndef NATIVE_BUILD
class CRGB;
class CLEDController;
#endif

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace lightwaveos {
namespace hal {

/**
 * @brief FastLED-based LED driver for WS2812 strips
 *
 * Implements the ILedDriver interface using the FastLED library.
 * Supports multiple physical strips mapped to a single logical buffer.
 *
 * Example:
 * @code
 * LedDriverConfig config;  // Uses v1 defaults
 * FastLedDriver driver(config);
 *
 * if (!driver.init()) {
 *     Serial.println("LED init failed!");
 *     return;
 * }
 *
 * // Set center LED to red (CENTER ORIGIN)
 * driver.setLed(driver.getCenterPoint(), RGB::Red());
 * driver.show();
 * @endcode
 */
class FastLedDriver : public ILedDriver {
public:
    /**
     * @brief Construct driver with configuration
     * @param config LED driver configuration
     */
    explicit FastLedDriver(const LedDriverConfig& config);

    /**
     * @brief Destructor - calls shutdown()
     */
    ~FastLedDriver() override;

    // Non-copyable, non-movable
    FastLedDriver(const FastLedDriver&) = delete;
    FastLedDriver& operator=(const FastLedDriver&) = delete;
    FastLedDriver(FastLedDriver&&) = delete;
    FastLedDriver& operator=(FastLedDriver&&) = delete;

    // ========== Lifecycle ==========

    bool init() override;
    void shutdown() override;
    bool isReady() const override;

    // ========== Configuration ==========

    uint16_t getLedCount() const override;
    uint16_t getCenterPoint() const override;
    StripTopology getTopology() const override;

    // ========== Buffer Operations ==========

    void setLed(uint16_t index, RGB color) override;
    void setLed(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override;
    RGB getLed(uint16_t index) const override;
    void fill(RGB color) override;
    void fillRange(uint16_t startIndex, uint16_t count, RGB color) override;
    void clear() override;
    RGB* getBuffer() override;
    const RGB* getBuffer() const override;

    // ========== Output Control ==========

    void show() override;
    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() const override;
    void setMaxPower(uint8_t volts, uint32_t milliamps) override;

    // ========== Performance ==========

    uint32_t getLastShowTime() const override;
    float getEstimatedFPS() const override;

    // ========== FastLED-Specific ==========

    /**
     * @brief Get controller for a specific strip
     *
     * Allows direct FastLED access for advanced features.
     *
     * @param stripIndex Physical strip index (0 or 1)
     * @return Pointer to CLEDController, or nullptr if invalid
     */
    void* getController(uint8_t stripIndex) const;

    /**
     * @brief Enable/disable temporal dithering
     * @param enable True to enable dithering
     */
    void setDithering(bool enable);

    /**
     * @brief Set color correction
     * @param correction FastLED color correction value (e.g., TypicalLEDStrip)
     */
    void setColorCorrection(uint32_t correction);

    /**
     * @brief Get the physical strip buffer for direct FastLED integration
     *
     * Returns the internal buffer for a specific physical strip.
     * This is useful for legacy effect code that expects separate strip buffers.
     *
     * @param stripIndex Physical strip index (0 or 1)
     * @param[out] buffer Pointer to receive buffer address
     * @param[out] count Pointer to receive LED count
     * @return true if successful, false if invalid strip index
     */
    bool getPhysicalStripBuffer(uint8_t stripIndex,
                                 RGB** buffer,
                                 uint16_t* count) const;

private:
    // Configuration
    LedDriverConfig m_config;

    // Internal LED buffer (unified across all strips)
    RGB* m_buffer;
    uint16_t m_totalLeds;

    // Per-strip buffers for FastLED (point into m_buffer or separate)
    RGB* m_stripBuffers[MAX_STRIPS];
    uint16_t m_stripStarts[MAX_STRIPS];

    // FastLED controller pointers
    void* m_controllers[MAX_STRIPS];

    // State
    bool m_initialized;
    uint8_t m_brightness;
    uint8_t m_powerVoltage;
    uint32_t m_powerMilliamps;

    // Performance tracking
    uint32_t m_lastShowTimeUs;
    uint32_t m_showCount;
    uint32_t m_totalShowTimeUs;

#ifdef ESP32
    // Thread safety
    SemaphoreHandle_t m_mutex;
#endif

    // Private helpers
    void mapLogicalToPhysical(uint16_t logicalIndex,
                              uint8_t& stripIndex,
                              uint16_t& stripOffset) const;
    void initializeFastLED();
    void syncBuffersToFastLED();
};

} // namespace hal
} // namespace lightwaveos

#endif // LIGHTWAVEOS_HAL_FASTLED_DRIVER_H
