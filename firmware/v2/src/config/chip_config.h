/**
 * @file chip_config.h
 * @brief Platform detection and chip-specific configuration entry point
 *
 * This header auto-detects the target chip based on ESP-IDF or build flags
 * and includes the appropriate chip-specific configuration.
 */

#pragma once

// ============================================================================
// Platform Auto-Detection
// ============================================================================

// Check ESP-IDF target definitions first (set by ESP-IDF build system)
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    #define CHIP_ESP32_P4 1
    #define CHIP_ESP32_S3 0
    #define CHIP_NAME "ESP32-P4"

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define CHIP_ESP32_S3 1
    #define CHIP_ESP32_P4 0
    #define CHIP_NAME "ESP32-S3"

// Check Arduino core definitions
#elif defined(ARDUINO_ESP32P4_DEV) || defined(ESP32P4)
    #define CHIP_ESP32_P4 1
    #define CHIP_ESP32_S3 0
    #define CHIP_NAME "ESP32-P4"

#elif defined(ARDUINO_ESP32S3_DEV) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    #define CHIP_ESP32_S3 1
    #define CHIP_ESP32_P4 0
    #define CHIP_NAME "ESP32-S3"

// Check explicit build flags (from platformio.ini)
#elif defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    // Already defined via build flag
    #ifndef CHIP_ESP32_S3
        #define CHIP_ESP32_S3 0
    #endif
    #define CHIP_NAME "ESP32-P4"

#elif defined(CHIP_ESP32_S3) && CHIP_ESP32_S3
    // Already defined via build flag
    #ifndef CHIP_ESP32_P4
        #define CHIP_ESP32_P4 0
    #endif
    #define CHIP_NAME "ESP32-S3"

// Default fallback for existing builds (assume S3)
#else
    #define CHIP_ESP32_S3 1
    #define CHIP_ESP32_P4 0
    #define CHIP_NAME "ESP32-S3"
    #warning "No chip target detected, defaulting to ESP32-S3"
#endif

// ============================================================================
// Include Chip-Specific Configuration
// ============================================================================

#if CHIP_ESP32_S3
    #include "chip_esp32s3.h"
#elif CHIP_ESP32_P4
    #include "chip_esp32p4.h"
#endif

// ============================================================================
// Cross-Platform Constants
// ============================================================================

namespace chip {

/**
 * @brief Get the chip name string
 * @return "ESP32-S3" or "ESP32-P4"
 */
inline const char* getChipName() {
    return CHIP_NAME;
}

/**
 * @brief Check if running on ESP32-S3
 */
inline constexpr bool isESP32S3() {
    return CHIP_ESP32_S3;
}

/**
 * @brief Check if running on ESP32-P4
 */
inline constexpr bool isESP32P4() {
    return CHIP_ESP32_P4;
}

} // namespace chip
