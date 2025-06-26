#ifndef STRIP_EFFECTS_H
#define STRIP_EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

// External dependencies from main.cpp
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
extern CRGB leds[HardwareConfig::NUM_LEDS];
extern uint8_t gHue;
extern CRGBPalette16 currentPalette;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// Orchestrator integration wrappers (from main.cpp)
extern CRGB getOrchestratedColor(uint8_t position, uint8_t brightness = 255);
extern uint8_t getEmotionalBrightness(uint8_t baseBrightness);
extern CRGB getOrchestratedColorFast(uint8_t position, uint8_t brightness = 255);

// Orchestrator instance for direct access when needed
#include "../CinematicColorOrchestrator.h"
extern CinematicColorOrchestrator colorOrchestrator;

// ============== UNIFIED SPEED SYSTEM ==============
// Convert paletteSpeed (1-50) to consistent speed multiplier (0.1 to 2.0)
// ALL effects MUST use this for consistency
inline float getUnifiedSpeed() {
    // Linear mapping: 1=0.1x (very slow), 25=1.0x (normal), 50=2.0x (fast)
    return 0.1f + (paletteSpeed - 1) * (1.9f / 49.0f);
}

// Frame-rate independent speed (accounts for actual frame time)
inline float getFrameSpeed(uint32_t targetFPS = 60) {
    static uint32_t lastFrameTime = 0;
    uint32_t currentTime = millis();
    uint32_t deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;
    
    // Calculate speed multiplier based on actual frame time
    float frameMultiplier = deltaTime / (1000.0f / targetFPS);
    return getUnifiedSpeed() * frameMultiplier;
}

// ============== BASIC STRIP EFFECTS ==============
void solidColor();
void pulseEffect();
void confetti();
void stripConfetti();
void sinelon();
void juggle();
void stripJuggle();
void bpm();

// ============== ADVANCED WAVE EFFECTS ==============
void waveEffect();
void rippleEffect();
void stripInterference();
void stripBPM();
void stripPlasma();

// ============== MATHEMATICAL PATTERNS ==============
void plasma();

// ============== NATURE-INSPIRED EFFECTS ==============
void fire();
void ocean();
void stripOcean();

// ============== NEW CENTER ORIGIN EFFECTS ==============
void heartbeatEffect();
void breathingEffect();
void shockwaveEffect();
void vortexEffect();
void collisionEffect();
void gravityWellEffect();

#endif // STRIP_EFFECTS_H