#ifndef ENCODERS_H
#define ENCODERS_H

#include <Arduino.h>
#include <Wire.h>
#include "config/hardware_config.h"

// Matrix mode has been surgically removed - encoder support is now permanent

// M5Stack 8Encoder I2C interface
// Using direct I2C communication for better control and timing

// Encoder performance monitoring
struct EncoderPerformance {
    uint32_t totalReads = 0;
    uint32_t successfulReads = 0;
    uint32_t timeouts = 0;
    uint32_t errors = 0;
    uint32_t totalTimeUs = 0;
    uint32_t maxTimeUs = 0;
    uint32_t lastReportTime = 0;
};

static EncoderPerformance g_encoderPerf;
static bool g_encoderAvailable = false;
static uint32_t g_lastEncoderCheck = 0;

// I2C register addresses for M5Stack 8Encoder
#define REG_ENCODER_VALUE 0x10
#define REG_ENCODER_RESET 0x20
#define REG_BUTTON_STATE  0x30
#define REG_RGB_LED       0x40

// Encoder state tracking
static int32_t g_encoderValues[8] = {0};
static bool g_buttonStates[8] = {false};
static uint32_t g_lastButtonPress[8] = {0};

// External function declarations
extern void startTransition(uint8_t newEffect);
extern uint8_t currentEffect;
extern const uint8_t NUM_EFFECTS;
extern uint8_t currentPaletteIndex;
extern const uint8_t gGradientPaletteCount;
extern uint8_t paletteSpeed;
extern uint8_t fadeAmount;
extern HardwareConfig::SyncMode currentSyncMode;
extern HardwareConfig::PropagationMode currentPropagationMode;

// I2C utility functions with timing
bool readI2CRegister(uint8_t reg, uint8_t* data, size_t len) {
    uint32_t startTime = micros();
    g_encoderPerf.totalReads++;
    
    Wire.beginTransmission(HardwareConfig::M5STACK_8ENCODER_ADDR);
    Wire.write(reg);
    uint8_t result = Wire.endTransmission();
    
    if (result != 0) {
        g_encoderPerf.errors++;
        return false;
    }
    
    Wire.requestFrom(HardwareConfig::M5STACK_8ENCODER_ADDR, len);
    uint32_t timeout = millis() + 10; // 10ms timeout
    
    while (Wire.available() < len && millis() < timeout) {
        delayMicroseconds(10);
    }
    
    if (Wire.available() < len) {
        g_encoderPerf.timeouts++;
        return false;
    }
    
    for (size_t i = 0; i < len; i++) {
        data[i] = Wire.read();
    }
    
    uint32_t elapsed = micros() - startTime;
    g_encoderPerf.totalTimeUs += elapsed;
    g_encoderPerf.maxTimeUs = max(g_encoderPerf.maxTimeUs, elapsed);
    g_encoderPerf.successfulReads++;
    
    return true;
}

bool writeI2CRegister(uint8_t reg, uint8_t* data, size_t len) {
    uint32_t startTime = micros();
    
    Wire.beginTransmission(HardwareConfig::M5STACK_8ENCODER_ADDR);
    Wire.write(reg);
    for (size_t i = 0; i < len; i++) {
        Wire.write(data[i]);
    }
    uint8_t result = Wire.endTransmission();
    
    uint32_t elapsed = micros() - startTime;
    g_encoderPerf.totalTimeUs += elapsed;
    
    return (result == 0);
}

// Initialize encoder system
void initEncoders() {
    Serial.println("Initializing M5Stack 8Encoder...");
    
    // DISABLE encoder for now to prevent crashes
    g_encoderAvailable = false;
    Serial.println("M5Stack 8Encoder DISABLED - system stability mode");
    Serial.println("Using button control instead");
    
    g_encoderPerf.lastReportTime = millis();
}

// Process encoder input with performance monitoring
void processEncoders() {
    if (!g_encoderAvailable) return;
    
    uint32_t now = millis();
    
    // Rate limiting: 20Hz (50ms intervals)
    if (now - g_lastEncoderCheck < 50) return;
    g_lastEncoderCheck = now;
    
    // Read all encoder values
    for (int i = 0; i < 8; i++) {
        uint8_t encoderData[4];
        if (readI2CRegister(REG_ENCODER_VALUE + i * 4, encoderData, 4)) {
            int32_t newValue = (encoderData[0] << 24) | (encoderData[1] << 16) | 
                              (encoderData[2] << 8) | encoderData[3];
            
            int32_t delta = newValue - g_encoderValues[i];
            g_encoderValues[i] = newValue;
            
            // Process encoder changes
            if (abs(delta) > 0 && abs(delta) < 100) { // Ignore spikes
                switch (i) {
                    case 0: // Effect selection
                        if (delta > 0) {
                            startTransition((currentEffect + 1) % NUM_EFFECTS);
                        } else {
                            startTransition(currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1);
                        }
                        break;
                        
                    case 1: // Palette selection
                        if (delta > 0) {
                            currentPaletteIndex = (currentPaletteIndex + 1) % gGradientPaletteCount;
                        } else {
                            currentPaletteIndex = currentPaletteIndex > 0 ? currentPaletteIndex - 1 : gGradientPaletteCount - 1;
                        }
                        break;
                        
                    case 2: // Speed control
                        paletteSpeed = constrain(paletteSpeed + (delta > 0 ? 2 : -2), 1, 50);
                        break;
                        
                    case 3: // Fade amount
                        fadeAmount = constrain(fadeAmount + (delta > 0 ? 3 : -3), 5, 50);
                        break;
                        
                    case 4: // Brightness - handled by FastLED
                        // This could control global brightness
                        break;
                        
                    case 5: // Sync mode
                        if (abs(delta) >= 2) { // Reduce sensitivity
                            int mode = (int)currentSyncMode + (delta > 0 ? 1 : -1);
                            if (mode < 0) mode = 3;
                            if (mode > 3) mode = 0;
                            currentSyncMode = (HardwareConfig::SyncMode)mode;
                        }
                        break;
                        
                    case 6: // Propagation mode
                        if (abs(delta) >= 2) { // Reduce sensitivity
                            int mode = (int)currentPropagationMode + (delta > 0 ? 1 : -1);
                            if (mode < 0) mode = 4;
                            if (mode > 4) mode = 0;
                            currentPropagationMode = (HardwareConfig::PropagationMode)mode;
                        }
                        break;
                        
                    case 7: // Reserved for future use
                        break;
                }
                
                // Update LED color to indicate activity
                uint8_t ledData[3] = {0, 255, 0}; // Green for activity
                writeI2CRegister(REG_RGB_LED + i, ledData, 3);
            }
        }
        
        // Small delay between encoder reads
        delayMicroseconds(500);
    }
    
    // Read button states
    uint8_t buttonData;
    if (readI2CRegister(REG_BUTTON_STATE, &buttonData, 1)) {
        for (int i = 0; i < 8; i++) {
            bool pressed = (buttonData & (1 << i)) != 0;
            
            if (pressed && !g_buttonStates[i] && (now - g_lastButtonPress[i] > 300)) {
                g_lastButtonPress[i] = now;
                
                // Button actions
                switch (i) {
                    case 0: // Reset to first effect
                        startTransition(0);
                        break;
                    case 1: // Reset palette
                        currentPaletteIndex = 0;
                        break;
                    case 2: // Reset speed
                        paletteSpeed = 10;
                        break;
                    case 3: // Reset fade
                        fadeAmount = 20;
                        break;
                    default:
                        // Other buttons for future use
                        break;
                }
                
                // Flash LED white for button press
                uint8_t ledData[3] = {255, 255, 255};
                writeI2CRegister(REG_RGB_LED + i, ledData, 3);
            }
            
            g_buttonStates[i] = pressed;
        }
    }
    
    // Performance reporting (every 10 seconds, no flooding)
    if (now - g_encoderPerf.lastReportTime > 10000) {
        if (g_encoderPerf.totalReads > 0) {
            uint32_t avgTimeUs = g_encoderPerf.totalTimeUs / g_encoderPerf.totalReads;
            float successRate = (float)g_encoderPerf.successfulReads / g_encoderPerf.totalReads * 100.0f;
            
            Serial.printf("Encoder Performance: %d reads, %.1f%% success, avg %dus, max %dus, %d timeouts, %d errors\n",
                         g_encoderPerf.totalReads, successRate, avgTimeUs, g_encoderPerf.maxTimeUs,
                         g_encoderPerf.timeouts, g_encoderPerf.errors);
        }
        
        // Reset counters
        g_encoderPerf = {0};
        g_encoderPerf.lastReportTime = now;
    }
}

// Fade encoder LEDs back to idle state
void updateEncoderLEDs() {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    
    if (now - lastUpdate > 100) { // Update every 100ms
        lastUpdate = now;
        
        for (int i = 0; i < 8; i++) {
            // Fade to dim blue idle state
            uint8_t ledData[3] = {0, 0, 32};
            writeI2CRegister(REG_RGB_LED + i, ledData, 3);
        }
    }
}


#endif // ENCODERS_H