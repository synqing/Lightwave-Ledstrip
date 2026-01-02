#ifndef TAB5_CONFIG_H
#define TAB5_CONFIG_H

#include <Arduino.h>

// ============================================================================
// Tab5.encoder Configuration
// ============================================================================
// M5Stack Tab5 (ESP32-P4) with M5ROTATE8 on Grove Port.A
// ============================================================================

// I2C Configuration for Tab5 Grove Port.A
// NOTE: These are the EXTERNAL I2C pins, isolated from internal bus
namespace I2C {
    // Tab5 Grove Port.A uses GPIO 53/54 for external I2C
    // These pins are obtained dynamically via M5.Ex_I2C.getSDA()/getSCL()
    // but we define constants here for reference and fallback
    constexpr uint8_t EXT_SDA_PIN = 53;
    constexpr uint8_t EXT_SCL_PIN = 54;

    // M5ROTATE8 I2C address
    constexpr uint8_t ROTATE8_ADDRESS = 0x41;

    // Conservative frequency (100kHz) for stability
    // The M5ROTATE8 library supports up to 400kHz, but we start safe
    constexpr uint32_t FREQ_HZ = 100000;

    // I2C timeout (ms) - larger than default 50ms for robustness
    constexpr uint16_t TIMEOUT_MS = 200;
}

// Parameter Indices (matches LightwaveOS)
enum class Parameter : uint8_t {
    Effect = 0,
    Brightness = 1,
    Palette = 2,
    Speed = 3,
    Intensity = 4,
    Saturation = 5,
    Complexity = 6,
    Variation = 7,
    COUNT = 8
};

// Parameter Ranges
namespace ParamRange {
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
}

// Parameter Default Values
namespace ParamDefault {
    constexpr uint8_t EFFECT = 0;
    constexpr uint8_t BRIGHTNESS = 128;
    constexpr uint8_t PALETTE = 0;
    constexpr uint8_t SPEED = 25;
    constexpr uint8_t INTENSITY = 128;
    constexpr uint8_t SATURATION = 255;
    constexpr uint8_t COMPLEXITY = 128;
    constexpr uint8_t VARIATION = 0;
}

// Parameter Names (for display/debugging)
namespace ParamName {
    constexpr const char* EFFECT = "Effect";
    constexpr const char* BRIGHTNESS = "Brightness";
    constexpr const char* PALETTE = "Palette";
    constexpr const char* SPEED = "Speed";
    constexpr const char* INTENSITY = "Intensity";
    constexpr const char* SATURATION = "Saturation";
    constexpr const char* COMPLEXITY = "Complexity";
    constexpr const char* VARIATION = "Variation";
}

// Helper: Get parameter name
inline const char* getParameterName(Parameter param) {
    switch (param) {
        case Parameter::Effect:      return ParamName::EFFECT;
        case Parameter::Brightness:  return ParamName::BRIGHTNESS;
        case Parameter::Palette:     return ParamName::PALETTE;
        case Parameter::Speed:       return ParamName::SPEED;
        case Parameter::Intensity:   return ParamName::INTENSITY;
        case Parameter::Saturation:  return ParamName::SATURATION;
        case Parameter::Complexity:  return ParamName::COMPLEXITY;
        case Parameter::Variation:   return ParamName::VARIATION;
        default:                     return "Unknown";
    }
}

// Helper: Get parameter min value
inline uint8_t getParameterMin(Parameter param) {
    switch (param) {
        case Parameter::Effect:      return ParamRange::EFFECT_MIN;
        case Parameter::Brightness:  return ParamRange::BRIGHTNESS_MIN;
        case Parameter::Palette:     return ParamRange::PALETTE_MIN;
        case Parameter::Speed:       return ParamRange::SPEED_MIN;
        case Parameter::Intensity:   return ParamRange::INTENSITY_MIN;
        case Parameter::Saturation:  return ParamRange::SATURATION_MIN;
        case Parameter::Complexity:  return ParamRange::COMPLEXITY_MIN;
        case Parameter::Variation:   return ParamRange::VARIATION_MIN;
        default:                     return 0;
    }
}

// Helper: Get parameter max value
inline uint8_t getParameterMax(Parameter param) {
    switch (param) {
        case Parameter::Effect:      return ParamRange::EFFECT_MAX;
        case Parameter::Brightness:  return ParamRange::BRIGHTNESS_MAX;
        case Parameter::Palette:     return ParamRange::PALETTE_MAX;
        case Parameter::Speed:       return ParamRange::SPEED_MAX;
        case Parameter::Intensity:   return ParamRange::INTENSITY_MAX;
        case Parameter::Saturation:  return ParamRange::SATURATION_MAX;
        case Parameter::Complexity:  return ParamRange::COMPLEXITY_MAX;
        case Parameter::Variation:   return ParamRange::VARIATION_MAX;
        default:                     return 255;
    }
}

// Helper: Get parameter default value
inline uint8_t getParameterDefault(Parameter param) {
    switch (param) {
        case Parameter::Effect:      return ParamDefault::EFFECT;
        case Parameter::Brightness:  return ParamDefault::BRIGHTNESS;
        case Parameter::Palette:     return ParamDefault::PALETTE;
        case Parameter::Speed:       return ParamDefault::SPEED;
        case Parameter::Intensity:   return ParamDefault::INTENSITY;
        case Parameter::Saturation:  return ParamDefault::SATURATION;
        case Parameter::Complexity:  return ParamDefault::COMPLEXITY;
        case Parameter::Variation:   return ParamDefault::VARIATION;
        default:                     return 0;
    }
}

#endif // TAB5_CONFIG_H
