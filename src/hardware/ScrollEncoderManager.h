#ifndef SCROLL_ENCODER_MANAGER_H
#define SCROLL_ENCODER_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "M5UnitScroll.h"
#include "../config/hardware_config.h"
#include "../core/EffectTypes.h"

// Scroll encoder parameters that can be controlled
enum ScrollParameter : uint8_t {
    PARAM_EFFECT = 0,       // Effect selection
    PARAM_BRIGHTNESS = 1,   // Global brightness
    PARAM_PALETTE = 2,      // Color palette
    PARAM_SPEED = 3,        // Animation speed
    PARAM_INTENSITY = 4,    // Effect intensity
    PARAM_SATURATION = 5,   // Color saturation
    PARAM_COMPLEXITY = 6,   // Pattern complexity
    PARAM_VARIATION = 7,    // Effect variation
    PARAM_COUNT = 8
};

// Parameter colors for LED feedback
extern const uint32_t PARAM_COLORS[PARAM_COUNT];

// Parameter names for display
extern const char* PARAM_NAMES[PARAM_COUNT];

// Scroll encoder state
struct ScrollState {
    int32_t value = 0;
    int32_t lastValue = 0;
    bool buttonPressed = false;
    uint32_t lastButtonPress = 0;
    uint32_t lastUpdate = 0;
    ScrollParameter currentParam = PARAM_EFFECT;
    
    // Parameter values (normalized 0-255)
    // Effect, Brightness, Palette, Speed, Intensity, Saturation, Complexity, Variation
    uint8_t paramValues[PARAM_COUNT] = {0, 96, 0, 128, 128, 128, 128, 128};
};

// Scroll encoder manager class
class ScrollEncoderManager {
private:
    M5UnitScroll* scrollEncoder = nullptr;
    bool isAvailable = false;
    ScrollState state;
    
    // Thread safety - defined in main.cpp
    
    // Callbacks
    void (*onParamChange)(ScrollParameter param, uint8_t value) = nullptr;
    void (*onEffectChange)(uint8_t effect) = nullptr;
    
    // LED animation
    uint32_t ledAnimPhase = 0;
    
public:
    ScrollEncoderManager();
    ~ScrollEncoderManager();
    
    // Initialize the scroll encoder
    bool begin();
    
    // Process encoder input (call from main loop)
    void update();
    
    // Check if available
    bool available() const { return isAvailable; }
    
    // Get current parameter
    ScrollParameter getCurrentParam() const { return state.currentParam; }
    
    // Get parameter value
    uint8_t getParamValue(ScrollParameter param) const {
        return state.paramValues[param];
    }
    
    // Set parameter value
    void setParamValue(ScrollParameter param, uint8_t value) {
        if (param < PARAM_COUNT) {
            state.paramValues[param] = value;
        }
    }
    
    // Set callbacks
    void setParamChangeCallback(void (*callback)(ScrollParameter, uint8_t)) {
        onParamChange = callback;
    }
    
    void setEffectChangeCallback(void (*callback)(uint8_t)) {
        onEffectChange = callback;
    }
    
    // Get visual params for effects
    void updateVisualParams(VisualParams& params);
    
private:
    // Update LED color based on current parameter
    void updateLED();
    
    // Handle parameter value changes
    void handleValueChange(int32_t delta);
    
    // Cycle through parameters
    void nextParameter();
};

// Global instance
extern ScrollEncoderManager scrollManager;

#endif // SCROLL_ENCODER_MANAGER_H