/**
 * @file RM690B0Driver.h
 * @brief Low-level QSPI driver for RM690B0 AMOLED display
 *
 * LightwaveOS v2 - AMOLED Test Rig Display Driver
 *
 * Drives the Waveshare ESP32-S3-Touch-AMOLED-2.41 display (600×450, RGB565)
 * via QSPI using the ESP-IDF SPI master driver directly.
 *
 * This is a minimal, zero-dependency driver — no LVGL, no Arduino_GFX.
 * It provides:
 *   - Hardware init (QSPI bus + RM690B0 panel init sequence)
 *   - Window-addressed pixel push (setWindow + pushPixels)
 *   - Brightness control
 *
 * The driver is NOT thread-safe. It must only be called from a single
 * FreeRTOS task (DisplayActor).
 *
 * Reference: LilyGo-AMOLED-Series RM690B0 init code
 */

#pragma once

#include <cstdint>

// ESP-IDF SPI master
#include <driver/spi_master.h>

namespace lightwaveos {
namespace display {

/**
 * @brief RM690B0 QSPI display driver
 *
 * Usage:
 *   RM690B0Driver display;
 *   display.init();
 *   display.setWindow(0, 0, 599, 449);
 *   display.pushPixels(framebuffer, 600 * 450);
 */
class RM690B0Driver {
public:
    RM690B0Driver() = default;
    ~RM690B0Driver();

    // Non-copyable
    RM690B0Driver(const RM690B0Driver&) = delete;
    RM690B0Driver& operator=(const RM690B0Driver&) = delete;

    /**
     * @brief Initialize QSPI bus, reset panel, run init sequence
     * @return true if initialization succeeded
     */
    bool init();

    /**
     * @brief Check if the display has been initialized
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Set the active drawing window (column/row address)
     * @param x0 Start column (0-599)
     * @param y0 Start row (0-449)
     * @param x1 End column (inclusive)
     * @param y1 End row (inclusive)
     */
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    /**
     * @brief Push RGB565 pixel data into the current window
     *
     * Data is sent via QSPI in DMA-sized chunks. The caller must have
     * called setWindow() first.
     *
     * @param data  Pointer to RGB565 pixel data (big-endian)
     * @param count Number of pixels (not bytes)
     */
    void pushPixels(const uint16_t* data, uint32_t count);

    /**
     * @brief Set display brightness
     * @param level 0-255 (0 = off, 255 = max)
     */
    void setBrightness(uint8_t level);

    /**
     * @brief Turn display on or off
     */
    void setDisplayOn(bool on);

    // Display dimensions
    static constexpr uint16_t WIDTH  = 600;
    static constexpr uint16_t HEIGHT = 450;

private:
    /**
     * @brief Send a single command with optional data bytes to the panel
     * @param cmd  RM690B0 register/command byte
     * @param data Pointer to parameter bytes (can be nullptr)
     * @param len  Number of parameter bytes
     */
    void writeCommand(uint8_t cmd, const uint8_t* data = nullptr, uint32_t len = 0);

    /**
     * @brief Hardware reset via AMOLED_RST pin (active LOW pulse)
     */
    void hardwareReset();

    /**
     * @brief Run the full RM690B0 panel init sequence
     */
    void runInitSequence();

    /**
     * @brief Assert/deassert chip select
     */
    void setCS();
    void clrCS();

    spi_device_handle_t m_spi = nullptr;
    bool m_initialized = false;

    /// Maximum bytes per SPI DMA transfer
    static constexpr uint32_t MAX_TRANSFER_BYTES = 32768;
};

} // namespace display
} // namespace lightwaveos
