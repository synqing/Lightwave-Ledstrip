/**
 * @file chip_esp32s3.h
 * @brief ESP32-S3 specific configuration and constants
 *
 * This file contains all chip-specific constants for ESP32-S3,
 * including pin assignments, peripheral capabilities, and memory constraints.
 */

#pragma once

#include <cstdint>

namespace chip {

// ============================================================================
// Hardware Capabilities
// ============================================================================

/// CPU maximum frequency in MHz
constexpr uint32_t CPU_FREQ_MHZ = 240;

/// Number of CPU cores
constexpr uint8_t CPU_CORES = 2;

/// Core architecture
constexpr const char* CPU_ARCH = "Xtensa LX7";

/// Has integrated WiFi radio
constexpr bool HAS_INTEGRATED_WIFI = true;

/// Has integrated Bluetooth
constexpr bool HAS_BLUETOOTH = true;

/// Has Ethernet MAC
constexpr bool HAS_ETHERNET = false;

/// Number of RMT channels available
constexpr uint8_t RMT_CHANNELS = 8;

/// Number of GPIO pins
constexpr uint8_t GPIO_COUNT = 45;

// ============================================================================
// Memory Configuration
// ============================================================================

/// Internal SRAM size in KB
constexpr uint32_t SRAM_SIZE_KB = 384;

/// Maximum PSRAM size in MB (if populated)
constexpr uint32_t PSRAM_MAX_MB = 8;

/// Recommended minimum free heap for stable operation
constexpr uint32_t MIN_FREE_HEAP_KB = 40;

// ============================================================================
// Default GPIO Pin Assignments
// ============================================================================

namespace gpio {

    // LED Strip pins (WS2812 via RMT)
    constexpr uint8_t LED_STRIP1_DATA = 4;
    constexpr uint8_t LED_STRIP2_DATA = 5;

    // I2S Audio (SPH0645 microphone)
    constexpr uint8_t I2S_BCLK = 14;    ///< Bit clock
    constexpr uint8_t I2S_DOUT = 13;    ///< Data out (mic output)
    constexpr uint8_t I2S_LRCL = 12;    ///< Left/Right clock (word select)

    // I2C (M5ROTATE8 encoder)
    constexpr uint8_t I2C_SDA = 17;
    constexpr uint8_t I2C_SCL = 18;

} // namespace gpio

// ============================================================================
// I2S Configuration
// ============================================================================

namespace i2s {

    /// I2S driver type for this chip
    constexpr const char* DRIVER_TYPE = "legacy";

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

    /// Renderer task core assignment (time-critical)
    constexpr uint8_t RENDERER_CORE = 1;

    /// Audio task core assignment
    constexpr uint8_t AUDIO_CORE = 0;

    /// Network task core assignment
    constexpr uint8_t NETWORK_CORE = 0;

    /// Default stack size multiplier (RISC-V needs more stack)
    constexpr float STACK_MULTIPLIER = 1.0f;

} // namespace task

// ============================================================================
// Performance Targets
// ============================================================================

namespace perf {

    /// Target frame rate in FPS
    constexpr uint16_t TARGET_FPS = 120;

    /// Frame time budget in microseconds
    constexpr uint32_t FRAME_BUDGET_US = 8333;  // 1000000 / 120

    /// Audio hop rate in Hz
    constexpr uint16_t AUDIO_HOP_RATE = 50;

    /// Audio latency target in milliseconds
    constexpr uint16_t AUDIO_LATENCY_MS = 20;

} // namespace perf

} // namespace chip
