#ifndef TAB5_CONFIG_H
#define TAB5_CONFIG_H

#include <Arduino.h>

// ============================================================================
// Feature Flags
// ============================================================================

// Tab5 WiFi: ESP32-C6 co-processor via SDIO with custom pins
// WiFi.setPins() must be called BEFORE WiFi.begin() or M5.begin()
// See: https://github.com/nikthefix/M5stack_Tab5_Arduino_Wifi_Example
#define ENABLE_WIFI 1

// Encoder diagnostics: Enable to log polling performance and delta processing
// Set to 0 to disable (default: disabled for production)
#define ENABLE_ENCODER_DIAGNOSTICS 0

// UI diagnostics: Enable to log UI rendering and screen transitions
// Set to 0 to disable (default: disabled for production)
#define ENABLE_UI_DIAGNOSTICS 0

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

    // I2C timeout (ms) - kept low so 32 transactions/loop stay under WDT (5s)
    constexpr uint16_t TIMEOUT_MS = 50;
}

// Parameter Indices (matches PARAMETER_TABLE and physical encoder layout)
enum class Parameter : uint8_t {
    // Unit A (0-7) - Global LightwaveOS parameters
    // Physical layout: Effect, Palette, Speed, Mood, FadeAmount, Complexity, Variation, Brightness
    Effect = 0,
    Palette = 1,      // Encoder 1 = Palette (wraps 0-74)
    Speed = 2,        // Encoder 2 = Speed
    Mood = 3,         // Encoder 3 = Mood
    FadeAmount = 4,   // Encoder 4 = FadeAmount
    Complexity = 5,   // Encoder 5 = Complexity
    Variation = 6,    // Encoder 6 = Variation
    Brightness = 7,   // Encoder 7 = Brightness (0-255)
    // Unit B (8-15) - Zone parameters
    // Pattern: [Zone N Effect, Zone N Speed/Palette] pairs
    // Note: Encoders 9, 11, 13, 15 toggle between Speed and Palette via button
    Zone0Effect = 8,
    Zone0Speed = 9,      // Also Zone0Palette when button toggled
    Zone1Effect = 10,
    Zone1Speed = 11,     // Also Zone1Palette when button toggled
    Zone2Effect = 12,
    Zone2Speed = 13,     // Also Zone2Palette when button toggled
    Zone3Effect = 14,
    Zone3Speed = 15,     // Also Zone3Palette when button toggled
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
    // Unit A (0-7) - Global parameters
    constexpr uint8_t EFFECT_MIN = 0;
    constexpr uint8_t EFFECT_MAX = 103;  // 104 effect slots (0-103) - matches v2 RendererActor::MAX_EFFECTS

    constexpr uint8_t PALETTE_MIN = 0;
    constexpr uint8_t PALETTE_MAX = 74;  // v2 has 75 palettes (0-74)

    constexpr uint8_t SPEED_MIN = 1;
    constexpr uint8_t SPEED_MAX = 100;

    constexpr uint8_t MOOD_MIN = 0;
    constexpr uint8_t MOOD_MAX = 255;

    constexpr uint8_t FADEAMOUNT_MIN = 0;
    constexpr uint8_t FADEAMOUNT_MAX = 255;

    constexpr uint8_t BRIGHTNESS_MIN = 0;
    constexpr uint8_t BRIGHTNESS_MAX = 255;

    constexpr uint8_t COMPLEXITY_MIN = 0;
    constexpr uint8_t COMPLEXITY_MAX = 255;

    constexpr uint8_t VARIATION_MIN = 0;
    constexpr uint8_t VARIATION_MAX = 255;

    // Zone Speed (Unit B, encoders 9, 11, 13, 15) - same range as global speed
    constexpr uint8_t ZONE_SPEED_MIN = 1;
    constexpr uint8_t ZONE_SPEED_MAX = 100;
    
    // Zone Palette (Unit B, encoders 9, 11, 13, 15 when toggled) - same as global palette
    constexpr uint8_t ZONE_PALETTE_MIN = 0;
    constexpr uint8_t ZONE_PALETTE_MAX = 74;  // v2 has 75 palettes (0-74)

    // Unit B (8-15) - Zone parameters
    // Zone Effect: 0-99 (wraps around for continuous scrolling) - matches v2 EXPECTED_EFFECT_COUNT
    constexpr uint8_t ZONE_EFFECT_MIN = 0;
    constexpr uint8_t ZONE_EFFECT_MAX = 103;  // 104 effect slots (0-103)

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
    // Unit A (0-7) - Global parameters
    constexpr uint8_t EFFECT = 0;
    constexpr uint8_t PALETTE = 0;
    constexpr uint8_t SPEED = 25;
    constexpr uint8_t MOOD = 0;
    constexpr uint8_t FADEAMOUNT = 0;
    constexpr uint8_t BRIGHTNESS = 128;
    constexpr uint8_t COMPLEXITY = 128;
    constexpr uint8_t VARIATION = 0;

    // Unit B (8-15) - Zone parameters
    // Zone Effect defaults to 0 (first effect)
    // Zone Speed defaults to 25 (same as global speed)
    // Zone Palette defaults to 0 (when toggled to palette mode)
    constexpr uint8_t ZONE0_EFFECT = 0;
    constexpr uint8_t ZONE0_SPEED = 25;
    constexpr uint8_t ZONE0_PALETTE = 0;
    constexpr uint8_t ZONE1_EFFECT = 0;
    constexpr uint8_t ZONE1_SPEED = 25;
    constexpr uint8_t ZONE1_PALETTE = 0;
    constexpr uint8_t ZONE2_EFFECT = 0;
    constexpr uint8_t ZONE2_SPEED = 25;
    constexpr uint8_t ZONE2_PALETTE = 0;
    constexpr uint8_t ZONE3_EFFECT = 0;
    constexpr uint8_t ZONE3_SPEED = 25;
    constexpr uint8_t ZONE3_PALETTE = 0;
}

// Parameter Names (for display/debugging)
namespace ParamName {
    // Unit A (0-7) - Global parameters
    constexpr const char* EFFECT = "Effect";
    constexpr const char* BRIGHTNESS = "Brightness";
    constexpr const char* PALETTE = "Palette";
    constexpr const char* SPEED = "Speed";
    constexpr const char* MOOD = "Mood";
    constexpr const char* FADEAMOUNT = "Fade Amt";
    constexpr const char* COMPLEXITY = "Complexity";
    constexpr const char* VARIATION = "Variation";

    // Unit B (8-15) - Zone parameters
    // Short names for display (max 8 chars to fit cell layout)
    constexpr const char* ZONE0_EFFECT = "Z0 Eff";
    constexpr const char* ZONE0_SPEED = "Z0 Spd";
    constexpr const char* ZONE0_PALETTE = "Z0 Pal";
    constexpr const char* ZONE1_EFFECT = "Z1 Eff";
    constexpr const char* ZONE1_SPEED = "Z1 Spd";
    constexpr const char* ZONE1_PALETTE = "Z1 Pal";
    constexpr const char* ZONE2_EFFECT = "Z2 Eff";
    constexpr const char* ZONE2_SPEED = "Z2 Spd";
    constexpr const char* ZONE2_PALETTE = "Z2 Pal";
    constexpr const char* ZONE3_EFFECT = "Z3 Eff";
    constexpr const char* ZONE3_SPEED = "Z3 Spd";
    constexpr const char* ZONE3_PALETTE = "Z3 Pal";
}

// Helper: Get parameter name
inline const char* getParameterName(Parameter param) {
    switch (param) {
        // Unit A (0-7)
        case Parameter::Effect:      return ParamName::EFFECT;
        case Parameter::Brightness:  return ParamName::BRIGHTNESS;
        case Parameter::Palette:     return ParamName::PALETTE;
        case Parameter::Speed:       return ParamName::SPEED;
        case Parameter::Mood:        return ParamName::MOOD;
        case Parameter::FadeAmount:  return ParamName::FADEAMOUNT;
        case Parameter::Complexity:  return ParamName::COMPLEXITY;
        case Parameter::Variation:   return ParamName::VARIATION;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect: return ParamName::ZONE0_EFFECT;
        case Parameter::Zone0Speed:  return ParamName::ZONE0_SPEED;
        case Parameter::Zone1Effect: return ParamName::ZONE1_EFFECT;
        case Parameter::Zone1Speed:  return ParamName::ZONE1_SPEED;
        case Parameter::Zone2Effect: return ParamName::ZONE2_EFFECT;
        case Parameter::Zone2Speed:  return ParamName::ZONE2_SPEED;
        case Parameter::Zone3Effect: return ParamName::ZONE3_EFFECT;
        case Parameter::Zone3Speed:  return ParamName::ZONE3_SPEED;
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
        case Parameter::Mood:        return ParamRange::MOOD_MIN;
        case Parameter::FadeAmount:  return ParamRange::FADEAMOUNT_MIN;
        case Parameter::Complexity:  return ParamRange::COMPLEXITY_MIN;
        case Parameter::Variation:   return ParamRange::VARIATION_MIN;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect: return ParamRange::ZONE_EFFECT_MIN;
        case Parameter::Zone0Speed:  return ParamRange::ZONE_SPEED_MIN;
        case Parameter::Zone1Effect: return ParamRange::ZONE_EFFECT_MIN;
        case Parameter::Zone1Speed:  return ParamRange::ZONE_SPEED_MIN;
        case Parameter::Zone2Effect: return ParamRange::ZONE_EFFECT_MIN;
        case Parameter::Zone2Speed:  return ParamRange::ZONE_SPEED_MIN;
        case Parameter::Zone3Effect: return ParamRange::ZONE_EFFECT_MIN;
        case Parameter::Zone3Speed:  return ParamRange::ZONE_SPEED_MIN;
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
        case Parameter::Mood:        return ParamRange::MOOD_MAX;
        case Parameter::FadeAmount:  return ParamRange::FADEAMOUNT_MAX;
        case Parameter::Complexity:  return ParamRange::COMPLEXITY_MAX;
        case Parameter::Variation:   return ParamRange::VARIATION_MAX;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect: return ParamRange::ZONE_EFFECT_MAX;
        case Parameter::Zone0Speed:  return ParamRange::ZONE_SPEED_MAX;
        case Parameter::Zone1Effect: return ParamRange::ZONE_EFFECT_MAX;
        case Parameter::Zone1Speed:  return ParamRange::ZONE_SPEED_MAX;
        case Parameter::Zone2Effect: return ParamRange::ZONE_EFFECT_MAX;
        case Parameter::Zone2Speed:  return ParamRange::ZONE_SPEED_MAX;
        case Parameter::Zone3Effect: return ParamRange::ZONE_EFFECT_MAX;
        case Parameter::Zone3Speed:  return ParamRange::ZONE_SPEED_MAX;
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
        case Parameter::Mood:        return ParamDefault::MOOD;
        case Parameter::FadeAmount:  return ParamDefault::FADEAMOUNT;
        case Parameter::Complexity:  return ParamDefault::COMPLEXITY;
        case Parameter::Variation:   return ParamDefault::VARIATION;
        // Unit B (8-15) - Zone parameters
        case Parameter::Zone0Effect: return ParamDefault::ZONE0_EFFECT;
        case Parameter::Zone0Speed:  return ParamDefault::ZONE0_SPEED;
        case Parameter::Zone1Effect: return ParamDefault::ZONE1_EFFECT;
        case Parameter::Zone1Speed:  return ParamDefault::ZONE1_SPEED;
        case Parameter::Zone2Effect: return ParamDefault::ZONE2_EFFECT;
        case Parameter::Zone2Speed:  return ParamDefault::ZONE2_SPEED;
        case Parameter::Zone3Effect: return ParamDefault::ZONE3_EFFECT;
        case Parameter::Zone3Speed:  return ParamDefault::ZONE3_SPEED;
        default:                     return 128;
    }
}

#endif // TAB5_CONFIG_H
