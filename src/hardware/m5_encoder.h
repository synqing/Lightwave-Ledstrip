#ifndef M5_ENCODER_H
#define M5_ENCODER_H

#include <Arduino.h>
#include <Wire.h>
#include "m5rotate8.h"
#include "config/hardware_config.h"

#if LED_STRIPS_MODE

// M5Stack 8Encoder using M5ROTATE8 library
// Simple, stable implementation without complex performance monitoring

// Global encoder instance
M5ROTATE8 encoder;
static bool g_encoderAvailable = false;
static uint32_t g_lastEncoderCheck = 0;
static uint32_t g_lastLEDUpdate = 0;

// External function declarations - access to main.cpp globals
extern void startTransition(uint8_t newEffect);
extern uint8_t currentEffect;
extern const uint8_t NUM_EFFECTS;
extern uint8_t currentPaletteIndex;
extern const uint8_t gGradientPaletteCount;
extern uint8_t paletteSpeed;
extern uint8_t fadeAmount;
extern uint8_t gHue;
extern HardwareConfig::SyncMode currentSyncMode;
extern HardwareConfig::PropagationMode currentPropagationMode;
extern CRGBPalette16 targetPalette;
extern const TProgmemRGBGradientPaletteRef gGradientPalettes[];

// Encoder state tracking
static int32_t g_lastEncoderValues[8] = {0};
static bool g_lastButtonStates[8] = {false};
static uint32_t g_lastButtonPress[8] = {0};

// LED feedback state
static uint32_t g_ledFlashTime[8] = {0};
static uint8_t g_ledFlashType[8] = {0}; // 0=idle, 1=active, 2=button

// Initialize encoder system using M5ROTATE8 library
void initEncoders() {
    Serial.println("Initializing M5Stack 8Encoder with M5ROTATE8 library...");
    
    // Initialize I2C with correct pins
    Wire.begin(HardwareConfig::I2C_SDA, HardwareConfig::I2C_SCL);
    Serial.print("I2C initialized - SDA: GPIO");
    Serial.print(HardwareConfig::I2C_SDA);
    Serial.print(", SCL: GPIO");
    Serial.println(HardwareConfig::I2C_SCL);
    
    // Initialize M5ROTATE8 encoder
    bool success = encoder.begin();
    g_encoderAvailable = success && encoder.isConnected();
    
    if (g_encoderAvailable) {
        Serial.print("M5Stack 8Encoder connected at address 0x");
        Serial.println(encoder.getAddress(), HEX);
        Serial.print("Firmware version: ");
        Serial.println(encoder.getVersion());
        
        // Reset all encoder counters
        encoder.resetAll();
        
        // Set all LEDs to dim blue idle state
        encoder.setAll(0, 0, 32);
        
        Serial.println("M5Stack 8Encoder initialized successfully!");
        Serial.println("Encoder mappings:");
        Serial.println("  0: Effect selection");
        Serial.println("  1: Palette selection");
        Serial.println("  2: Palette speed");
        Serial.println("  3: Fade amount");
        Serial.println("  4: Brightness control");
        Serial.println("  5: Sync mode");
        Serial.println("  6: Propagation mode");
        Serial.println("  7: Reserved");
        
    } else {
        Serial.println("M5Stack 8Encoder NOT found - running without encoder control");
        g_encoderAvailable = false;
    }
}

// Process encoder input using M5ROTATE8 library
void processEncoders() {
    if (!g_encoderAvailable) return;
    
    uint32_t now = millis();
    
    // Rate limiting: 20Hz (50ms intervals) for stability
    if (now - g_lastEncoderCheck < 50) return;
    g_lastEncoderCheck = now;
    
    // Check connection health
    if (!encoder.isConnected()) {
        Serial.println("M5Stack 8Encoder disconnected!");
        g_encoderAvailable = false;
        return;
    }
    
    // Process each encoder channel
    for (int i = 0; i < 8; i++) {
        // Read relative counter change (more efficient than absolute)
        int32_t delta = encoder.getRelCounter(i);
        
        // Process encoder rotation
        if (delta != 0) {
            // Flash green LED for activity
            g_ledFlashTime[i] = now;
            g_ledFlashType[i] = 1; // Active
            encoder.writeRGB(i, 0, 255, 0);
            
            // Map encoder to parameters
            switch (i) {
                case 0: // Effect selection
                    if (delta > 0) {
                        startTransition((currentEffect + 1) % NUM_EFFECTS);
                    } else {
                        startTransition(currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1);
                    }
                    Serial.print("Encoder 0: Effect -> ");
                    Serial.println(currentEffect);
                    break;
                    
                case 1: // Palette selection
                    if (delta > 0) {
                        currentPaletteIndex = (currentPaletteIndex + 1) % gGradientPaletteCount;
                    } else {
                        currentPaletteIndex = currentPaletteIndex > 0 ? currentPaletteIndex - 1 : gGradientPaletteCount - 1;
                    }
                    // Update target palette for immediate effect
                    targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
                    Serial.print("Encoder 1: Palette -> ");
                    Serial.println(currentPaletteIndex);
                    break;
                    
                case 2: // Palette speed
                    paletteSpeed = constrain(paletteSpeed + (delta > 0 ? 2 : -2), 1, 50);
                    Serial.print("Encoder 2: Speed -> ");
                    Serial.println(paletteSpeed);
                    break;
                    
                case 3: // Fade amount
                    fadeAmount = constrain(fadeAmount + (delta > 0 ? 3 : -3), 5, 50);
                    Serial.print("Encoder 3: Fade -> ");
                    Serial.println(fadeAmount);
                    break;
                    
                case 4: // Brightness control
                    {
                        int brightness = FastLED.getBrightness() + (delta > 0 ? 8 : -8);
                        brightness = constrain(brightness, 16, 255);
                        FastLED.setBrightness(brightness);
                        Serial.print("Encoder 4: Brightness -> ");
                        Serial.println(brightness);
                    }
                    break;
                    
                case 5: // Sync mode
                    if (abs(delta) >= 1) {
                        int mode = (int)currentSyncMode + (delta > 0 ? 1 : -1);
                        if (mode < 0) mode = 3;
                        if (mode > 3) mode = 0;
                        currentSyncMode = (HardwareConfig::SyncMode)mode;
                        Serial.print("Encoder 5: Sync mode -> ");
                        Serial.println((int)currentSyncMode);
                    }
                    break;
                    
                case 6: // Propagation mode
                    if (abs(delta) >= 1) {
                        int mode = (int)currentPropagationMode + (delta > 0 ? 1 : -1);
                        if (mode < 0) mode = 4;
                        if (mode > 4) mode = 0;
                        currentPropagationMode = (HardwareConfig::PropagationMode)mode;
                        Serial.print("Encoder 6: Propagation mode -> ");
                        Serial.println((int)currentPropagationMode);
                    }
                    break;
                    
                case 7: // Reserved for future use
                    Serial.print("Encoder 7: Reserved -> ");
                    Serial.println(delta);
                    break;
            }
        }
        
        // Read button state
        bool pressed = encoder.getKeyPressed(i);
        
        // Handle button press (rising edge detection)
        if (pressed && !g_lastButtonStates[i] && (now - g_lastButtonPress[i] > 300)) {
            g_lastButtonPress[i] = now;
            g_ledFlashTime[i] = now;
            g_ledFlashType[i] = 2; // Button press
            encoder.writeRGB(i, 255, 0, 0); // Red flash
            
            // Button actions - reset parameters to defaults
            switch (i) {
                case 0: // Reset to first effect
                    startTransition(0);
                    Serial.println("Button 0: Reset to first effect");
                    break;
                case 1: // Reset palette
                    currentPaletteIndex = 0;
                    targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
                    Serial.println("Button 1: Reset palette");
                    break;
                case 2: // Reset speed
                    paletteSpeed = 10;
                    Serial.println("Button 2: Reset speed");
                    break;
                case 3: // Reset fade
                    fadeAmount = 20;
                    Serial.println("Button 3: Reset fade");
                    break;
                case 4: // Reset brightness
                    FastLED.setBrightness(HardwareConfig::STRIP_BRIGHTNESS);
                    Serial.println("Button 4: Reset brightness");
                    break;
                case 5: // Reset sync mode
                    currentSyncMode = HardwareConfig::SYNC_SYNCHRONIZED;
                    Serial.println("Button 5: Reset sync mode");
                    break;
                case 6: // Reset propagation mode
                    currentPropagationMode = HardwareConfig::PROPAGATE_OUTWARD;
                    Serial.println("Button 6: Reset propagation mode");
                    break;
                case 7: // Mode switch / special function
                    Serial.println("Button 7: Mode switch");
                    break;
            }
        }
        
        g_lastButtonStates[i] = pressed;
    }
}

// Update encoder LED feedback
void updateEncoderLEDs() {
    if (!g_encoderAvailable) return;
    
    uint32_t now = millis();
    
    // Update LEDs every 100ms
    if (now - g_lastLEDUpdate < 100) return;
    g_lastLEDUpdate = now;
    
    for (int i = 0; i < 8; i++) {
        // Check if flash time has expired (500ms for active, 200ms for button)
        uint32_t flashDuration = (g_ledFlashType[i] == 2) ? 200 : 500;
        
        if (g_ledFlashTime[i] > 0 && (now - g_ledFlashTime[i] > flashDuration)) {
            // Flash expired, return to idle state
            g_ledFlashTime[i] = 0;
            g_ledFlashType[i] = 0;
            
            // Set LED based on encoder function
            switch (i) {
                case 0: // Effect - red hue
                    encoder.writeRGB(i, 16, 0, 0);
                    break;
                case 1: // Palette - purple hue
                    encoder.writeRGB(i, 8, 0, 16);
                    break;
                case 2: // Speed - yellow hue
                    encoder.writeRGB(i, 16, 8, 0);
                    break;
                case 3: // Fade - cyan hue
                    encoder.writeRGB(i, 0, 8, 16);
                    break;
                case 4: // Brightness - white
                    encoder.writeRGB(i, 8, 8, 8);
                    break;
                case 5: // Sync mode - green hue
                    encoder.writeRGB(i, 0, 16, 0);
                    break;
                case 6: // Propagation mode - blue hue
                    encoder.writeRGB(i, 0, 0, 16);
                    break;
                case 7: // Reserved - dim blue
                    encoder.writeRGB(i, 0, 0, 8);
                    break;
            }
        }
    }
}

// Get encoder availability status
bool isEncoderAvailable() {
    return g_encoderAvailable;
}

// Get encoder connection status
bool checkEncoderConnection() {
    if (!g_encoderAvailable) return false;
    return encoder.isConnected();
}

#endif // LED_STRIPS_MODE

#endif // M5_ENCODER_H