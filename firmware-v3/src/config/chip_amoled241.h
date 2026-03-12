/**
 * @file chip_amoled241.h
 * @brief Pin configuration for Waveshare ESP32-S3-Touch-AMOLED-2.41 test rig
 *
 * LightwaveOS v2 - Headless Test Rig Configuration
 *
 * This board has an RM690B0 600×450 AMOLED driven via QSPI, plus an
 * FT3168 capacitive touch controller.  It is used as a headless test rig:
 * no physical LED strips are required (WS2812 is unidirectional — RMT
 * generates the waveform identically with nothing connected).
 *
 * Waveshare 2.41" AMOLED Pin Map (from working voice-feasibility harness):
 *   QSPI Display: CS=9, CLK=10, D0=11, D1=12, D2=13, D3=14
 *   AMOLED_RST: GPIO 21
 *   I2S (SPH0645): BCLK=38, WS/LRCL=40, DIN/DOUT=39
 *   Touch I2C:  SDA=47, SCL=48, RST=3
 *   I2C Expander: SDA=15, SCL=16 (CH422G)
 *   Free GPIOs: 7, 8, 18, 41, 42, 43, 44, 45, 46
 *
 * @note GPIO 4 and 5 are SD card pins on this board but we don't use SD.
 *       They're repurposed as WS2812 strip data pins (firmware defaults).
 */

#pragma once

#include <cstdint>

namespace chip {

// ============================================================================
// Hardware Capabilities (same silicon as other S3 boards)
// ============================================================================

constexpr uint32_t CPU_FREQ_MHZ = 240;
constexpr uint8_t CPU_CORES = 2;
constexpr const char* CPU_ARCH = "Xtensa LX7";
constexpr bool HAS_INTEGRATED_WIFI = true;
constexpr bool HAS_BLUETOOTH = true;
constexpr bool HAS_ETHERNET = false;
constexpr uint8_t RMT_CHANNELS = 8;
constexpr uint8_t GPIO_COUNT = 45;

// ============================================================================
// Memory Configuration
// ============================================================================

constexpr uint32_t SRAM_SIZE_KB = 384;
constexpr uint32_t PSRAM_MAX_MB = 8;
constexpr uint32_t MIN_FREE_HEAP_KB = 40;

// ============================================================================
// GPIO Pin Assignments — AMOLED 2.41" Test Rig
// ============================================================================

namespace gpio {

    // LED Strip pins (WS2812 via RMT) — GPIO 4/5 are SD card pins on this
    // board but SD is not used, so they map to firmware defaults.
#ifdef K1_LED_STRIP1_DATA
    constexpr uint8_t LED_STRIP1_DATA = K1_LED_STRIP1_DATA;
#else
    constexpr uint8_t LED_STRIP1_DATA = 4;
#endif
#ifdef K1_LED_STRIP2_DATA
    constexpr uint8_t LED_STRIP2_DATA = K1_LED_STRIP2_DATA;
#else
    constexpr uint8_t LED_STRIP2_DATA = 5;
#endif

    // I2S Audio (SPH0645 external mic)
    // BCLK=38 — no conflict with display (AMOLED_RST is GPIO 21)
#ifdef K1_I2S_BCLK
    constexpr uint8_t I2S_BCLK = K1_I2S_BCLK;
#else
    constexpr uint8_t I2S_BCLK = 38;    ///< Bit clock (no conflict — AMOLED_RST is GPIO 21)
#endif
#ifdef K1_I2S_DOUT
    constexpr uint8_t I2S_DOUT = K1_I2S_DOUT;
#else
    constexpr uint8_t I2S_DOUT = 39;    ///< Data out (mic output)
#endif
#ifdef K1_I2S_LRCL
    constexpr uint8_t I2S_LRCL = K1_I2S_LRCL;
#else
    constexpr uint8_t I2S_LRCL = 40;    ///< Left/Right clock (word select)
#endif

    // I2C — uses the board's CH422G expander bus
#ifdef K1_I2C_SDA
    constexpr uint8_t I2C_SDA = K1_I2C_SDA;
#else
    constexpr uint8_t I2C_SDA = 15;
#endif
#ifdef K1_I2C_SCL
    constexpr uint8_t I2C_SCL = K1_I2C_SCL;
#else
    constexpr uint8_t I2C_SCL = 16;
#endif

    // No TTP223 touch button on this board
    constexpr int8_t TTP223 = -1;

    // ========================================================================
    // AMOLED Display (RM690B0 via QSPI)
    // ========================================================================

    constexpr uint8_t AMOLED_CS   = 9;
    constexpr uint8_t AMOLED_CLK  = 10;
    constexpr uint8_t AMOLED_D0   = 11;
    constexpr uint8_t AMOLED_D1   = 12;
    constexpr uint8_t AMOLED_D2   = 13;
    constexpr uint8_t AMOLED_D3   = 14;
    constexpr uint8_t AMOLED_RST  = 21;   // Hardware reset (confirmed by voice-feasibility harness)

    // Touch controller (FT3168) on separate I2C bus
    constexpr uint8_t TOUCH_SDA   = 47;
    constexpr uint8_t TOUCH_SCL   = 48;
    constexpr uint8_t TOUCH_RST   = 3;

} // namespace gpio

// ============================================================================
// AMOLED Display Configuration
// ============================================================================

namespace display {

    constexpr uint16_t WIDTH  = 600;
    constexpr uint16_t HEIGHT = 450;
    constexpr uint32_t PIXEL_COUNT = WIDTH * HEIGHT;  // 270,000

    /// QSPI frequency — RM690B0 supports up to 80 MHz
    constexpr uint32_t SPI_FREQ_HZ = 40000000;  // 40 MHz — matches working harness (80 MHz unreliable for init)

    /// Pixel format: RGB565 (2 bytes per pixel)
    constexpr uint8_t BYTES_PER_PIXEL = 2;

    /// DMA transfer buffer size (in pixels) — 4800 pixels = 9600 bytes
    /// Balances DMA efficiency vs SRAM pressure
    constexpr uint16_t DMA_BUF_PIXELS = 4800;

} // namespace display

// ============================================================================
// I2S Configuration
// ============================================================================

namespace i2s {

    constexpr const char* DRIVER_TYPE = "legacy";
    constexpr uint8_t PORT = 0;

#ifndef SAMPLE_RATE
    constexpr uint32_t SAMPLE_RATE = 12800;
#endif

    constexpr uint8_t DMA_BUFFER_COUNT = 4;
    constexpr uint16_t DMA_BUFFER_SAMPLES = 512;

} // namespace i2s

// ============================================================================
// FreeRTOS Task Configuration
// ============================================================================

namespace task {

    constexpr uint8_t RENDERER_CORE = 1;
    constexpr uint8_t AUDIO_CORE = 0;
    constexpr uint8_t NETWORK_CORE = 0;
    constexpr float STACK_MULTIPLIER = 1.0f;

} // namespace task

// ============================================================================
// Performance Targets
// ============================================================================

namespace perf {

    constexpr uint16_t TARGET_FPS = 120;
    constexpr uint32_t FRAME_BUDGET_US = 8333;
    constexpr uint16_t AUDIO_HOP_RATE = 50;
    constexpr uint16_t AUDIO_LATENCY_MS = 20;

} // namespace perf

} // namespace chip
