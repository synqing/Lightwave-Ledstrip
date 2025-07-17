#include "ScrollEncoderManager.h"
#include <FastLED.h>

// Global instance
ScrollEncoderManager scrollManager;

// Parameter colors for LED feedback
const uint32_t PARAM_COLORS[PARAM_COUNT] = {
    0xFF0000,  // Effect - Red
    0xFFFF00,  // Brightness - Yellow  
    0x00FF00,  // Palette - Green
    0x00FFFF,  // Speed - Cyan
    0x0080FF,  // Intensity - Light Blue
    0xFF00FF,  // Saturation - Magenta
    0xFF8000,  // Complexity - Orange
    0x8000FF   // Variation - Purple
};

// Parameter names for display
const char* PARAM_NAMES[PARAM_COUNT] = {
    "Effect", "Brightness", "Palette", "Speed",
    "Intensity", "Saturation", "Complexity", "Variation"
};

// External dependencies
extern uint8_t currentEffect;
extern const uint8_t NUM_EFFECTS;
extern uint8_t currentPaletteIndex;
// External palette count
extern const uint8_t gGradientPaletteCount;
// External transition function
extern void startAdvancedTransition(uint8_t targetEffect);
extern SemaphoreHandle_t i2cMutex;

ScrollEncoderManager::ScrollEncoderManager() {
    scrollEncoder = nullptr;
    isAvailable = false;
}

ScrollEncoderManager::~ScrollEncoderManager() {
    if (scrollEncoder) {
        delete scrollEncoder;
        scrollEncoder = nullptr;
    }
}

bool ScrollEncoderManager::begin() {
    Serial.println("\n🔌 Initializing M5Unit-Scroll...");
    
    // Create encoder instance
    scrollEncoder = new M5UnitScroll();
    if (!scrollEncoder) {
        Serial.println("❌ Failed to allocate memory for scroll encoder");
        return false;
    }
    
    // Initialize with correct pins for your wiring
    bool initSuccess = scrollEncoder->begin(
        &Wire,                              // Use main Wire instance
        HardwareConfig::M5UNIT_SCROLL_ADDR, // 0x40
        HardwareConfig::I2C_SDA_SCROLL,     // GPIO11 - your SDA wiring
        HardwareConfig::I2C_SCL_SCROLL,     // GPIO12 - your SCL wiring
        400000                              // 400kHz
    );
    
    if (initSuccess) {
        isAvailable = true;
        Serial.println("✅ M5Unit-Scroll connected successfully!");
        
        // Get firmware version
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint8_t version = scrollEncoder->getFirmwareVersion();
            Serial.printf("   Firmware: V%d\n", version);
            xSemaphoreGive(i2cMutex);
        }
        
        // Test LED with startup animation
        Serial.println("   Testing LED...");
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Rainbow startup sequence
            scrollEncoder->setLEDColor(0xFF0000); // Red
            xSemaphoreGive(i2cMutex);
            delay(200);
            
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                scrollEncoder->setLEDColor(0x00FF00); // Green
                xSemaphoreGive(i2cMutex);
            }
            delay(200);
            
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                scrollEncoder->setLEDColor(0x0000FF); // Blue
                xSemaphoreGive(i2cMutex);
            }
            delay(200);
        }
        
        // Reset encoder value
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            scrollEncoder->setEncoderValue(0);
            xSemaphoreGive(i2cMutex);
        }
        
        // Initialize state
        state.value = 0;
        state.lastValue = 0;
        state.currentParam = PARAM_EFFECT;
        
        // Set initial LED color
        updateLED();
        
        return true;
    } else {
        delete scrollEncoder;
        scrollEncoder = nullptr;
        isAvailable = false;
        
        Serial.println("\n❌ M5Unit-Scroll initialization failed!");
        Serial.println("   Possible issues:");
        Serial.printf("   1. Check wiring (SDA=GPIO%d, SCL=GPIO%d)\n", 
                      HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL);
        Serial.println("   2. Verify I2C address is 0x40");
        Serial.println("   3. Ensure device is powered");
        Serial.println("   4. Check for I2C conflicts with other devices");
        
        return false;
    }
}

void ScrollEncoderManager::update() {
    if (!isAvailable || !scrollEncoder) return;
    
    uint32_t now = millis();
    
    // Rate limit to 50Hz
    if (now - state.lastUpdate < 20) return;
    state.lastUpdate = now;
    
    // Thread-safe I2C access
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return; // Skip this update if can't get mutex
    }
    
    // Read incremental encoder value
    int16_t delta = scrollEncoder->getIncEncoderValue();
    
    // Read button state
    bool buttonPressed = scrollEncoder->getButtonStatus();
    
    xSemaphoreGive(i2cMutex);
    
    // Handle encoder rotation
    if (delta != 0) {
        state.lastValue = state.value;
        state.value += delta;
        handleValueChange(delta);
        
        // Flash LED briefly on change
        ledAnimPhase = 255;
    }
    
    // Handle button press (with debouncing)
    if (buttonPressed && !state.buttonPressed && (now - state.lastButtonPress > 200)) {
        state.buttonPressed = true;
        state.lastButtonPress = now;
        nextParameter();
        
        Serial.printf("Scroll: Switched to %s mode\n", PARAM_NAMES[state.currentParam]);
    } else if (!buttonPressed && state.buttonPressed) {
        state.buttonPressed = false;
    }
    
    // Update LED with animation
    if (ledAnimPhase > 0) {
        ledAnimPhase = max(0, (int)ledAnimPhase - 10);
    }
    updateLED();
}

void ScrollEncoderManager::handleValueChange(int32_t delta) {
    ScrollParameter param = state.currentParam;
    uint8_t oldValue = state.paramValues[param];
    int16_t newValue = oldValue;
    
    switch (param) {
        case PARAM_EFFECT:
            // Special handling for effect selection
            if (delta > 0) {
                uint8_t newEffect = (currentEffect + 1) % NUM_EFFECTS;
                if (onEffectChange) {
                    onEffectChange(newEffect);
                } else {
                    startAdvancedTransition(newEffect);
                }
            } else if (delta < 0) {
                uint8_t newEffect = currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1;
                if (onEffectChange) {
                    onEffectChange(newEffect);
                } else {
                    startAdvancedTransition(newEffect);
                }
            }
            return; // Don't update paramValues for effect
            
        case PARAM_BRIGHTNESS:
            // Fine control for brightness
            newValue += delta * 2;
            break;
            
        case PARAM_PALETTE:
            // Cycle through palettes
            if (delta > 0) {
                newValue = (oldValue + 1) % gGradientPaletteCount;
            } else if (delta < 0) {
                newValue = oldValue > 0 ? oldValue - 1 : gGradientPaletteCount - 1;
            }
            break;
            
        default:
            // Standard increment/decrement for other params
            newValue += delta * 4; // Larger steps
            break;
    }
    
    // Clamp to valid range
    newValue = constrain(newValue, 0, 255);
    state.paramValues[param] = newValue;
    
    // Trigger callback
    if (onParamChange && newValue != oldValue) {
        onParamChange(param, newValue);
    }
    
    // Debug output
    #if FEATURE_DEBUG_OUTPUT
    if (newValue != oldValue) {
        Serial.printf("Scroll: %s = %d\n", PARAM_NAMES[param], newValue);
    }
    #endif
}

void ScrollEncoderManager::nextParameter() {
    state.currentParam = (ScrollParameter)((state.currentParam + 1) % PARAM_COUNT);
    updateLED();
}

void ScrollEncoderManager::updateLED() {
    if (!isAvailable || !scrollEncoder) return;
    
    uint32_t baseColor = PARAM_COLORS[state.currentParam];
    
    // Add brightness modulation for activity
    uint8_t brightness = 128 + (ledAnimPhase / 2);
    
    // Extract RGB components
    uint8_t r = (baseColor >> 16) & 0xFF;
    uint8_t g = (baseColor >> 8) & 0xFF;
    uint8_t b = baseColor & 0xFF;
    
    // Apply brightness
    r = (r * brightness) / 255;
    g = (g * brightness) / 255;
    b = (b * brightness) / 255;
    
    uint32_t color = (r << 16) | (g << 8) | b;
    
    // Thread-safe LED update
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        scrollEncoder->setLEDColor(color);
        xSemaphoreGive(i2cMutex);
    }
}

void ScrollEncoderManager::updateVisualParams(VisualParams& params) {
    params.intensity = state.paramValues[PARAM_INTENSITY];
    params.saturation = state.paramValues[PARAM_SATURATION];
    params.complexity = state.paramValues[PARAM_COMPLEXITY];
    params.variation = state.paramValues[PARAM_VARIATION];
}