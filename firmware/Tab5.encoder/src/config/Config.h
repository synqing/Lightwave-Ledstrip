#ifndef TAB5_CONFIG_H
#define TAB5_CONFIG_H

#include <Arduino.h>

// ============================================================================
// Feature Flags
// ============================================================================

// ==========================================================================
// Tab5 WiFi: ESP32-C6 co-processor via SDIO with custom pins
// ==========================================================================
// ⚠️ KNOWN REGRESSION - WiFi DISABLED (January 2026)
//
// PROBLEM: Arduino-ESP32's pre-compiled ESP-Hosted library uses WRONG SDIO
// pin mappings. The library defaults conflict with Tab5's actual hardware:
//
//   Tab5 Actual Pins:       Library Default Pins:
//     CLK = GPIO 12           CLK = GPIO 14 ❌
//     CMD = GPIO 13           CMD = GPIO 15 ❌
//     D0  = GPIO 11           D0  = GPIO 2  ❌
//     ...etc
//
// SYMPTOM: Boot crash with "sdmmc_init_ocr: send_op_cond (1) returned 0x107"
//
// BLOCKER: WiFi.setPins() requires Arduino-ESP32 v3.3.0+ which is NOT YET
// RELEASED. The pin mapping is compiled into the library binary.
//
// WORKAROUND: Set ENABLE_WIFI=0 to disable WiFi entirely.
// Tab5 operates as local-only encoder controller without network connectivity.
//
// FUTURE FIX: When Arduino-ESP32 v3.3.0+ releases, re-enable and call:
//   WiFi.setPins(TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD, TAB5_WIFI_SDIO_D0,
//                TAB5_WIFI_SDIO_D1, TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3);
//
// Reference: https://github.com/nikthefix/M5stack_Tab5_Arduino_Wifi_Example
// ==========================================================================
#define ENABLE_WIFI 0  // DISABLED - See regression note above

// Tab5 WiFi SDIO pin definitions (ESP32-C6 co-processor)
#define TAB5_WIFI_SDIO_CLK   12
#define TAB5_WIFI_SDIO_CMD   13
#define TAB5_WIFI_SDIO_D0    11
#define TAB5_WIFI_SDIO_D1    10
#define TAB5_WIFI_SDIO_D2    9
#define TAB5_WIFI_SDIO_D3    8
#define TAB5_WIFI_SDIO_RST   15

// ============================================================================
// Tab5.encoder Configuration
// ============================================================================
// M5Stack Tab5 (ESP32-P4) with dual M5ROTATE8 units
// Unit A: Grove Port.A (GPIO 53/54) - Parameters 0-7
// Unit B: Custom port (GPIO 49/50) - Parameters 8-15
// ============================================================================

// I2C Configuration
namespace I2C {
    // Primary I2C: Grove Port.A (Unit A - encoders 0-7)
    // Tab5 Grove Port.A uses GPIO 53/54 for external I2C
    // These pins are obtained dynamically via M5.Ex_I2C.getSDA()/getSCL()
    // but we define constants here for reference and fallback
    constexpr uint8_t EXT_SDA_PIN = 53;
    constexpr uint8_t EXT_SCL_PIN = 54;

    // Secondary I2C: Custom port on G49/G50 (Unit B - encoders 8-15)
    constexpr uint8_t EXT2_SDA_PIN = 49;
    constexpr uint8_t EXT2_SCL_PIN = 50;

    // M5ROTATE8 I2C address (same for both units, different buses)
    constexpr uint8_t ROTATE8_ADDRESS = 0x41;

    // Conservative frequency (100kHz) for stability
    // The M5ROTATE8 library supports up to 400kHz, but we start safe
    constexpr uint32_t FREQ_HZ = 100000;

    // I2C timeout (ms) - larger than default 50ms for robustness
    constexpr uint16_t TIMEOUT_MS = 200;
}

// Parameter Indices (matches LightwaveOS)
enum class Parameter : uint8_t {
    // Unit A (0-7) - Core LightwaveOS parameters
    Effect = 0,
    Brightness = 1,
    Palette = 2,
    Speed = 3,
    Intensity = 4,
    Saturation = 5,
    Complexity = 6,
    Variation = 7,
    // Unit B (8-15) - Zone parameters
    // Pattern: [Zone N Effect, Zone N Brightness] pairs
    Zone0Effect = 8,
    Zone0Brightness = 9,
    Zone1Effect = 10,
    Zone1Brightness = 11,
    Zone2Effect = 12,
    Zone2Brightness = 13,
    Zone3Effect = 14,
    Zone3Brightness = 15,
    COUNT = 16
};

// Zone parameter helper functions
namespace ZoneParam {
    // Check if parameter index is a zone parameter (8-15)
    constexpr bool isZoneParameter(uint8_t index) {
        return index >= 8 && index <= 15;
    }

    // Get zone ID (0-3) from parameter index
    constexpr uint8_t getZoneId(uint8_t index) {
        return (index - 8) / 2;
    }

    // Check if parameter is a zone effect selector
    constexpr bool isZoneEffect(uint8_t index) {
        return isZoneParameter(index) && ((index - 8) % 2 == 0);
    }

    // Check if parameter is a zone brightness control
    constexpr bool isZoneBrightness(uint8_t index) {
        return isZoneParameter(index) && ((index - 8) % 2 == 1);
    }

    // Get encoder index for a zone's effect parameter
    constexpr uint8_t getZoneEffectIndex(uint8_t zoneId) {
        return 8 + (zoneId * 2);
    }

    // Get encoder index for a zone's brightness parameter
    constexpr uint8_t getZoneBrightnessIndex(uint8_t zoneId) {
        return 9 + (zoneId * 2);
    }
}

// Parameter Ranges
namespace ParamRange {
    // Unit A (0-7) - Core parameters
    constexpr uint8_t EFFECT_MIN = 0;
    constexpr uint8_t EFFECT_MAX = 95;

    constexpr uint8_t BRIGHTNESS_MIN = 0;
    constexpr uint8_t BRIGHTNESS_MAX = 255;

    constexpr uint8_t PALETTE_MIN = 0;
    constexpr uint8_t PALETTE_MAX = 63;

    constexpr uint8_t SPEED_MIN = 1;
    constexpr uint8_t SPEED_MAX = 100;

    constexpr uint8_t INTENSITY_MIN = 0;
    constexpr uint8_t INTENSITY_MAX = 255;

    constexpr uint8_t SATURATION_MIN = 0;
    constexpr uint8_t SATURATION_MAX = 255;

    constexpr uint8_t COMPLEXITY_MIN = 0;
    constexpr uint8_t COMPLEXITY_MAX = 255;

    constexpr uint8_t VARIATION_MIN = 0;
    constexpr uint8_t VARIATION_MAX = 255;

    // Unit B (8-15) - Zone parameters
    // Zone Effect: 0-95 (wraps around for continuous scrolling)
    constexpr uint8_t ZONE_EFFECT_MIN = 0;
    constexpr uint8_t ZONE_EFFECT_MAX = 95;

    // Zone Brightness: 0-255 (clamped, no wrap)
    constexpr uint8_t ZONE_BRIGHTNESS_MIN = 0;
    constexpr uint8_t ZONE_BRIGHTNESS_MAX = 255;

    // Aliases for individual parameters (for backward compatibility)
    constexpr uint8_t ZONE0_EFFECT_MIN = ZONE_EFFECT_MIN;
    constexpr uint8_t ZONE0_EFFECT_MAX = ZONE_EFFECT_MAX;
    constexpr uint8_t ZONE0_BRIGHTNESS_MIN = ZONE_BRIGHTNESS_MIN;
    constexpr uint8_t ZONE0_BRIGHTNESS_MAX = ZONE_BRIGHTNESS_MAX;

    constexpr uint8_t ZONE1_EFFECT_MIN = ZONE_EFFECT_MIN;
    constexpr uint8_t ZONE1_EFFECT_MAX = ZONE_EFFECT_MAX;
    constexpr uint8_t ZONE1_BRIGHTNESS_MIN = ZONE_BRIGHTNESS_MIN;
    constexpr uint8_t ZONE1_BRIGHTNESS_MAX = ZONE_BRIGHTNESS_MAX;

    constexpr uint8_t ZONE2_EFFECT_MIN = ZONE_EFFECT_MIN;
    constexpr uint8_t ZONE2_EFFECT_MAX = ZONE_EFFECT_MAX;
    constexpr uint8_t ZONE2_BRIGHTNESS_MIN = ZONE_BRIGHTNESS_MIN;
    constexpr uint8_t ZONE2_BRIGHTNESS_MAX = ZONE_BRIGHTNESS_MAX;

    constexpr uint8_t ZONE3_EFFECT_MIN = ZONE_EFFECT_MIN;
    constexpr uint8_t ZONE3_EFFECT_MAX = ZONE_EFFECT_MAX;
    constexpr uint8_t ZONE3_BRIGHTNESS_MIN = ZONE_BRIGHTNESS_MIN;
    constexpr uint8_t ZONE3_BRIGHTNESS_MAX = ZONE_BRIGHTNESS_MAX;
}

// Parameter Default Values
namespace ParamDefault {
    // Unit A (0-7) - Core parameters
    constexpr uint8_t EFFECT = 0;
    constexpr uint8_t BRIGHTNESS = 128;
    constexpr uint8_t PALETTE = 0;
    constexpr uint8_t SPEED = 25;
    constexpr uint8_t INTENSITY = 128;
    constexpr uint8_t SATURATION = 255;
    constexpr uint8_t COMPLEXITY = 128;
    constexpr uint8_t VARIATION = 0;

    // Unit B (8-15) - Zone parameters
    // Zone Effect defaults to 0 (first effect)
    // Zone Brightness defaults to 128 (50% brightness)
    constexpr uint8_t ZONE0_EFFECT = 0;
    constexpr uint8_t ZONE0_BRIGHTNESS = 128;
    constexpr uint8_t ZONE1_EFFECT = 0;
    constexpr uint8_t ZONE1_BRIGHTNESS = 128;
    constexpr uint8_t ZONE2_EFFECT = 0;
    constexpr uint8_t ZONE2_BRIGHTNESS = 128;
    constexpr uint8_t ZONE3_EFFECT = 0;
    constexpr uint8_t ZONE3_BRIGHTNESS = 128;
}

// Parameter Names (for display/debugging)
namespace ParamName {
    // Unit A (0-7) - Core parameters
    constexpr const char* EFFECT = "Effect";
    constexpr const char* BRIGHTNESS = "Brightness";
    constexpr const char* PALETTE = "Palette";
    constexpr const char* SPEED = "Speed";
    constexpr const char* INTENSITY = "Intensity";
    constexpr const char* SATURATION = "Saturation";
    constexpr const char* COMPLEXITY = "Complexity";
    constexpr const char* VARIATION = "Variation";

    // Unit B (8-15) - Zone parameters
    // Short names for display (max 8 chars to fit cell layout)
    constexpr const char* ZONE0_EFFECT = "Z0 Eff";
    constexpr const char* ZONE0_BRIGHTNESS = "Z0 Bri";
    constexpr const char* ZONE1_EFFECT = "Z1 Eff";
    constexpr const char* ZONE1_BRIGHTNESS = "Z1 Bri";
    constexpr const char* ZONE2_EFFECT = "Z2 Eff";
    constexpr const char* ZONE2_BRIGHTNESS = "Z2 Bri";
    constexpr const char* ZONE3_EFFECT = "Z3 Eff";
    constexpr const char* ZONE3_BRIGHTNESS = "Z3 Bri";
}

// Helper: Get parameter name
inline const char* getParameterName(Parameter param) {
    switch (param) {
        // Unit A (0-7)
        case Parameter::Effect:      return ParamName::EFFECT;
        case Parameter::Brightness:  return ParamName::BRIGHTNESS;
        case Parameter::Palette:     return ParamName::PALETTE;
        case Parameter::Speed:       return ParamName::SPEED;
        case Parameter::Intensity:   return ParamName::INTENSITY;
        case Parameter::Saturation:  return ParamName::SATURATION;
        case Parameter::Complexity:  return ParamName::COMPLEXITY;
        case Parameter::Variation:   return ParamName::VARIATION;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect:     return ParamName::ZONE0_EFFECT;
        case Parameter::Zone0Brightness: return ParamName::ZONE0_BRIGHTNESS;
        case Parameter::Zone1Effect:     return ParamName::ZONE1_EFFECT;
        case Parameter::Zone1Brightness: return ParamName::ZONE1_BRIGHTNESS;
        case Parameter::Zone2Effect:     return ParamName::ZONE2_EFFECT;
        case Parameter::Zone2Brightness: return ParamName::ZONE2_BRIGHTNESS;
        case Parameter::Zone3Effect:     return ParamName::ZONE3_EFFECT;
        case Parameter::Zone3Brightness: return ParamName::ZONE3_BRIGHTNESS;
        default:                     return "Unknown";
    }
}

// Helper: Get parameter min value
inline uint8_t getParameterMin(Parameter param) {
    switch (param) {
        // Unit A (0-7)
        case Parameter::Effect:      return ParamRange::EFFECT_MIN;
        case Parameter::Brightness:  return ParamRange::BRIGHTNESS_MIN;
        case Parameter::Palette:     return ParamRange::PALETTE_MIN;
        case Parameter::Speed:       return ParamRange::SPEED_MIN;
        case Parameter::Intensity:   return ParamRange::INTENSITY_MIN;
        case Parameter::Saturation:  return ParamRange::SATURATION_MIN;
        case Parameter::Complexity:  return ParamRange::COMPLEXITY_MIN;
        case Parameter::Variation:   return ParamRange::VARIATION_MIN;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect:     return ParamRange::ZONE0_EFFECT_MIN;
        case Parameter::Zone0Brightness: return ParamRange::ZONE0_BRIGHTNESS_MIN;
        case Parameter::Zone1Effect:     return ParamRange::ZONE1_EFFECT_MIN;
        case Parameter::Zone1Brightness: return ParamRange::ZONE1_BRIGHTNESS_MIN;
        case Parameter::Zone2Effect:     return ParamRange::ZONE2_EFFECT_MIN;
        case Parameter::Zone2Brightness: return ParamRange::ZONE2_BRIGHTNESS_MIN;
        case Parameter::Zone3Effect:     return ParamRange::ZONE3_EFFECT_MIN;
        case Parameter::Zone3Brightness: return ParamRange::ZONE3_BRIGHTNESS_MIN;
        default:                     return 0;
    }
}

// Helper: Get parameter max value
inline uint8_t getParameterMax(Parameter param) {
    switch (param) {
        // Unit A (0-7)
        case Parameter::Effect:      return ParamRange::EFFECT_MAX;
        case Parameter::Brightness:  return ParamRange::BRIGHTNESS_MAX;
        case Parameter::Palette:     return ParamRange::PALETTE_MAX;
        case Parameter::Speed:       return ParamRange::SPEED_MAX;
        case Parameter::Intensity:   return ParamRange::INTENSITY_MAX;
        case Parameter::Saturation:  return ParamRange::SATURATION_MAX;
        case Parameter::Complexity:  return ParamRange::COMPLEXITY_MAX;
        case Parameter::Variation:   return ParamRange::VARIATION_MAX;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect:     return ParamRange::ZONE0_EFFECT_MAX;
        case Parameter::Zone0Brightness: return ParamRange::ZONE0_BRIGHTNESS_MAX;
        case Parameter::Zone1Effect:     return ParamRange::ZONE1_EFFECT_MAX;
        case Parameter::Zone1Brightness: return ParamRange::ZONE1_BRIGHTNESS_MAX;
        case Parameter::Zone2Effect:     return ParamRange::ZONE2_EFFECT_MAX;
        case Parameter::Zone2Brightness: return ParamRange::ZONE2_BRIGHTNESS_MAX;
        case Parameter::Zone3Effect:     return ParamRange::ZONE3_EFFECT_MAX;
        case Parameter::Zone3Brightness: return ParamRange::ZONE3_BRIGHTNESS_MAX;
        default:                     return 255;
    }
}

// Helper: Get parameter default value
inline uint8_t getParameterDefault(Parameter param) {
    switch (param) {
        // Unit A (0-7)
        case Parameter::Effect:      return ParamDefault::EFFECT;
        case Parameter::Brightness:  return ParamDefault::BRIGHTNESS;
        case Parameter::Palette:     return ParamDefault::PALETTE;
        case Parameter::Speed:       return ParamDefault::SPEED;
        case Parameter::Intensity:   return ParamDefault::INTENSITY;
        case Parameter::Saturation:  return ParamDefault::SATURATION;
        case Parameter::Complexity:  return ParamDefault::COMPLEXITY;
        case Parameter::Variation:   return ParamDefault::VARIATION;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect:     return ParamDefault::ZONE0_EFFECT;
        case Parameter::Zone0Brightness: return ParamDefault::ZONE0_BRIGHTNESS;
        case Parameter::Zone1Effect:     return ParamDefault::ZONE1_EFFECT;
        case Parameter::Zone1Brightness: return ParamDefault::ZONE1_BRIGHTNESS;
        case Parameter::Zone2Effect:     return ParamDefault::ZONE2_EFFECT;
        case Parameter::Zone2Brightness: return ParamDefault::ZONE2_BRIGHTNESS;
        case Parameter::Zone3Effect:     return ParamDefault::ZONE3_EFFECT;
        case Parameter::Zone3Brightness: return ParamDefault::ZONE3_BRIGHTNESS;
        default:                     return 128;
    }
}

#endif // TAB5_CONFIG_H
