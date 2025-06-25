#ifndef ENCODER_LED_FEEDBACK_H
#define ENCODER_LED_FEEDBACK_H

#include <Arduino.h>
#include <FastLED.h>
#include "m5rotate8.h"
#include "../config/hardware_config.h"
#include "../core/EffectTypes.h"

/**
 * Visual Feedback System (VFS)
 * 
 * This system uses the RGB LEDs on each encoder to provide real-time
 * visual feedback about effect parameters and system state.
 * 
 * Features:
 * - Dynamic color mapping based on effect parameters
 * - Breathing effects synchronized with effect timing
 * - Activity indicators for parameter changes
 * - Health/performance visualization
 */
class EncoderLEDFeedback {
private:
    M5ROTATE8* m_encoder;
    
    // LED state tracking
    struct LEDState {
        uint8_t r, g, b;
        float brightness;
        float phase;
        uint32_t lastActivity;
        bool isActive;
    } m_ledStates[9];  // 8 encoders + 1 scroll encoder
    
    // Animation parameters
    uint32_t m_lastUpdate = 0;
    float m_globalPhase = 0;
    static constexpr uint32_t UPDATE_RATE_MS = 20;  // 50Hz update rate
    
    // Visual parameters reference
    const VisualParams* m_visualParams;
    
    // Current effect metadata
    uint8_t m_currentEffectIndex = 0;
    const char* m_currentEffectName = nullptr;
    
    // Performance visualization
    float m_cpuUsage = 0.0f;
    float m_frameRate = 120.0f;
    
public:
    EncoderLEDFeedback(M5ROTATE8* encoder, const VisualParams* params) 
        : m_encoder(encoder), m_visualParams(params) {
        // Initialize LED states
        for (int i = 0; i < 9; i++) {
            m_ledStates[i] = {0, 0, 0, 1.0f, 0.0f, 0, false};
        }
    }
    
    // Update the LED feedback system
    void update();
    
    // Set current effect for context-aware feedback
    void setCurrentEffect(uint8_t index, const char* name) {
        m_currentEffectIndex = index;
        m_currentEffectName = name;
    }
    
    // Update performance metrics for visualization
    void updatePerformanceMetrics(float cpuUsage, float frameRate) {
        m_cpuUsage = cpuUsage;
        m_frameRate = frameRate;
    }
    
    // Flash an encoder LED to indicate activity
    void flashEncoder(uint8_t encoderId, uint8_t r, uint8_t g, uint8_t b, uint32_t duration = 200);
    
    // Set encoder LED color mapping based on function
    void applyDefaultColorScheme();
    
private:
    // Update individual encoder LEDs based on their function
    void updateEncoderLED(uint8_t index);
    
    // Specialized update functions for each encoder
    void updateEffectIndicator(LEDState& led);      // Encoder 0
    void updateBrightnessIndicator(LEDState& led);  // Encoder 1
    void updatePaletteIndicator(LEDState& led);     // Encoder 2
    void updateSpeedIndicator(LEDState& led);       // Encoder 3
    void updateIntensityIndicator(LEDState& led);   // Encoder 4
    void updateSaturationIndicator(LEDState& led);  // Encoder 5
    void updateComplexityIndicator(LEDState& led);  // Encoder 6
    void updateVariationIndicator(LEDState& led);   // Encoder 7
    void updateScrollIndicator(LEDState& led);      // Scroll encoder
    
    // Visual effect generators
    float generateBreathingEffect(float phase, float speed = 1.0f);
    float generatePulseEffect(float phase, float speed = 1.0f);
    float generateActivityDecay(uint32_t lastActivity, uint32_t decayTime = 500);
    
    // Color utility functions
    CRGB interpolateColor(CRGB from, CRGB to, float progress);
    void applyBrightness(LEDState& led, float brightness);
    
    // Performance indicator helpers
    CRGB getPerformanceColor(float value, float min, float max);
};

// Implementation
inline void EncoderLEDFeedback::update() {
    uint32_t now = millis();
    if (now - m_lastUpdate < UPDATE_RATE_MS) return;
    m_lastUpdate = now;
    
    // Update global animation phase
    m_globalPhase += 0.05f;  // ~1 second for full cycle at 50Hz
    if (m_globalPhase > TWO_PI) m_globalPhase -= TWO_PI;
    
    // Update each encoder LED
    for (int i = 0; i < 8; i++) {
        updateEncoderLED(i);
    }
    
    // Update scroll encoder if available
    // Note: Scroll encoder LED update would need to be implemented
    // based on the specific hardware capabilities
}

inline void EncoderLEDFeedback::updateEncoderLED(uint8_t index) {
    LEDState& led = m_ledStates[index];
    
    switch (index) {
        case 0: updateEffectIndicator(led); break;
        case 1: updateBrightnessIndicator(led); break;
        case 2: updatePaletteIndicator(led); break;
        case 3: updateSpeedIndicator(led); break;
        case 4: updateIntensityIndicator(led); break;
        case 5: updateSaturationIndicator(led); break;
        case 6: updateComplexityIndicator(led); break;
        case 7: updateVariationIndicator(led); break;
    }
    
    // Apply activity decay if active
    if (led.isActive) {
        float decay = generateActivityDecay(led.lastActivity);
        if (decay < 0.01f) {
            led.isActive = false;
        } else {
            led.brightness = max(led.brightness, decay);
        }
    }
    
    // Apply the color to the physical LED
    uint8_t finalR = led.r * led.brightness / 255;
    uint8_t finalG = led.g * led.brightness / 255;
    uint8_t finalB = led.b * led.brightness / 255;
    
    m_encoder->writeRGB(index, finalR, finalG, finalB);
}

inline void EncoderLEDFeedback::updateEffectIndicator(LEDState& led) {
    // Effect indicator - changes color based on effect type
    // Red base color with breathing
    led.r = 255;
    led.g = 0;
    led.b = 0;
    led.brightness = 128 + 127 * generateBreathingEffect(m_globalPhase);
}

inline void EncoderLEDFeedback::updateBrightnessIndicator(LEDState& led) {
    // Brightness - white intensity matches actual brightness
    uint8_t brightness = FastLED.getBrightness();
    led.r = led.g = led.b = 255;
    led.brightness = brightness;
}

inline void EncoderLEDFeedback::updatePaletteIndicator(LEDState& led) {
    // Palette - cycles through palette colors
    static float palettePhase = 0;
    palettePhase += 0.02f;
    if (palettePhase > 1.0f) palettePhase -= 1.0f;
    
    // Sample the current palette
    extern CRGBPalette16 currentPalette;
    CRGB color = ColorFromPalette(currentPalette, palettePhase * 255);
    led.r = color.r;
    led.g = color.g;
    led.b = color.b;
    led.brightness = 200;
}

inline void EncoderLEDFeedback::updateSpeedIndicator(LEDState& led) {
    // Speed - yellow pulsing faster with speed
    extern uint8_t paletteSpeed;
    float speedNorm = paletteSpeed / 255.0f;
    float pulseSpeed = 0.5f + speedNorm * 4.0f;
    
    led.r = 255;
    led.g = 200;
    led.b = 0;
    led.brightness = 128 + 127 * generatePulseEffect(m_globalPhase * pulseSpeed);
}

inline void EncoderLEDFeedback::updateIntensityIndicator(LEDState& led) {
    // Intensity - orange brightness based on parameter
    float intensity = m_visualParams->getIntensityNorm();
    led.r = 255;
    led.g = 128;
    led.b = 0;
    led.brightness = 64 + intensity * 191;  // Min brightness 64
}

inline void EncoderLEDFeedback::updateSaturationIndicator(LEDState& led) {
    // Saturation - cyan saturation visualization
    float saturation = m_visualParams->getSaturationNorm();
    led.r = 0;
    led.g = 255 * saturation;
    led.b = 255;
    led.brightness = 128 + 64 * generateBreathingEffect(m_globalPhase + PI);
}

inline void EncoderLEDFeedback::updateComplexityIndicator(LEDState& led) {
    // Complexity - green with pattern based on complexity
    float complexity = m_visualParams->getComplexityNorm();
    float pattern = sin(m_globalPhase * (1.0f + complexity * 5.0f));
    
    led.r = 0;
    led.g = 255;
    led.b = 64;
    led.brightness = 128 + 127 * pattern * pattern;  // Squared for sharper pulses
}

inline void EncoderLEDFeedback::updateVariationIndicator(LEDState& led) {
    // Variation - magenta with mode-based pattern
    float variation = m_visualParams->getVariationNorm();
    uint8_t mode = variation * 4;  // 4 discrete modes
    
    led.r = 255;
    led.g = 0;
    led.b = 255;
    
    // Different patterns for different modes
    switch (mode) {
        case 0: led.brightness = 255; break;  // Solid
        case 1: led.brightness = 128 + 127 * generateBreathingEffect(m_globalPhase); break;
        case 2: led.brightness = 128 + 127 * generatePulseEffect(m_globalPhase * 2); break;
        case 3: led.brightness = random8(128, 255); break;  // Sparkle
    }
}

inline void EncoderLEDFeedback::flashEncoder(uint8_t encoderId, uint8_t r, uint8_t g, uint8_t b, uint32_t duration) {
    if (encoderId >= 8) return;
    
    LEDState& led = m_ledStates[encoderId];
    led.r = r;
    led.g = g;
    led.b = b;
    led.brightness = 255;
    led.lastActivity = millis();
    led.isActive = true;
}

inline float EncoderLEDFeedback::generateBreathingEffect(float phase, float speed) {
    return (sin(phase * speed) + 1.0f) * 0.5f;
}

inline float EncoderLEDFeedback::generatePulseEffect(float phase, float speed) {
    float saw = fmod(phase * speed, TWO_PI) / TWO_PI;
    return 1.0f - saw;
}

inline float EncoderLEDFeedback::generateActivityDecay(uint32_t lastActivity, uint32_t decayTime) {
    uint32_t elapsed = millis() - lastActivity;
    if (elapsed >= decayTime) return 0.0f;
    return 1.0f - (float)elapsed / decayTime;
}

inline void EncoderLEDFeedback::applyDefaultColorScheme() {
    // Set default colors as per the specification
    m_encoder->writeRGB(0, 16, 0, 0);    // Red - Effect
    m_encoder->writeRGB(1, 16, 16, 16);  // White - Brightness
    m_encoder->writeRGB(2, 8, 0, 16);    // Purple - Palette
    m_encoder->writeRGB(3, 16, 8, 0);    // Yellow - Speed
    m_encoder->writeRGB(4, 16, 0, 8);    // Orange - Intensity
    m_encoder->writeRGB(5, 0, 16, 16);   // Cyan - Saturation
    m_encoder->writeRGB(6, 8, 16, 0);    // Lime - Complexity
    m_encoder->writeRGB(7, 16, 0, 16);   // Magenta - Variation
}

#endif // ENCODER_LED_FEEDBACK_H