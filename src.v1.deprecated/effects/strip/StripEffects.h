#ifndef STRIP_EFFECTS_H
#define STRIP_EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "../../core/FxEngine.h"

// External dependencies from main.cpp
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
extern CRGB leds[HardwareConfig::NUM_LEDS];
extern uint8_t gHue;
extern CRGBPalette16 currentPalette;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// Safety macro for writing to strip2 (respects actual LED count)
#define SAFE_STRIP2_WRITE(index, value) \
    do { \
        if ((index) < HardwareConfig::STRIP2_LED_COUNT) { \
            strip2[(index)] = (value); \
        } \
    } while(0)

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

// LGP-specific effects
void lgpInterferenceScanner();
void lgpHolographic();
void lgpModalResonance();
void lgpWaveCollision();
void lgpBoxWave();
void lgpDiamondLattice();
void lgpHexagonalGrid();
void lgpSpiralVortex();
void lgpSierpinskiTriangles();
void lgpChevronWaves();
void lgpConcentricRings();
void lgpStarBurst();
void lgpMeshNetwork();

// LGP Color Mixing Effects declarations
void lgpColorTemperature();
void lgpRGBPrism();
void lgpComplementaryMixing();
void lgpAdditiveSubtractive();
void lgpQuantumColors();
void lgpDopplerShift();
void lgpChromaticAberration();
void lgpHSVCylinder();
void lgpPerceptualBlend();
void lgpMetamericColors();
void lgpColorAccelerator();
void lgpDNAHelix();
void lgpPhaseTransition();

// Include new LGP Advanced Effects
#include "LGPAdvancedEffects.h"

// Include new LGP Organic Effects  
#include "LGPOrganicEffects.h"

// Include new LGP Geometric Effects
#include "LGPGeometricEffects.h"

// Include new LGP Color Mixing Effects
#include "LGPColorMixingEffects.h"

#if FEATURE_AUDIO_EFFECTS && FEATURE_AUDIO_SYNC
#include "LGPAudioReactive.h"
#endif

// StripEffects registration class
class StripEffects {
public:
    static void registerAll(FxEngine& engine);
};

#endif // STRIP_EFFECTS_H
