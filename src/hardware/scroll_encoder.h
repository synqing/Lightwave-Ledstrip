#ifndef SCROLL_ENCODER_H
#define SCROLL_ENCODER_H

#include <Arduino.h>
#include <Wire.h>
#include "M5UnitScroll.h"
#include "config/hardware_config.h"

// M5Unit-Scroll encoder interface on secondary I2C bus
// This provides a 9th encoder for additional control

// Use the pre-defined Wire1 instance from Arduino framework
// Wire1 is already defined as TwoWire(1) in the framework

// Scroll encoder instance
M5UnitScroll scrollEncoder;
bool scrollEncoderAvailable = false;

// Scroll encoder state
struct ScrollEncoderState {
    int32_t value = 0;
    int32_t lastValue = 0;
    bool buttonPressed = false;
    bool buttonPressHandled = false;
    uint32_t lastUpdate = 0;
    uint32_t lastButtonPress = 0;
    
    // Which M5ROTATE8 encoder we're mirroring (0-7)
    uint8_t mirroredEncoder = 0;
    
    // Function pointer for value change callback
    void (*onValueChange)(int32_t delta) = nullptr;
    void (*onButtonPress)() = nullptr;
} scrollState;

// Performance monitoring for scroll encoder
struct ScrollEncoderPerf {
    uint32_t totalReads = 0;
    uint32_t successfulReads = 0;
    uint32_t errors = 0;
    uint32_t lastReportTime = 0;
} scrollPerf;

// Forward declarations
void updateScrollEncoderLEDForMode();

// Initialize scroll encoder on secondary I2C bus
void initScrollEncoder() {
    Serial.println("Initializing M5Unit-Scroll on secondary I2C bus...");
    
    // Initialize secondary I2C bus on pins 20/21
    Wire1.begin(HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL);
    Wire1.setClock(100000);  // 100kHz for stability
    
    // Small delay for I2C bus to stabilize
    delay(50);
    
    // Initialize scroll encoder
    if (scrollEncoder.begin(&Wire1, HardwareConfig::M5UNIT_SCROLL_ADDR)) {
        scrollEncoderAvailable = true;
        Serial.println("✅ M5Unit-Scroll connected successfully!");
        Serial.print("   Address: 0x");
        Serial.println(HardwareConfig::M5UNIT_SCROLL_ADDR, HEX);
        Serial.print("   Firmware: V");
        Serial.println(scrollEncoder.getFirmwareVersion());
        
        // Reset encoder value to 0
        scrollEncoder.setEncoderValue(0);
        
        // Set initial LED color based on mode
        updateScrollEncoderLEDForMode();
        
        // Initialize state
        scrollState.value = 0;
        scrollState.lastValue = 0;
        scrollPerf.lastReportTime = millis();
        
    } else {
        scrollEncoderAvailable = false;
        Serial.println("❌ M5Unit-Scroll not found on secondary I2C bus");
        Serial.println("   Check connections: SDA=GPIO20, SCL=GPIO21");
    }
}

// Process scroll encoder input
void processScrollEncoder() {
    if (!scrollEncoderAvailable) return;
    
    uint32_t now = millis();
    
    // Rate limit to 50Hz
    if (now - scrollState.lastUpdate < 20) return;
    scrollState.lastUpdate = now;
    
    scrollPerf.totalReads++;
    
    // Read encoder value
    int32_t newValue = scrollEncoder.getEncoderValue();
    if (newValue != scrollState.value) {
        int32_t delta = newValue - scrollState.value;
        scrollState.lastValue = scrollState.value;
        scrollState.value = newValue;
        
        // Ignore large jumps (likely errors)
        if (abs(delta) < 100) {
            // Flash LED green for activity
            scrollEncoder.setLEDColor(0x00FF00);  // Green
            
            // Call callback if set
            if (scrollState.onValueChange) {
                scrollState.onValueChange(delta);
            }
            
            scrollPerf.successfulReads++;
        } else {
            scrollPerf.errors++;
        }
    }
    
    // Read button state with debouncing
    bool buttonNow = scrollEncoder.getButtonStatus();
    if (buttonNow && !scrollState.buttonPressed && (now - scrollState.lastButtonPress > 300)) {
        scrollState.buttonPressed = true;
        scrollState.lastButtonPress = now;
        scrollState.buttonPressHandled = false;
        
        // Flash LED white for button press
        scrollEncoder.setLEDColor(0xFFFFFF);  // White
        
        // Call callback if set
        if (scrollState.onButtonPress) {
            scrollState.onButtonPress();
        }
        
        scrollPerf.successfulReads++;
    } else if (!buttonNow && scrollState.buttonPressed) {
        scrollState.buttonPressed = false;
    }
    
    // Fade LED back to mode color
    static uint32_t lastLedUpdate = 0;
    if (now - lastLedUpdate > 100) {
        lastLedUpdate = now;
        updateScrollEncoderLEDForMode();  // Return to mode color
    }
    
    // Performance reporting
    if (now - scrollPerf.lastReportTime > 10000) {
        if (scrollPerf.totalReads > 0) {
            float successRate = (float)scrollPerf.successfulReads / scrollPerf.totalReads * 100.0f;
            Serial.printf("Scroll Encoder: %d reads, %.1f%% success, %d errors\n",
                         scrollPerf.totalReads, successRate, scrollPerf.errors);
        }
        
        // Reset counters
        scrollPerf = {0};
        scrollPerf.lastReportTime = now;
    }
}

// Set scroll encoder LED color
void setScrollEncoderLED(uint8_t r, uint8_t g, uint8_t b) {
    if (scrollEncoderAvailable) {
        uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        scrollEncoder.setLEDColor(color);
    }
}

// Get scroll encoder value
int32_t getScrollEncoderValue() {
    return scrollState.value;
}

// Reset scroll encoder value
void resetScrollEncoder() {
    if (scrollEncoderAvailable) {
        scrollEncoder.setEncoderValue(0);
        scrollState.value = 0;
        scrollState.lastValue = 0;
    }
}

// Set callbacks for scroll encoder events
void setScrollEncoderCallbacks(void (*onChange)(int32_t), void (*onPress)()) {
    scrollState.onValueChange = onChange;
    scrollState.onButtonPress = onPress;
}

// Update LED color based on mirrored encoder
void updateScrollEncoderLEDForMode() {
    if (!scrollEncoderAvailable) return;
    
    // Different color for each encoder mode
    const uint32_t encoderColors[8] = {
        0xFF0000,  // 0: Effect - Red
        0xFFFF00,  // 1: Brightness - Yellow  
        0x00FF00,  // 2: Palette - Green
        0x00FFFF,  // 3: Speed - Cyan
        0x0080FF,  // 4: Intensity - Light Blue
        0xFF00FF,  // 5: Saturation - Magenta
        0xFF8000,  // 6: Complexity - Orange
        0x8000FF   // 7: Variation - Purple
    };
    
    scrollEncoder.setLEDColor(encoderColors[scrollState.mirroredEncoder]);
}

// Get current mirrored encoder
uint8_t getScrollMirroredEncoder() {
    return scrollState.mirroredEncoder;
}

// Set which encoder to mirror
void setScrollMirroredEncoder(uint8_t encoder) {
    if (encoder < 8) {
        scrollState.mirroredEncoder = encoder;
        updateScrollEncoderLEDForMode();
    }
}

#endif // SCROLL_ENCODER_H