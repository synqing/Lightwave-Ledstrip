#ifndef EFFECT_TYPES_H
#define EFFECT_TYPES_H

#include <Arduino.h>

// Universal visual parameters for encoders 4-7
struct VisualParams {
    uint8_t intensity = 128;      // Effect intensity/amplitude (0-255)
    uint8_t saturation = 255;     // Color saturation (0-255)
    uint8_t complexity = 128;     // Effect complexity/detail (0-255)
    uint8_t variation = 0;        // Effect variation/mode (0-255)
    
    // Helper functions for normalized access
    float getIntensityNorm() const { return intensity / 255.0f; }
    float getSaturationNorm() const { return saturation / 255.0f; }
    float getComplexityNorm() const { return complexity / 255.0f; }
    float getVariationNorm() const { return variation / 255.0f; }
};

#endif // EFFECT_TYPES_H