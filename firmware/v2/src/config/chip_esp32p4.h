/**
 * @file chip_esp32p4.h
 * @brief ESP32-P4 specific configuration and constants
 *
 * This file contains all chip-specific constants for ESP32-P4,
 * including pin assignments, peripheral capabilities, and memory constraints.
 *
 * Key differences from ESP32-S3:
 * - RISC-V architecture (vs Xtensa)
 * - 400 MHz CPU (vs 240 MHz)
 * - No integrated WiFi/Bluetooth
 * - Integrated Ethernet MAC
 * - 768 KB SRAM (vs 384 KB)
 * - 4 RMT channels (vs 8)
 */

#pragma once

#include <cstdint>

namespace chip {

// ============================================================================
// Hardware Capabilities
// ============================================================================

/// CPU maximum frequency in MHz
constexpr uint32_t CPU_FREQ_MHZ = 400;

/// Number of CPU cores (HP core only, LP core runs at 40MHz)
constexpr uint8_t CPU_CORES = 2;

/// Core architecture
constexpr const char* CPU_ARCH = "RISC-V HP";

/// Has integrated WiFi radio (P4 does NOT have WiFi)
constexpr bool HAS_INTEGRATED_WIFI = false;

/// Has integrated Bluetooth (P4 does NOT have Bluetooth)
constexpr bool HAS_BLUETOOTH = false;

/// Has Ethernet MAC (P4 has 10/100 Mbps Ethernet)
constexpr bool HAS_ETHERNET = true;

/// Number of RMT channels available
constexpr uint8_t RMT_CHANNELS = 4;

/// Number of GPIO pins
constexpr uint8_t GPIO_COUNT = 55;

// ============================================================================
// Memory Configuration
// ============================================================================

/// Internal SRAM size in KB (P4 has 2x more than S3)
constexpr uint32_t SRAM_SIZE_KB = 768;

/// Maximum PSRAM size in MB (if populated)
constexpr uint32_t PSRAM_MAX_MB = 32;

/// Recommended minimum free heap for stable operation
constexpr uint32_t MIN_FREE_HEAP_KB = 60;

// ============================================================================
// Default GPIO Pin Assignments
// ============================================================================

namespace gpio {

    // LED Strip pins (WS2812 via RMT)
    // Note: These should be validated against P4 Function EV Board pinout
    constexpr uint8_t LED_STRIP1_DATA = 4;
    constexpr uint8_t LED_STRIP2_DATA = 5;

    // I2S Audio (SPH0645 microphone)
    // Note: Pins may need adjustment based on P4 board layout
    constexpr uint8_t I2S_BCLK = 14;    ///< Bit clock
    constexpr uint8_t I2S_DOUT = 13;    ///< Data out (mic output)
    constexpr uint8_t I2S_LRCL = 12;    ///< Left/Right clock (word select)

    // I2C (M5ROTATE8 encoder)
    constexpr uint8_t I2C_SDA = 17;
    constexpr uint8_t I2C_SCL = 18;

    // Ethernet PHY (P4 Function EV Board typical pinout)
    constexpr uint8_t ETH_MDC = 23;
    constexpr uint8_t ETH_MDIO = 24;
    constexpr uint8_t ETH_PHY_RST = 5;
    constexpr uint8_t ETH_PHY_POWER = 12;

    // USB (P4 has USB 2.0 HS)
    // Note: GPIO 24, 25 are commonly used for USB - avoid for LEDs
    constexpr uint8_t USB_DP = 24;
    constexpr uint8_t USB_DM = 25;

} // namespace gpio

// ============================================================================
// I2S Configuration
// ============================================================================

namespace i2s {

    /// I2S driver type for this chip (P4 uses new std mode driver)
    constexpr const char* DRIVER_TYPE = "std";

    /// I2S port number
    constexpr uint8_t PORT = 0;

    /// Sample rate in Hz
    constexpr uint32_t SAMPLE_RATE = 12800;

    /// DMA buffer count
    constexpr uint8_t DMA_BUFFER_COUNT = 4;

    /// DMA buffer size in samples
    constexpr uint16_t DMA_BUFFER_SAMPLES = 512;

} // namespace i2s

// ============================================================================
// FreeRTOS Task Configuration
// ============================================================================

namespace task {

    /// Renderer task core assignment (time-critical, HP core)
    constexpr uint8_t RENDERER_CORE = 1;

    /// Audio task core assignment (HP core)
    constexpr uint8_t AUDIO_CORE = 0;

    /// Network task core assignment (could use LP core for lower priority)
    constexpr uint8_t NETWORK_CORE = 0;

    /// Stack size multiplier for RISC-V (needs ~12-25% more stack)
    constexpr float STACK_MULTIPLIER = 1.2f;

} // namespace task

// ============================================================================
// Performance Targets
// ============================================================================

namespace perf {

    /// Target frame rate in FPS (P4 can potentially do higher)
    constexpr uint16_t TARGET_FPS = 120;

    /// Frame time budget in microseconds
    constexpr uint32_t FRAME_BUDGET_US = 8333;  // 1000000 / 120

    /// Audio hop rate in Hz
    constexpr uint16_t AUDIO_HOP_RATE = 50;

    /// Audio latency target in milliseconds
    constexpr uint16_t AUDIO_LATENCY_MS = 20;

} // namespace perf

// ============================================================================
// Ethernet Configuration (P4 specific)
// ============================================================================

namespace ethernet {

    /// PHY address (depends on board design)
    constexpr uint8_t PHY_ADDR = 1;

    /// PHY type (LAN8720 is common)
    constexpr const char* PHY_TYPE = "LAN8720";

    /// Enable RMII interface
    constexpr bool USE_RMII = true;

} // namespace ethernet

} // namespace chip
