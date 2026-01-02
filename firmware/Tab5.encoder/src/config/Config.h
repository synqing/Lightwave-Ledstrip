#ifndef TAB5_CONFIG_H
#define TAB5_CONFIG_H

#include <Arduino.h>

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
    // Unit B (8-15) - Placeholder parameters (TBD)
    Param8 = 8,
    Param9 = 9,
    Param10 = 10,
    Param11 = 11,
    Param12 = 12,
    Param13 = 13,
    Param14 = 14,
    Param15 = 15,
    COUNT = 16
};

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

    // Unit B (8-15) - Placeholder parameters (all 0-255 for now)
    constexpr uint8_t PARAM8_MIN = 0;
    constexpr uint8_t PARAM8_MAX = 255;

    constexpr uint8_t PARAM9_MIN = 0;
    constexpr uint8_t PARAM9_MAX = 255;

    constexpr uint8_t PARAM10_MIN = 0;
    constexpr uint8_t PARAM10_MAX = 255;

    constexpr uint8_t PARAM11_MIN = 0;
    constexpr uint8_t PARAM11_MAX = 255;

    constexpr uint8_t PARAM12_MIN = 0;
    constexpr uint8_t PARAM12_MAX = 255;

    constexpr uint8_t PARAM13_MIN = 0;
    constexpr uint8_t PARAM13_MAX = 255;

    constexpr uint8_t PARAM14_MIN = 0;
    constexpr uint8_t PARAM14_MAX = 255;

    constexpr uint8_t PARAM15_MIN = 0;
    constexpr uint8_t PARAM15_MAX = 255;
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

    // Unit B (8-15) - Placeholder parameters (default 128)
    constexpr uint8_t PARAM8 = 128;
    constexpr uint8_t PARAM9 = 128;
    constexpr uint8_t PARAM10 = 128;
    constexpr uint8_t PARAM11 = 128;
    constexpr uint8_t PARAM12 = 128;
    constexpr uint8_t PARAM13 = 128;
    constexpr uint8_t PARAM14 = 128;
    constexpr uint8_t PARAM15 = 128;
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

    // Unit B (8-15) - Placeholder parameters
    constexpr const char* PARAM8 = "Param 8";
    constexpr const char* PARAM9 = "Param 9";
    constexpr const char* PARAM10 = "Param 10";
    constexpr const char* PARAM11 = "Param 11";
    constexpr const char* PARAM12 = "Param 12";
    constexpr const char* PARAM13 = "Param 13";
    constexpr const char* PARAM14 = "Param 14";
    constexpr const char* PARAM15 = "Param 15";
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
        // Unit B (8-15)
        case Parameter::Param8:      return ParamName::PARAM8;
        case Parameter::Param9:      return ParamName::PARAM9;
        case Parameter::Param10:     return ParamName::PARAM10;
        case Parameter::Param11:     return ParamName::PARAM11;
        case Parameter::Param12:     return ParamName::PARAM12;
        case Parameter::Param13:     return ParamName::PARAM13;
        case Parameter::Param14:     return ParamName::PARAM14;
        case Parameter::Param15:     return ParamName::PARAM15;
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
        // Unit B (8-15)
        case Parameter::Param8:      return ParamRange::PARAM8_MIN;
        case Parameter::Param9:      return ParamRange::PARAM9_MIN;
        case Parameter::Param10:     return ParamRange::PARAM10_MIN;
        case Parameter::Param11:     return ParamRange::PARAM11_MIN;
        case Parameter::Param12:     return ParamRange::PARAM12_MIN;
        case Parameter::Param13:     return ParamRange::PARAM13_MIN;
        case Parameter::Param14:     return ParamRange::PARAM14_MIN;
        case Parameter::Param15:     return ParamRange::PARAM15_MIN;
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
        // Unit B (8-15)
        case Parameter::Param8:      return ParamRange::PARAM8_MAX;
        case Parameter::Param9:      return ParamRange::PARAM9_MAX;
        case Parameter::Param10:     return ParamRange::PARAM10_MAX;
        case Parameter::Param11:     return ParamRange::PARAM11_MAX;
        case Parameter::Param12:     return ParamRange::PARAM12_MAX;
        case Parameter::Param13:     return ParamRange::PARAM13_MAX;
        case Parameter::Param14:     return ParamRange::PARAM14_MAX;
        case Parameter::Param15:     return ParamRange::PARAM15_MAX;
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
        // Unit B (8-15)
        case Parameter::Param8:      return ParamDefault::PARAM8;
        case Parameter::Param9:      return ParamDefault::PARAM9;
        case Parameter::Param10:     return ParamDefault::PARAM10;
        case Parameter::Param11:     return ParamDefault::PARAM11;
        case Parameter::Param12:     return ParamDefault::PARAM12;
        case Parameter::Param13:     return ParamDefault::PARAM13;
        case Parameter::Param14:     return ParamDefault::PARAM14;
        case Parameter::Param15:     return ParamDefault::PARAM15;
        default:                     return 128;
    }
}

#endif // TAB5_CONFIG_H
