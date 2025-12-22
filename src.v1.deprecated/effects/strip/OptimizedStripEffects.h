#ifndef OPTIMIZED_STRIP_EFFECTS_H
#define OPTIMIZED_STRIP_EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>

// External references from main
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;

// Distance lookup table
extern uint8_t distanceFromCenter[];

// Initialize optimization tables
void initOptimizedEffects();

// Optimized effect functions
void stripInterferenceOptimized();
void heartbeatEffectOptimized();
void breathingEffectOptimized();
void stripPlasmaOptimized();
void vortexEffectOptimized();

// Performance testing
void comparePerformance();

// Original functions for comparison
void stripInterference();

#endif // OPTIMIZED_STRIP_EFFECTS_H 