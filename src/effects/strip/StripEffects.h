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