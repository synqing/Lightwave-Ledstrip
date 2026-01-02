#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// I2C Configuration for Tab5 Grove Port.A
namespace I2C {
    // Tab5 Grove Port.A pins (via M5.Ex_I2C)
    constexpr uint8_t SDA_PIN = 53;  // GPIO53
    constexpr uint8_t SCL_PIN = 54;  // GPIO54
    constexpr uint8_t ROTATE8_ADDRESS = 0x41;
    
    // For stability: 100kHz is confirmed working by the M5ROTATE8 library docs.
    // (400kHz also works, but we start conservative to avoid i2cRead timeouts.)
    constexpr uint32_t FREQ_HZ = 100000;
    
    // Wire default timeout is 50ms; we use a larger value to reduce spurious ESP_ERR_TIMEOUT (263).
    constexpr uint16_t TIMEOUT_MS = 200;
}

// Parameter Indices
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
    // Effect: 0-95
    constexpr uint8_t EFFECT_MIN = 0;
    constexpr uint8_t EFFECT_MAX = 95;

    // Brightness: 0-255
    constexpr uint8_t BRIGHTNESS_MIN = 0;
    constexpr uint8_t BRIGHTNESS_MAX = 255;

    // Palette: 0-63
    constexpr uint8_t PALETTE_MIN = 0;
    constexpr uint8_t PALETTE_MAX = 63;

    // Speed: 1-100
    constexpr uint8_t SPEED_MIN = 1;
    constexpr uint8_t SPEED_MAX = 100;

    // Intensity: 0-255
    constexpr uint8_t INTENSITY_MIN = 0;
    constexpr uint8_t INTENSITY_MAX = 255;

    // Saturation: 0-255
    constexpr uint8_t SATURATION_MIN = 0;
    constexpr uint8_t SATURATION_MAX = 255;

    // Complexity: 0-255
    constexpr uint8_t COMPLEXITY_MIN = 0;
    constexpr uint8_t COMPLEXITY_MAX = 255;

    // Variation: 0-255
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

// Helper function to get parameter name
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

// Helper function to get parameter min value
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

// Helper function to get parameter max value
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

// Helper function to get parameter default value
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

#endif // CONFIG_H

