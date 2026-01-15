#pragma once
// ============================================================================
// EspHal - Hardware Abstraction Layer for ESP32 and Simulator
// ============================================================================
// Provides unified interface for hardware-specific operations:
// - Heap monitoring
// - Battery/power management
// - Time functions
// - Logging
// ============================================================================

#include <cstdint>
#include <cstdarg>

namespace EspHal {

/**
 * @brief Get current free heap in bytes
 */
uint32_t getFreeHeap();

/**
 * @brief Get minimum free heap since boot
 */
uint32_t getMinFreeHeap();

/**
 * @brief Get largest free block available
 */
uint32_t getMaxAllocHeap();

/**
 * @brief Get battery level (0-100, or -1 if unknown)
 */
int8_t getBatteryLevel();

/**
 * @brief Check if device is charging
 */
bool isCharging();

/**
 * @brief Get battery voltage in volts (or -1.0 if unknown)
 */
float getBatteryVoltage();

/**
 * @brief Get milliseconds since boot
 */
uint32_t millis();

/**
 * @brief Log formatted message (printf-style)
 * In hardware: outputs to Serial
 * In simulator: outputs to stdout
 */
void log(const char* format, ...);

/**
 * @brief Log formatted message with va_list (for internal use)
 */
void logV(const char* format, va_list args);

}  // namespace EspHal

