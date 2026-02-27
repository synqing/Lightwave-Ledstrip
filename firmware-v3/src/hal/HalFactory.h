/**
 * @file HalFactory.h
 * @brief Hardware Abstraction Layer factory for compile-time chip selection
 *
 * This header provides type aliases that resolve to platform-specific
 * implementations at compile time based on CHIP_ESP32_S3 or CHIP_ESP32_P4 defines.
 */

#pragma once

#include "config/chip_config.h"

// Include interface headers
#include "hal/interface/IAudioCapture.h"
#include "hal/interface/ILedDriver.h"
#include "hal/interface/INetworkDriver.h"

// Include platform-specific implementations based on target chip
#if defined(CHIP_ESP32_S3) && CHIP_ESP32_S3

    // ESP32-S3 implementations
    // Note: Audio/network drivers are still pending for HAL abstraction
    // #include "hal/esp32s3/AudioCapture_S3.h"
    #include "hal/esp32s3/LedDriver_S3.h"
    // #include "hal/esp32s3/WiFiDriver_S3.h"

namespace lightwaveos {
namespace hal {

    // Type aliases for S3 platform
    // using AudioCapture = AudioCapture_S3;
    using LedDriver = LedDriver_S3;
    // using NetworkDriver = WiFiDriver_S3;

    // Platform info
    constexpr const char* PLATFORM_NAME = "ESP32-S3";
    constexpr bool HAS_INTEGRATED_WIFI = true;
    constexpr bool HAS_ETHERNET = false;
    constexpr uint32_t CPU_FREQ_MHZ = 240;

} // namespace hal
} // namespace lightwaveos

#elif defined(CHIP_ESP32_P4) && CHIP_ESP32_P4

    // ESP32-P4 implementations
    // #include "hal/esp32p4/AudioCapture_P4.h"
    
    // LED Driver selection: Custom parallel RMT driver (recommended) or FastLED fallback
    #ifndef USE_FASTLED_DRIVER
    #define USE_FASTLED_DRIVER 0  // 0 = Custom RMT (parallel), 1 = FastLED (sequential)
    #endif

    #if USE_FASTLED_DRIVER
    #include "hal/esp32p4/LedDriver_P4.h"
    #else
        #include "hal/esp32p4/LedDriver_P4_RMT.h"
    #endif
    // #include "hal/esp32p4/EthernetDriver_P4.h"

namespace lightwaveos {
namespace hal {

    // Type aliases for P4 platform
    // using AudioCapture = AudioCapture_P4;
    #if USE_FASTLED_DRIVER
    using LedDriver = LedDriver_P4;          // FastLED-based (sequential)
    #else
        using LedDriver = LedDriver_P4_RMT;      // Custom RMT (parallel, recommended)
    #endif
    // using NetworkDriver = EthernetDriver_P4;

    // Platform info
    constexpr const char* PLATFORM_NAME = "ESP32-P4";
    constexpr bool HAS_INTEGRATED_WIFI = false;
    constexpr bool HAS_ETHERNET = false;
    constexpr uint32_t CPU_FREQ_MHZ = 400;

} // namespace hal
} // namespace lightwaveos

#else

    // Compilation will fail if no platform is defined
    // This is intentional - we need explicit platform selection
    #if !defined(CHIP_ESP32_S3) && !defined(CHIP_ESP32_P4)
        // Default to S3 for backward compatibility during transition
        #define CHIP_ESP32_S3 1
        #define CHIP_ESP32_P4 0

namespace lightwaveos {
namespace hal {
    constexpr const char* PLATFORM_NAME = "ESP32-S3";
    constexpr bool HAS_INTEGRATED_WIFI = true;
    constexpr bool HAS_ETHERNET = false;
    constexpr uint32_t CPU_FREQ_MHZ = 240;
} // namespace hal
} // namespace lightwaveos

    #endif

#endif

namespace lightwaveos {
namespace hal {

/**
 * @brief Get human-readable platform name
 * @return Platform string (e.g., "ESP32-S3" or "ESP32-P4")
 */
inline const char* getPlatformName() {
    return PLATFORM_NAME;
}

/**
 * @brief Check if platform has integrated WiFi
 * @return true if WiFi is built-in (S3, C3, C6), false otherwise (P4)
 */
inline constexpr bool hasIntegratedWiFi() {
    return HAS_INTEGRATED_WIFI;
}

/**
 * @brief Check if platform has Ethernet MAC
 * @return true if Ethernet is available (P4)
 */
inline constexpr bool hasEthernet() {
    return HAS_ETHERNET;
}

/**
 * @brief Get CPU frequency in MHz
 * @return CPU clock in MHz
 */
inline constexpr uint32_t getCpuFreqMHz() {
    return CPU_FREQ_MHZ;
}

} // namespace hal
} // namespace lightwaveos
