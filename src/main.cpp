#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include "config/features.h"
#include "config/hardware_config.h"
#include "core/EffectTypes.h"
#include "effects/strip/StripEffects.h"
#include "effects/strip/OptimizedStripEffects.h"
#include "effects/transitions/TransitionEngine.h"
#include "effects/zones/ZoneComposer.h"

#if FEATURE_ENHANCEMENT_ENGINES
#include "effects/engines/ColorEngine.h"
#include "effects/engines/MotionEngine.h"
#include "effects/engines/BlendingEngine.h"
#endif

#if FEATURE_NARRATIVE_ENGINE
#include "core/NarrativeEngine.h"
#endif

#if FEATURE_WEB_SERVER
#include "network/WebServer.h"
#include "network/WiFiManager.h"
#endif

#if FEATURE_PERFORMANCE_MONITOR
#include "utils/PerformanceOptimizer.h"
#include "hardware/PerformanceMonitor.h"
#endif

// #include "core/AudioRenderTask.h"  // DISABLED - Audio system removed

#if FEATURE_SERIAL_MENU
#include "utils/SerialMenu.h"
#endif

#if FEATURE_AUDIO_SYNC
#include "audio/audio_sync.h"
#include "audio/AudioSystem.h"
#include "audio/audio_effects.h"
#include "audio/MicTest.h"
#include "audio/GoertzelUtils.h"
#include "audio/i2s_mic.h"
#endif


// LED Type Configuration
// WS2812 Dual-Strip: GPIO4 (Strip 1, 160 LEDs) + GPIO5 (Strip 2, 160 LEDs)
#define LED_TYPE WS2812
#define COLOR_ORDER GRB  // WS2812 uses GRB color order

// LED buffer allocation - Dual 160-LED strips (320 total)
// Matrix mode has been surgically removed
// ==================== LED STRIPS CONFIGURATION ====================
// Dual strip buffers for 160 LEDs each
CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
CRGB strip1_transition[HardwareConfig::STRIP1_LED_COUNT];
CRGB strip2_transition[HardwareConfig::STRIP2_LED_COUNT];

// Keep controller pointers for WS2812 diagnostics
CLEDController* ws2812_ctrl_strip1 = nullptr;
CLEDController* ws2812_ctrl_strip2 = nullptr;

// Combined virtual buffer for compatibility with EffectBase expectations
// EffectBase expects leds[NUM_LEDS] where NUM_LEDS = 320 for strips mode
// We create a unified buffer that can handle both strips
CRGB leds[HardwareConfig::NUM_LEDS];  // Full 320 LED buffer for EffectBase compatibility
CRGB transitionBuffer[HardwareConfig::NUM_LEDS];


// Strip-specific globals
HardwareConfig::SyncMode currentSyncMode = HardwareConfig::SYNC_SYNCHRONIZED;
HardwareConfig::PropagationMode currentPropagationMode = HardwareConfig::PROPAGATE_OUTWARD;

// Strip mapping arrays for spatial effects
uint8_t angles[HardwareConfig::NUM_LEDS];
uint8_t radii[HardwareConfig::NUM_LEDS];

// Effect parameters
uint8_t gHue = 0;
uint8_t currentEffect = 0;
uint8_t previousEffect = 0;
uint32_t lastButtonPress = 0;
uint8_t fadeAmount = 20;
uint8_t paletteSpeed = 10;
uint16_t fps = HardwareConfig::DEFAULT_FPS;
uint8_t brightnessVal = HardwareConfig::STRIP_BRIGHTNESS;

// Visual parameters (serial-controlled only, no encoder HMI)
VisualParams visualParams;

// Advanced Transition System
TransitionEngine transitionEngine(HardwareConfig::NUM_LEDS);
CRGB transitionSourceBuffer[HardwareConfig::NUM_LEDS];
bool useRandomTransitions = true;

// Zone Composer System
ZoneComposer zoneComposer;

// WebServer control variables
uint8_t effectSpeed = 10;              // Effect animation speed (1-50)
uint8_t effectIntensity = 255;         // Visual pipeline intensity (0-255)
uint8_t effectSaturation = 255;        // Visual pipeline saturation (0-255)
uint8_t effectComplexity = 128;        // Visual pipeline complexity (0-255)
uint8_t effectVariation = 128;         // Visual pipeline variation (0-255)

#if FEATURE_PERFORMANCE_MONITOR
PerformanceMonitor perfMon;
#endif

// LED strip state tracking
int strip1LitCount = 0;
int strip2LitCount = 0;

// Preset Management System - DISABLED
// PresetManager* presetManager = nullptr;

// Feature flag for optimized effects
bool useOptimizedEffects = true;

// Palette management
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;
uint8_t currentPaletteIndex = 0;
bool paletteAutoCycle = true;              // Toggle for auto-cycling palettes
uint32_t paletteCycleInterval = 5000;      // Interval in ms (default 5 seconds)

// Include master palette system (57 palettes with metadata)
#include "Palettes_Master.h"

#if FEATURE_ROTATE8_ENCODER
#include "hardware/EncoderManager.h"
#endif

// Dual-Strip Wave Engine (disabled for now - files don't exist yet)
// #include "effects/waves/DualStripWaveEngine.h"
// #include "effects/waves/WaveEffects.h"
// #include "effects/waves/WaveEncoderControl.h"

// Global wave engine instance (disabled for now)
// DualStripWaveEngine waveEngine;

// Global I2C mutex (stub for compilation - HMI disabled)
SemaphoreHandle_t i2cMutex = NULL;

// LED buffer mutex for thread-safe syncLedsToStrips() operations
SemaphoreHandle_t ledBufferMutex = NULL;

#if FEATURE_SERIAL_MENU
// Serial menu instance
SerialMenu serialMenu;
// Dummy wave engine for compatibility
DummyWave2D fxWave2D;
#endif


// Forward declare wrapper functions
// REMOVED: stripInterferenceWrapper, heartbeatEffectWrapper, breathingEffectWrapper, stripPlasmaWrapper, vortexEffectWrapper
// These were useless effects that have been purged

// Audio/render task callbacks
void audioUpdateCallback();
void renderUpdateCallback();
void effectUpdateCallback();


// Forward declare all effect functions
extern void solidColor();
extern void pulseEffect();
extern void confetti();
extern void sinelon();
extern void juggle();
extern void bpm();
extern void waveEffect();
extern void rippleEffect();
extern void stripInterference();
extern void stripBPM();
extern void stripPlasma();
extern void fire();
extern void ocean();
extern void stripOcean();
extern void heartbeatEffect();
extern void breathingEffect();
extern void shockwaveEffect();
extern void vortexEffect();
extern void collisionEffect();
extern void gravityWellEffect();
extern void lgpFFTColorMap();
extern void lgpHarmonicResonance();
extern void lgpStereoPhasePattern();
#if FEATURE_AUDIO_EFFECTS && FEATURE_AUDIO_SYNC
extern void spectrumLightshowEngine();
#endif

// Include effects header
#include "effects.h"

// LGP Effect Declarations
#include "effects/strip/LGPInterferenceEffects.h"
#include "effects/strip/LGPGeometricEffects.h"
#include "effects/strip/LGPAdvancedEffects.h"
#include "effects/strip/LGPOrganicEffects.h"
#include "effects/strip/LGPQuantumEffects.h"
#include "effects/strip/LGPColorMixingEffects.h"
#include "effects/strip/LGPNovelPhysics.h"
// #include "effects/strip/LGPAudioReactive.h"  // Removed: all old audio-reactive modes retired

// New Light Guide Plate Physics Effects
#include "effects/lightguide/LGPPhysicsEffects.h"

// Effects array - Matrix mode has been surgically removed
// USELESS EFFECTS PURGED - Only the good shit remains
Effect effects[] = {
// DISABLED: Enhanced effects have bugs causing hangs - need debugging
// #if FEATURE_ENHANCEMENT_ENGINES
//     // =============== ENHANCED EFFECTS (TOP OF LIST) ===============
//     // Week 3: ColorEngine - dual/triple palette blending and diffusion
//     {"Fire+", fireEnhanced, EFFECT_TYPE_STANDARD},
//     {"Ocean+", stripOceanEnhanced, EFFECT_TYPE_STANDARD},
//     {"LGP Holographic+", lgpHolographicEnhanced, EFFECT_TYPE_STANDARD},
//
//     // Week 4: MotionEngine - momentum physics, phase control, speed modulation
//     {"Shockwave+", shockwaveEnhanced, EFFECT_TYPE_STANDARD},
//     {"Collision+", collisionEnhanced, EFFECT_TYPE_STANDARD},
//     {"LGP Wave Collision+", lgpWaveCollisionEnhanced, EFFECT_TYPE_STANDARD},
// #endif

    // =============== QUALITY STRIP EFFECTS ===============
    // Signature effects with CENTER ORIGIN
    {"Fire", fire, EFFECT_TYPE_STANDARD},
    {"Ocean", stripOcean, EFFECT_TYPE_STANDARD},
    
    // Wave dynamics (already CENTER ORIGIN)
    {"Wave", waveEffect, EFFECT_TYPE_STANDARD},
    {"Ripple", rippleEffect, EFFECT_TYPE_STANDARD},
    
    // Motion effects worth keeping
    {"Sinelon", sinelon, EFFECT_TYPE_STANDARD},
    
    // =============== NEW CENTER ORIGIN EFFECTS ===============
    {"Shockwave", shockwaveEffect, EFFECT_TYPE_STANDARD},
    {"Collision", collisionEffect, EFFECT_TYPE_STANDARD},
    {"Gravity Well", gravityWellEffect, EFFECT_TYPE_STANDARD},
    
    // =============== LGP INTERFERENCE EFFECTS ===============
    // Specifically designed for Light Guide Plate optics
    {"LGP Holographic", lgpHolographic, EFFECT_TYPE_STANDARD},
    {"LGP Modal Resonance", lgpModalResonance, EFFECT_TYPE_STANDARD},
    {"LGP Interference Scanner", lgpInterferenceScanner, EFFECT_TYPE_STANDARD},
    {"LGP Wave Collision", lgpWaveCollision, EFFECT_TYPE_STANDARD},
    
    // =============== LGP GEOMETRIC EFFECTS ===============
    // Advanced shapes leveraging waveguide physics
    {"LGP Diamond Lattice", lgpDiamondLattice, EFFECT_TYPE_STANDARD},
    {"LGP Concentric Rings", lgpConcentricRings, EFFECT_TYPE_STANDARD},
    {"LGP Star Burst", lgpStarBurst, EFFECT_TYPE_STANDARD},
    
    // =============== LGP ADVANCED EFFECTS ===============
    // Next-gen optical patterns
    {"LGP Moiré Curtains", lgpMoireCurtains, EFFECT_TYPE_STANDARD},
    {"LGP Radial Ripple", lgpRadialRipple, EFFECT_TYPE_STANDARD},
    {"LGP Holographic Vortex", lgpHolographicVortex, EFFECT_TYPE_STANDARD},
    {"LGP Chromatic Shear", lgpChromaticShear, EFFECT_TYPE_STANDARD},
    {"LGP Fresnel Zones", lgpFresnelZones, EFFECT_TYPE_STANDARD},
    {"LGP Photonic Crystal", lgpPhotonicCrystal, EFFECT_TYPE_STANDARD},
    
    // =============== LGP ORGANIC EFFECTS ===============
    // Nature-inspired optical phenomena
    {"LGP Aurora Borealis", lgpAuroraBorealis, EFFECT_TYPE_STANDARD},
    {"LGP Bioluminescent", lgpBioluminescentWaves, EFFECT_TYPE_STANDARD},
    {"LGP Plasma Membrane", lgpPlasmaMembrane, EFFECT_TYPE_STANDARD},
    
    // =============== LGP QUANTUM EFFECTS ===============
    // Mind-bending physics simulations
    {"LGP Quantum Tunneling", lgpQuantumTunneling, EFFECT_TYPE_STANDARD},
    {"LGP Gravitational Lens", lgpGravitationalLensing, EFFECT_TYPE_STANDARD},
    {"LGP Time Crystal", lgpTimeCrystal, EFFECT_TYPE_STANDARD},
    {"LGP Metamaterial Cloak", lgpMetamaterialCloaking, EFFECT_TYPE_STANDARD},
    {"LGP GRIN Cloak", lgpGrinCloak, EFFECT_TYPE_STANDARD},
    {"LGP Caustic Fan", lgpCausticFan, EFFECT_TYPE_STANDARD},
    {"LGP Birefringent Shear", lgpBirefringentShear, EFFECT_TYPE_STANDARD},
    {"LGP Anisotropic Cloak", lgpAnisotropicCloak, EFFECT_TYPE_STANDARD},
    {"LGP Evanescent Skin", lgpEvanescentSkin, EFFECT_TYPE_STANDARD},
    
    // =============== LGP COLOR MIXING EFFECTS ===============
    {"LGP Chromatic Aberration", lgpChromaticAberration, EFFECT_TYPE_STANDARD},
    {"LGP Color Accelerator", lgpColorAccelerator, EFFECT_TYPE_STANDARD},
    
    // =============== LGP PHYSICS-BASED EFFECTS ===============
    // Advanced physics simulations for Light Guide Plate
    {"LGP Liquid Crystal", lgpLiquidCrystal, EFFECT_TYPE_STANDARD},
    {"LGP Prism Cascade", lgpPrismCascade, EFFECT_TYPE_STANDARD},
    {"LGP Silk Waves", lgpSilkWaves, EFFECT_TYPE_STANDARD},
    {"LGP Beam Collision", lgpBeamCollision, EFFECT_TYPE_STANDARD},
    {"LGP Laser Duel", lgpLaserDuel, EFFECT_TYPE_STANDARD},
    {"LGP Tidal Forces", lgpTidalForces, EFFECT_TYPE_STANDARD},

    // =============== LGP NOVEL PHYSICS EFFECTS ===============
    // Advanced effects exploiting dual-edge optical interference
    {"LGP Chladni Harmonics", lgpChladniHarmonics, EFFECT_TYPE_STANDARD},
    {"LGP Gravitational Chirp", lgpGravitationalWaveChirp, EFFECT_TYPE_STANDARD},
    {"LGP Quantum Entangle", lgpQuantumEntanglementCollapse, EFFECT_TYPE_STANDARD},
    {"LGP Mycelial Network", lgpMycelialNetwork, EFFECT_TYPE_STANDARD},
    {"LGP Riley Dissonance", lgpRileyDissonance, EFFECT_TYPE_STANDARD},

// Audio effects removed

};

const uint8_t NUM_EFFECTS = sizeof(effects) / sizeof(effects[0]);

// Initialize strip mapping for spatial effects
void initializeStripMapping() {
    // Linear strip mapping for dual strips
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float normalized = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Linear angle mapping for strips
        angles[i] = normalized * 255;
        
        // Distance from center point for outward propagation
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_HALF_LENGTH;
        radii[i] = distFromCenter * 255;
    }
}

// Synchronization functions for unified LED buffer
void syncLedsToStrips() {
    // CRITICAL: Thread-safe LED buffer synchronization
    // Protects against race conditions between effect rendering and FastLED.show()

    if (ledBufferMutex == NULL) {
        Serial.println("ERROR: LED buffer mutex not initialized!");
        return;  // Fail-safe: don't corrupt buffers
    }

    // Acquire mutex with timeout to prevent deadlocks
    if (xSemaphoreTake(ledBufferMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Copy first half of leds buffer to strip1, second half to strip2
        memcpy(strip1, leds, HardwareConfig::STRIP1_LED_COUNT * sizeof(CRGB));
        memcpy(strip2, &leds[HardwareConfig::STRIP1_LED_COUNT], HardwareConfig::STRIP2_LED_COUNT * sizeof(CRGB));

        xSemaphoreGive(ledBufferMutex);
    } else {
        Serial.println("WARNING: syncLedsToStrips() mutex timeout!");
        // Don't copy - safer to skip frame than corrupt buffer
    }
}

void syncStripsToLeds() {
    // CRITICAL: Thread-safe LED buffer synchronization
    // Protects against race conditions when reading current state

    if (ledBufferMutex == NULL) {
        Serial.println("ERROR: LED buffer mutex not initialized!");
        return;  // Fail-safe: don't corrupt buffers
    }

    // Acquire mutex with timeout to prevent deadlocks
    if (xSemaphoreTake(ledBufferMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Copy strips back to unified buffer (for Light Guide effects that read current state)
        memcpy(leds, strip1, HardwareConfig::STRIP1_LED_COUNT * sizeof(CRGB));
        memcpy(&leds[HardwareConfig::STRIP1_LED_COUNT], strip2, HardwareConfig::STRIP2_LED_COUNT * sizeof(CRGB));

        xSemaphoreGive(ledBufferMutex);
    } else {
        Serial.println("WARNING: syncStripsToLeds() mutex timeout!");
        // Don't copy - safer to skip frame than corrupt buffer
    }
}

// ============== BASIC EFFECTS ==============
// Effects have been moved to src/effects/strip/StripEffects.cpp

// ============== EFFECT WRAPPERS ==============
// REMOVED: All wrappers for shitty effects have been eliminated
// The purge is complete - only quality effects remain


// ============== ADVANCED TRANSITION SYSTEM ==============

void startAdvancedTransition(uint8_t newEffect) {
    Serial.println("*** TRANSITION FUNCTION CALLED ***");
    if (newEffect == currentEffect) return;
    
    // Configure transition engine for dual-strip mode
    transitionEngine.setDualStripMode(true, HardwareConfig::STRIP_LENGTH);
    
    // Save current LED state to source buffer
    memcpy(transitionSourceBuffer, leds, HardwareConfig::NUM_LEDS * sizeof(CRGB));
    
    // Switch to new effect
    previousEffect = currentEffect;
    currentEffect = newEffect;
    
    // Render one frame of the new effect to get target state
    effects[currentEffect].function();
    
    // CRITICAL: Sync strips to leds buffer so transition engine can see the effect
    syncStripsToLeds();
    
    // Select transition type
    TransitionType transType;
    uint32_t duration;
    EasingCurve curve;
    
    if (useRandomTransitions) {
        transType = TransitionEngine::getRandomTransition();
        
        // DEBUG: Force cycle through new transitions for testing
        static uint8_t forceTransition = 5; // Start with PULSEWAVE
        if (random8() < 50) { // 20% chance to use a new transition
            transType = (TransitionType)forceTransition;
            forceTransition++;
            if (forceTransition > 11) forceTransition = 5; // Cycle 5-11 (new transitions)
        }
        
        // Vary duration based on transition type
        switch (transType) {
            case TRANSITION_FADE:
                duration = 800;
                curve = EASE_IN_OUT_QUAD;
                break;
            case TRANSITION_WIPE_OUT:
            case TRANSITION_WIPE_IN:
                duration = 1200;
                curve = EASE_OUT_CUBIC;
                break;
            case TRANSITION_DISSOLVE:
                duration = 1500;
                curve = EASE_LINEAR;
                break;
            case TRANSITION_PHASE_SHIFT:
                duration = 1400;
                curve = EASE_IN_OUT_CUBIC;
                break;
            case TRANSITION_PULSEWAVE:
                duration = 2000;
                curve = EASE_OUT_QUAD;
                break;
            case TRANSITION_IMPLOSION:
                duration = 1500;
                curve = EASE_IN_CUBIC;
                break;
            case TRANSITION_IRIS:
                duration = 1200;
                curve = EASE_IN_OUT_QUAD;
                break;
            case TRANSITION_NUCLEAR:
                duration = 2500;
                curve = EASE_OUT_ELASTIC;
                break;
            case TRANSITION_STARGATE:
                duration = 3000;
                curve = EASE_IN_OUT_BACK;
                break;
            case TRANSITION_KALEIDOSCOPE:
                duration = 1800;
                curve = EASE_IN_OUT_CUBIC;
                break;
            case TRANSITION_MANDALA:
                duration = 2200;
                curve = EASE_IN_OUT_ELASTIC;
                break;
            default:
                duration = 1000;
                curve = EASE_IN_OUT_QUAD;
                break;
        }
    } else {
        // Simple fade for consistent behavior during testing
        transType = TRANSITION_FADE;
        duration = 1000;
        curve = EASE_IN_OUT_QUAD;
    }
    
    // Start the transition
    transitionEngine.startTransition(
        transitionSourceBuffer,  // Source: old effect
        leds,                   // Target: new effect  
        leds,                   // Output: back to leds buffer
        transType,
        duration,
        curve
    );
    
    Serial.printf("🎭 TRANSITION: %s → %s using ", 
                  effects[previousEffect].name, 
                  effects[currentEffect].name);
    
    // Debug transition type names - CENTER ORIGIN ONLY
    const char* transitionNames[] = {
        "FADE", "WIPE_OUT", "WIPE_IN", "DISSOLVE", 
        "PHASE_SHIFT", "PULSEWAVE", "IMPLOSION", 
        "IRIS", "NUCLEAR", "STARGATE", "KALEIDOSCOPE", "MANDALA"
    };
    Serial.printf("%s (%dms)\n", transitionNames[transType], duration);
}


void setup() {
    // Initialize serial with USB CDC wait
    Serial.begin(115200);
    
    // Wait for USB CDC to enumerate (ESP32-S3 specific)
    #ifdef ARDUINO_USB_CDC_ON_BOOT
    delay(2000);  // Give USB time to enumerate
    while (!Serial && millis() < 5000) {
        delay(10);  // Wait up to 5 seconds for serial
    }
    #endif
    
    delay(1000);
    
    Serial.println("\n=== LightwaveOS - Dual LED Strips ===");
    Serial.println("Matrix mode has been surgically removed");
    Serial.println("Mode: Dual 160-LED Strips");
    
    Serial.print("Strip 1 Pin: GPIO");
    Serial.println(HardwareConfig::STRIP1_DATA_PIN);
    Serial.print("Strip 2 Pin: GPIO");
    Serial.println(HardwareConfig::STRIP2_DATA_PIN);
    Serial.print("Total LEDs: ");
    Serial.println(HardwareConfig::TOTAL_LEDS);
    Serial.print("Strip Length: ");
    Serial.println(HardwareConfig::STRIP_LENGTH);
    Serial.print("Center Point: ");
    Serial.println(HardwareConfig::STRIP_CENTER_POINT);
    
    Serial.println("Board: ESP32-S3");
    Serial.print("Effects: ");
    Serial.println(NUM_EFFECTS);
    
    // Initialize power pin
    pinMode(HardwareConfig::POWER_PIN, OUTPUT);
    digitalWrite(HardwareConfig::POWER_PIN, HIGH);
    
#if FEATURE_BUTTON_CONTROL
    // Initialize button (only if board has button)
    pinMode(HardwareConfig::BUTTON_PIN, INPUT_PULLUP);
#endif
    
    // ============== COMPREHENSIVE LED STRIP INITIALIZATION WITH ERROR HANDLING ==============

    // Initialize WS2812 Dual-Strip Configuration
    {
        Serial.println("\n=== Initializing WS2812 Dual-Strip System ===");
        Serial.printf("Configuration: 2 strips x %d LEDs = %d total\n",
                      HardwareConfig::LEDS_PER_STRIP, HardwareConfig::TOTAL_LEDS);

        // Strip 1: GPIO4, first 160 LEDs of unified buffer
        Serial.printf("\nStrip 1: GPIO%d, %d LEDs (leds[0-%d])\n",
                      HardwareConfig::STRIP1_DATA_PIN,
                      HardwareConfig::STRIP1_LED_COUNT,
                      HardwareConfig::STRIP1_LED_COUNT - 1);
        ws2812_ctrl_strip1 = &FastLED.addLeds<LED_TYPE, HardwareConfig::STRIP1_DATA_PIN, COLOR_ORDER>(
            leds, 0, HardwareConfig::STRIP1_LED_COUNT);
        Serial.println("  Strip 1 registered");

        // Strip 2: GPIO5, second 160 LEDs of unified buffer
        Serial.printf("\nStrip 2: GPIO%d, %d LEDs (leds[%d-%d])\n",
                      HardwareConfig::STRIP2_DATA_PIN,
                      HardwareConfig::STRIP2_LED_COUNT,
                      HardwareConfig::STRIP1_LED_COUNT,
                      HardwareConfig::TOTAL_LEDS - 1);
        ws2812_ctrl_strip2 = &FastLED.addLeds<LED_TYPE, HardwareConfig::STRIP2_DATA_PIN, COLOR_ORDER>(
            leds, HardwareConfig::STRIP1_LED_COUNT, HardwareConfig::STRIP2_LED_COUNT);
        Serial.println("  Strip 2 registered");

        Serial.printf("\nTotal FastLED controllers: %d\n", FastLED.count());
    }

    // Configure brightness with validation
    {
        Serial.println("\n=== Configuring LED Brightness ===");

        uint8_t requested_brightness = HardwareConfig::STRIP_BRIGHTNESS;

        // Validate brightness value
        if (requested_brightness == 0) {
            Serial.println("WARNING: Brightness set to 0 (LEDs will be dark). Consider increasing.");
        } else if (requested_brightness > 255) {
            Serial.println("ERROR: Brightness exceeds maximum (255). Clamping to 255.");
            requested_brightness = 255;
        }

        FastLED.setBrightness(requested_brightness);
        Serial.printf("Brightness set to: %d/255\n", requested_brightness);
    }

    // Test LED communication with quick blink on both strips
    {
        Serial.println("\n=== Testing LED Communication ===");

        // Quick white blink to verify WS2812 communication
        fill_solid(leds, HardwareConfig::TOTAL_LEDS, CRGB::White);
        FastLED.show();
        delay(100);  // Brief white flash

        fill_solid(leds, HardwareConfig::TOTAL_LEDS, CRGB::Black);
        FastLED.show();

        Serial.println("LED communication test completed (both strips)");
    }

    // Final confirmation
    Serial.println("\n=== LED Initialization Complete ===");
    Serial.printf("Total LED controllers: %d\n", FastLED.count());
    Serial.println("Dual strip FastLED system ready\n");

#if FEATURE_PERFORMANCE_MONITOR
    perfMon.begin(HardwareConfig::DEFAULT_FPS);
#endif

    // Create LED buffer mutex for thread-safe LED synchronization
    ledBufferMutex = xSemaphoreCreateMutex();
    if (ledBufferMutex == NULL) {
        Serial.println("ERROR: Failed to create LED buffer mutex!");
    } else {
        Serial.println("LED buffer mutex created for thread-safe operations");
    }

    // ============== ENCODER INITIALIZATION ==============
#if FEATURE_ROTATE8_ENCODER
    Serial.println("\n=== Initializing M5Stack 8Encoder ===");

    // Create I2C mutex for thread-safe encoder operations
    i2cMutex = xSemaphoreCreateMutex();
    if (i2cMutex == NULL) {
        Serial.println("❌ ERROR: Failed to create I2C mutex!");
    } else {
        Serial.println("✅ I2C mutex created for thread-safe operations");
    }

    // Initialize I2C bus
    Wire.begin(HardwareConfig::I2C_SDA, HardwareConfig::I2C_SCL, 400000);
    Serial.printf("✅ I2C initialized: SDA=GPIO%d, SCL=GPIO%d @ 400kHz\n",
                  HardwareConfig::I2C_SDA, HardwareConfig::I2C_SCL);

    // Initialize encoder manager (creates I2C task on Core 0)
    if (encoderManager.begin()) {
        Serial.println("✅ Encoder manager initialized successfully");
        Serial.println("🎮 8 encoders available for real-time control");
    } else {
        Serial.println("⚠️  Encoder initialization failed - will retry in background");
    }
#else
    Serial.println("ℹ️ HMI disabled - serial control only");
#endif

    // Preset Management System - DISABLED FOR NOW
    // Will be re-enabled once basic transitions work
    
    // FastLED built-in optimizations
    FastLED.setCorrection(TypicalLEDStrip);  // Color correction for WS2812
    FastLED.setDither(BINARY_DITHER);         // Enable temporal dithering
    
    // Initialize strip mapping
    initializeStripMapping();
    
    // Initialize optimized effect lookup tables
    initOptimizedEffects();
    
    // Initialize palette (using master palette system with 57 palettes)
    currentPaletteIndex = 0;
    currentPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
    targetPalette = currentPalette;
    
    // Clear LEDs
    FastLED.clear(true);

    // Audio/render task DISABLED - Audio system removed
    /*
    // Start the consolidated audio/render task FIRST for deterministic timing
    Serial.println("\n=== Starting Audio/Render Task (PRIORITY) ===");
    if (!audioRenderTask.start(audioUpdateCallback, renderUpdateCallback, effectUpdateCallback)) {
        Serial.println("⚠️  CRITICAL: Failed to start audio/render task!");
        Serial.println("⚠️  System performance will be degraded");
    } else {
        Serial.println("✅ Started 8ms audio/render task on Core 1");
    }
    */
    
#if FEATURE_SERIAL_MENU
    // Initialize serial menu system
    serialMenu.begin();
#endif
    
    // Initialize WiFi Manager (non-blocking)
#if FEATURE_WEB_SERVER
    Serial.println("\n=== Starting Non-Blocking WiFi Manager ===");
    WiFiManager& wifiManager = WiFiManager::getInstance();
    
    // Configure WiFi
    wifiManager.setCredentials(NetworkConfig::WIFI_SSID_VALUE, NetworkConfig::WIFI_PASSWORD_VALUE);
    
    // Enable Soft-AP as immediate fallback
    wifiManager.enableSoftAP(NetworkConfig::AP_SSID, NetworkConfig::AP_PASSWORD);
    
    // Start WiFi manager task on Core 0
    if (wifiManager.begin()) {
        Serial.println("✅ WiFi Manager started on Core 0");
        Serial.println("📡 WiFi connection in progress (non-blocking)");
        Serial.println("🔄 Soft-AP available immediately if needed");
    } else {
        Serial.println("⚠️  WiFi Manager failed to start");
    }
    
    // Initialize Web Server (works with or without WiFi)
    Serial.println("\n=== Starting Web Server (Network-Independent) ===");
    if (webServer.begin()) {
        Serial.println("✅ Web server started successfully");
        Serial.println("📱 Server ready - will be accessible once network is up");
        Serial.println("   - WiFi: http://lightwaveos.local (once connected)");
        Serial.printf("   - AP: http://%s (fallback mode)\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("⚠️  Web server failed to start");
    }
#endif

#if FEATURE_AUDIO_SYNC
    // Initialize audio sync system
    Serial.println("\n=== Initializing Audio Synq ===");
    if (audioSynq.begin()) {
        Serial.println("✅ Audio synq initialized");
        Serial.println("✅ Audio synq ready for WebSocket commands");
    } else {
        Serial.println("⚠️ Audio synq initialization failed");
    }
    
    // Initialize AudioSystem (provides audio data to effects)
    AudioSync.begin();
    Serial.println("✅ AudioSystem initialized - mock data active");
    
    // Test I2S microphone
    Serial.println("\n=== Testing I2S Microphone ===");
    if (MicTest::testMicConnection()) {
        Serial.println("🎤 I2S microphone test PASSED - keeping driver active");
        
        // DON'T call startMicMonitoring() - the test already initialized the driver!
        // Instead, directly switch AudioSynq to use the already-initialized mic
        Serial.println("🔗 Connecting I2SMic to AudioSynq for real-time audio effects");
        audioSynq.setAudioSource(true);  // Switch to microphone mode
        
        Serial.println("✅ Real-time microphone audio active for LGP effects");
    } else {
        Serial.println("⚠️  Microphone test failed - using mock audio data only");
    }
#endif

    // ============== ZONE COMPOSER INITIALIZATION ==============
    Serial.println("\n=== Initializing Zone Composer ===");

    // FORCE 3-ZONE MODE: Always load Preset 2 (Triple Rings - 3 zones)
    // This ensures consistent 3-zone layout matching the webapp visualization
    Serial.println("Loading default 3-zone configuration (Preset 2: Triple Rings)");
    zoneComposer.loadPreset(2);

    // NOTE: Commented out NVS loading to prevent 4-zone override
    // User can manually load saved configs via serial: "zone load"
    // if (!zoneComposer.loadConfig()) {
    //     Serial.println("No saved config - loading default preset");
    //     zoneComposer.loadPreset(2);
    // }

    Serial.println("Zone Composer initialized in 3-ZONE mode (30+90+40 LEDs)");
    Serial.println("Use 'zone status' to view configuration");
    Serial.println("Use 'zone on' to enable zone mode");
    Serial.println("Use 'zone presets' to see available presets\n");

#if FEATURE_ENHANCEMENT_ENGINES
    // ============== ENHANCEMENT ENGINES INITIALIZATION ==============
    Serial.println("\n=== Initializing Visual Enhancement Engines ===");

    #if FEATURE_COLOR_ENGINE
    ColorEngine::getInstance().reset();
    Serial.println("✅ ColorEngine initialized (skeleton)");
    #endif

    #if FEATURE_MOTION_ENGINE
    MotionEngine::getInstance().enable();
    Serial.println("✅ MotionEngine initialized (skeleton)");
    #endif

    #if FEATURE_BLENDING_ENGINE
    BlendingEngine::getInstance().clearBuffers();
    Serial.println("✅ BlendingEngine initialized (skeleton)");
    #endif

    #if FEATURE_NARRATIVE_ENGINE
    NarrativeEngine::getInstance().enable();
    Serial.println("✅ NarrativeEngine initialized (dramatic timing)");
    #endif

    Serial.println("Enhancement engines ready - Week 1-2 skeleton phase\n");
#endif

    Serial.println("\n=== Setup Complete ===");
    Serial.println("🎭 Advanced Transition System Active");
    Serial.println("⚡ FastLED Optimizations ENABLED");
    Serial.println("\n📟 SERIAL COMMAND SYSTEM READY");
    Serial.println("   Press 'h' or '?' for command reference");
    Serial.println("   Quick start: Press 1-8 for parameter modes, +/- to adjust");
    Serial.println("");
    
#if FEATURE_DEBUG_OUTPUT
    Serial.println("[DEBUG] Entering main loop...");
#endif
}

void handleButton() {
    if (digitalRead(HardwareConfig::BUTTON_PIN) == LOW) {
        if (millis() - lastButtonPress > HardwareConfig::BUTTON_DEBOUNCE_MS) {
            lastButtonPress = millis();
            
            // Start transition to next effect
            uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
            startAdvancedTransition(nextEffect);
            
            Serial.print("Transitioning to: ");
            Serial.println(effects[nextEffect].name);
        }
    }
}

void updatePalette() {
    // Smoothly blend between palettes
    static uint32_t lastPaletteChange = 0;

    // Only auto-cycle if enabled
    if (paletteAutoCycle && (millis() - lastPaletteChange > paletteCycleInterval)) {
        lastPaletteChange = millis();
        currentPaletteIndex = (currentPaletteIndex + 1) % gMasterPaletteCount;
        targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
    }

    // Blend towards target palette
    nblendPaletteTowardPalette(currentPalette, targetPalette, 24);
}

// Manual palette control functions
void setPaletteIndex(uint8_t index) {
    if (index < gMasterPaletteCount) {
        currentPaletteIndex = index;
        targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
    }
}

void nextPalette() {
    currentPaletteIndex = (currentPaletteIndex + 1) % gMasterPaletteCount;
    targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
}

void prevPalette() {
    if (currentPaletteIndex == 0) {
        currentPaletteIndex = gMasterPaletteCount - 1;
    } else {
        currentPaletteIndex--;
    }
    targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
}

// Get effective brightness (capped by palette power limits)
uint8_t getEffectiveBrightness() {
    uint8_t paletteCap = getPaletteMaxBrightness(currentPaletteIndex);
    return min(brightnessVal, paletteCap);
}

// HMI REMOVED - updateEncoderFeedback() deleted

// ============== AUDIO/RENDER TASK CALLBACKS ==============

// Audio update callback - runs in 8ms task
void audioUpdateCallback() {
#if FEATURE_AUDIO_SYNC
    // Update audio systems
    audioSynq.update();
    AudioSync.update();
    // Provide latest 96-bin magnitudes to utility layer for visual engine
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    GoertzelUtils::setBinsPointer(frame.frequency_bins);
#endif
}

// Render update callback - runs in 8ms task
void renderUpdateCallback() {
#if FEATURE_NARRATIVE_ENGINE
    // Update dramatic timing (top layer - before physics)
    if (NarrativeEngine::getInstance().isEnabled()) {
        NarrativeEngine::getInstance().update();
    }
#endif

#if FEATURE_MOTION_ENGINE
    // Update motion physics before rendering
    if (MotionEngine::getInstance().isEnabled()) {
        MotionEngine::getInstance().update();
    }
#endif

    // Update transition system
    if (transitionEngine.isActive()) {
        syncStripsToLeds();
        transitionEngine.update();
        syncLedsToStrips();
    } else {
        syncStripsToLeds();
    }
}

// Effect update callback - runs in 8ms task
void effectUpdateCallback() {
    // Check if Zone Composer is enabled
    if (zoneComposer.isEnabled()) {
        // Render all zones
        zoneComposer.render();
    } else {
        // Legacy single-effect mode
        if (effects[currentEffect].function) {
            effects[currentEffect].function();
        }
    }
}

// Print serial command help
// ============== KEYBOARD CONTROL SYSTEM ==============
// Debug menu state machine (keyboard-driven since no HMI devices)
static enum { MENU_OFF, MENU_MAIN, MENU_PIPELINE } dbg_menu_state = MENU_OFF;

// Brightness limits for keyboard control
static constexpr uint8_t MIN_BRIGHTNESS = 8;
static constexpr uint8_t MAX_BRIGHTNESS = 200;
static constexpr uint8_t BRIGHTNESS_STEP = 8;

// Forward declarations for keystroke handling
static void handleSingleKeystroke(char ch);
static void handleMenuInput(char ch);
static void printMenuMain();
static void printMenuPipeline();

void printSerialHelp() {
    Serial.println("\n========== KEYBOARD CONTROLS ==========");
    Serial.println("  SPACEBAR  - Next effect");
    Serial.println("  BACKSPACE - Previous effect");
    Serial.println("  ] / [     - Next/prev palette");
    Serial.println("  + / =     - Brightness up");
    Serial.println("  - / _     - Brightness down");
    Serial.println("  > / .     - Speed up");
    Serial.println("  < / ,     - Speed down");
    Serial.println("  t         - Toggle random transitions");
    Serial.println("  m         - Toggle debug menu");
    Serial.println("  s         - Print status");
    Serial.println("  ?         - Show this help");
    Serial.println("========================================");
    Serial.println("String commands: type 'help' + Enter\n");
}

static void printMenuMain() {
    Serial.println("\n==== DEBUG MENU ====");
    Serial.printf("Transitions: %s\n", useRandomTransitions ? "RANDOM" : "FADE");
    Serial.printf("Optimized: %s\n", useOptimizedEffects ? "ON" : "OFF");
    Serial.println("--------------------");
    Serial.println("  1) Toggle random transitions");
    Serial.println("  2) Toggle optimized effects");
    Serial.println("  3) Pipeline settings...");
    Serial.println("  0) Close menu");
    Serial.println("====================\n");
}

static void printMenuPipeline() {
    Serial.println("\n-- Pipeline Settings --");
    Serial.printf("1) Intensity: %d\n", visualParams.intensity);
    Serial.printf("2) Saturation: %d\n", visualParams.saturation);
    Serial.printf("3) Complexity: %d\n", visualParams.complexity);
    Serial.printf("4) Variation: %d\n", visualParams.variation);
    Serial.println("0) Back to main");
    Serial.println("-----------------------\n");
}

static void handleMenuInput(char ch) {
    switch (dbg_menu_state) {
        case MENU_MAIN:
            if (ch == '1') {
                useRandomTransitions = !useRandomTransitions;
                Serial.printf("Transitions: %s\n", useRandomTransitions ? "RANDOM" : "FADE");
                printMenuMain();
            } else if (ch == '2') {
                useOptimizedEffects = !useOptimizedEffects;
                Serial.printf("Optimized: %s\n", useOptimizedEffects ? "ON" : "OFF");
                printMenuMain();
            } else if (ch == '3') {
                dbg_menu_state = MENU_PIPELINE;
                printMenuPipeline();
            } else if (ch == '0') {
                dbg_menu_state = MENU_OFF;
                Serial.println("Menu closed");
            }
            break;

        case MENU_PIPELINE:
            if (ch == '1') {
                visualParams.intensity = (visualParams.intensity + 32) % 256;
                Serial.printf("Intensity: %d\n", visualParams.intensity);
                printMenuPipeline();
            } else if (ch == '2') {
                visualParams.saturation = (visualParams.saturation + 32) % 256;
                Serial.printf("Saturation: %d\n", visualParams.saturation);
                printMenuPipeline();
            } else if (ch == '3') {
                visualParams.complexity = (visualParams.complexity + 32) % 256;
                Serial.printf("Complexity: %d\n", visualParams.complexity);
                printMenuPipeline();
            } else if (ch == '4') {
                visualParams.variation = (visualParams.variation + 32) % 256;
                Serial.printf("Variation: %d\n", visualParams.variation);
                printMenuPipeline();
            } else if (ch == '0') {
                dbg_menu_state = MENU_MAIN;
                printMenuMain();
            }
            break;

        default:
            break;
    }
}

static void handleSingleKeystroke(char ch) {
    // SPACEBAR - Next effect
    if (ch == ' ') {
        uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
        startAdvancedTransition(nextEffect);
        Serial.printf("Effect: [%d] %s\n", currentEffect, effects[currentEffect].name);
        return;
    }

    // BACKSPACE (127 or 8) - Previous effect
    if (ch == 127 || ch == 8) {
        uint8_t prevEffect = currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1;
        startAdvancedTransition(prevEffect);
        Serial.printf("Effect: [%d] %s\n", currentEffect, effects[currentEffect].name);
        return;
    }

    // ] - Next palette
    if (ch == ']') {
        currentPaletteIndex = (currentPaletteIndex + 1) % gMasterPaletteCount;
        targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
        Serial.printf("Palette: [%d/%d]\n", currentPaletteIndex, gMasterPaletteCount - 1);
        return;
    }

    // [ - Previous palette
    if (ch == '[') {
        currentPaletteIndex = (currentPaletteIndex == 0) ? (gMasterPaletteCount - 1) : (currentPaletteIndex - 1);
        targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
        Serial.printf("Palette: [%d/%d]\n", currentPaletteIndex, gMasterPaletteCount - 1);
        return;
    }

    // + or = - Brightness up
    if (ch == '+' || ch == '=') {
        int32_t nb = (int32_t)brightnessVal + BRIGHTNESS_STEP;
        if (nb > MAX_BRIGHTNESS) nb = MAX_BRIGHTNESS;
        brightnessVal = (uint8_t)nb;
        FastLED.setBrightness(brightnessVal);
        Serial.printf("Brightness: %d\n", brightnessVal);
        return;
    }

    // - or _ - Brightness down
    if (ch == '-' || ch == '_') {
        int32_t nb = (int32_t)brightnessVal - BRIGHTNESS_STEP;
        if (nb < MIN_BRIGHTNESS) nb = MIN_BRIGHTNESS;
        brightnessVal = (uint8_t)nb;
        FastLED.setBrightness(brightnessVal);
        Serial.printf("Brightness: %d\n", brightnessVal);
        return;
    }

    // > or . - Speed up
    if (ch == '>' || ch == '.') {
        int32_t ns = (int32_t)paletteSpeed + 4;
        if (ns > 50) ns = 50;
        paletteSpeed = (uint8_t)ns;
        Serial.printf("Speed: %d\n", paletteSpeed);
        return;
    }

    // < or , - Speed down
    if (ch == '<' || ch == ',') {
        int32_t ns = (int32_t)paletteSpeed - 4;
        if (ns < 1) ns = 1;
        paletteSpeed = (uint8_t)ns;
        Serial.printf("Speed: %d\n", paletteSpeed);
        return;
    }

    // t - Toggle transitions
    if (ch == 't' || ch == 'T') {
        useRandomTransitions = !useRandomTransitions;
        Serial.printf("Transitions: %s\n", useRandomTransitions ? "RANDOM" : "FADE");
        return;
    }

    // m - Toggle menu
    if (ch == 'm' || ch == 'M') {
        if (dbg_menu_state == MENU_OFF) {
            dbg_menu_state = MENU_MAIN;
            printMenuMain();
        } else {
            dbg_menu_state = MENU_OFF;
            Serial.println("Menu closed");
        }
        return;
    }

    // s - Status
    if (ch == 's' || ch == 'S') {
        Serial.printf("\n--- STATUS ---\n");
        Serial.printf("Effect: [%d] %s\n", currentEffect, effects[currentEffect].name);
        Serial.printf("Palette: %d/%d\n", currentPaletteIndex, gMasterPaletteCount - 1);
        Serial.printf("Brightness: %d\n", brightnessVal);
        Serial.printf("Speed: %d\n", paletteSpeed);
        Serial.printf("Heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("--------------\n");
        return;
    }

    // ? - Help
    if (ch == '?') {
        printSerialHelp();
        return;
    }

    // h - Help
    if (ch == 'h' || ch == 'H') {
        printSerialHelp();
        return;
    }

    // n - Next effect (alias)
    if (ch == 'n' || ch == 'N') {
        uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
        startAdvancedTransition(nextEffect);
        Serial.printf("Effect: [%d] %s\n", currentEffect, effects[currentEffect].name);
        return;
    }

#if FEATURE_NARRATIVE_ENGINE
    // y - NarrativeEngine status, Y - toggle on/off
    if (ch == 'y') {
        NarrativeEngine::getInstance().printStatus();
        return;
    }
    if (ch == 'Y') {
        if (NarrativeEngine::getInstance().isEnabled()) {
            NarrativeEngine::getInstance().disable();
            Serial.println(F("NarrativeEngine DISABLED"));
        } else {
            NarrativeEngine::getInstance().enable();
            Serial.println(F("NarrativeEngine ENABLED"));
        }
        return;
    }
#endif

    // Menu input handling (when menu is open)
    if (dbg_menu_state != MENU_OFF) {
        handleMenuInput(ch);
    }
}

void handleSerial() {
    while (Serial.available()) {
        char ch = Serial.peek();

        // Check for quick keys (single-char immediate commands)
        bool isSingleChar = (Serial.available() == 1);
        bool isQuickKey = (ch == ' ' || ch == '[' || ch == ']' || ch == '+' || ch == '=' ||
                          ch == '-' || ch == '_' || ch == '>' || ch == '<' || ch == '.' ||
                          ch == ',' || ch == '?' || ch == 127 || ch == 8);
        bool isMenuDigit = (dbg_menu_state != MENU_OFF && ch >= '0' && ch <= '9');
        bool isMenuToggle = (ch == 'm' || ch == 'M');
        bool isStatusKey = ((ch == 's' || ch == 'S') && Serial.available() == 1);
        bool isToggleKey = ((ch == 't' || ch == 'T') && Serial.available() == 1);
        bool isHelpKey = ((ch == 'h' || ch == 'H' || ch == 'n' || ch == 'N') && Serial.available() == 1);

        if (isSingleChar || isQuickKey || isMenuDigit || isMenuToggle || isStatusKey || isToggleKey || isHelpKey) {
            ch = Serial.read();
            handleSingleKeystroke(ch);
            continue;
        }

        // String command processing
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.startsWith("effect ")) {
            uint8_t idx = cmd.substring(7).toInt();
            if (idx < NUM_EFFECTS) {
                startAdvancedTransition(idx);
                Serial.printf("Effect: %s\n", effects[idx].name);
            } else {
                Serial.printf("Invalid effect index (0-%d)\n", NUM_EFFECTS - 1);
            }
        }
        else if (cmd.startsWith("brightness ") || cmd.startsWith("bri ")) {
            int sp = cmd.indexOf(' ');
            uint8_t val = cmd.substring(sp + 1).toInt();
            brightnessVal = constrain(val, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
            FastLED.setBrightness(brightnessVal);
            Serial.printf("Brightness: %d\n", brightnessVal);
        }
        else if (cmd.startsWith("palette ") || cmd.startsWith("pal ")) {
            int sp = cmd.indexOf(' ');
            uint16_t idx = cmd.substring(sp + 1).toInt();
            if (idx < gMasterPaletteCount) {
                currentPaletteIndex = idx;
                targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
                Serial.printf("Palette: %d\n", idx);
            } else {
                Serial.printf("Invalid palette index (0-%d)\n", gMasterPaletteCount - 1);
            }
        }
        else if (cmd.startsWith("speed ")) {
            uint8_t val = constrain(cmd.substring(6).toInt(), 1, 50);
            paletteSpeed = val;
            Serial.printf("Speed: %d\n", paletteSpeed);
        }
        else if (cmd == "status") {
            Serial.printf("Effect: [%d] %s\n", currentEffect, effects[currentEffect].name);
            Serial.printf("Brightness: %d\n", brightnessVal);
            Serial.printf("Palette: %d\n", currentPaletteIndex);
            Serial.printf("Speed: %d\n", paletteSpeed);
            Serial.printf("Heap: %d bytes\n", ESP.getFreeHeap());
        }
        else if (cmd == "effects" || cmd == "list") {
            Serial.println("\n=== EFFECTS LIST ===");
            for (uint8_t i = 0; i < NUM_EFFECTS; i++) {
                Serial.printf("  %2d: %s%s\n", i, effects[i].name, (i == currentEffect) ? " *" : "");
            }
            Serial.println("====================\n");
        }
        else if (cmd == "next") {
            uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
            startAdvancedTransition(nextEffect);
            Serial.printf("Effect: %s\n", effects[currentEffect].name);
        }
        else if (cmd == "prev") {
            uint8_t prevEffect = currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1;
            startAdvancedTransition(prevEffect);
            Serial.printf("Effect: %s\n", effects[currentEffect].name);
        }
        else if (cmd == "zone status") {
            zoneComposer.printStatus();
        }
        else if (cmd == "zone on") {
            zoneComposer.enable();
            Serial.println("✅ Zone Composer enabled");
        }
        else if (cmd == "zone off") {
            zoneComposer.disable();
            Serial.println("✅ Zone Composer disabled (single-effect mode)");
        }
        else if (cmd.startsWith("zone ") && cmd.indexOf(" effect ") > 0) {
            // Parse: zone <id> effect <effect_id>
            int spaceIdx = cmd.indexOf(' ', 5);
            uint8_t zoneId = cmd.substring(5, spaceIdx).toInt();
            uint8_t effectId = cmd.substring(spaceIdx + 8).toInt();
            zoneComposer.setZoneEffect(zoneId, effectId);
        }
        else if (cmd.startsWith("zone ") && cmd.indexOf(" enable") > 0) {
            // Parse: zone <id> enable
            uint8_t zoneId = cmd.substring(5, cmd.indexOf(' ', 5)).toInt();
            zoneComposer.enableZone(zoneId, true);
        }
        else if (cmd.startsWith("zone ") && cmd.indexOf(" disable") > 0) {
            // Parse: zone <id> disable
            uint8_t zoneId = cmd.substring(5, cmd.indexOf(' ', 5)).toInt();
            zoneComposer.enableZone(zoneId, false);
        }
        else if (cmd.startsWith("zone count ")) {
            // Parse: zone count <1-4>
            uint8_t count = cmd.substring(11).toInt();  // "zone count " = 11 chars
            if (count >= 1 && count <= 4) {
                zoneComposer.setZoneCount(count);
                Serial.printf("✅ Zone count set to %d\n", count);
            } else {
                Serial.println("❌ Invalid count (1-4)");
            }
        }
        else if (cmd.startsWith("zone preset ")) {
            // Parse: zone preset <0-4>
            uint8_t presetId = cmd.substring(12).toInt();  // "zone preset " = 12 chars
            if (presetId < 5) {
                if (zoneComposer.loadPreset(presetId)) {
                    Serial.printf("✅ Loaded preset %d: %s\n", presetId, zoneComposer.getPresetName(presetId));
                } else {
                    Serial.println("❌ Failed to load preset");
                }
            } else {
                Serial.println("❌ Invalid preset ID (0-4)");
            }
        }
        else if (cmd == "zone presets") {
            // List all available presets
            Serial.println("\n=== AVAILABLE ZONE PRESETS ===");
            for (uint8_t i = 0; i < 5; i++) {
                Serial.printf("  %d: %s\n", i, zoneComposer.getPresetName(i));
            }
            Serial.println("Usage: zone preset <0-4>\n");
        }
        else if (cmd == "zone save") {
            // Save current configuration to NVS
            if (zoneComposer.saveConfig()) {
                Serial.println("✅ Zone configuration saved");
            } else {
                Serial.println("❌ Failed to save configuration");
            }
        }
        else if (cmd == "zone load") {
            // Load configuration from NVS
            if (zoneComposer.loadConfig()) {
                Serial.println("✅ Zone configuration loaded");
            } else {
                Serial.println("⚠️  No saved configuration found - using defaults");
            }
        }
        else if (cmd == "help") {
            Serial.println("\n=== STRING COMMANDS ===");
            Serial.printf("  effect <0-%d>      - Set effect by index\n", NUM_EFFECTS - 1);
            Serial.printf("  brightness <8-200> - Set brightness\n");
            Serial.printf("  palette <0-%d>     - Set palette by index\n", gMasterPaletteCount - 1);
            Serial.println("  speed <1-50>       - Set animation speed");
            Serial.println("  next               - Next effect");
            Serial.println("  prev               - Previous effect");
            Serial.println("  effects / list     - Show all effects");
            Serial.println("  status             - Show current state");
            Serial.println("  help               - This message");
            Serial.println("\n=== ZONE COMMANDS ===");
            Serial.println("  zone status              - Show zone configuration");
            Serial.println("  zone on/off              - Enable/disable zone mode");
            Serial.println("  zone count <1-4>         - Set number of zones");
            Serial.printf("  zone <0-3> effect <0-%d> - Assign effect to zone\n", NUM_EFFECTS - 1);
            Serial.println("  zone <0-3> enable        - Enable specific zone");
            Serial.println("  zone <0-3> disable       - Disable specific zone");
            Serial.println("  zone presets             - List available presets");
            Serial.println("  zone preset <0-4>        - Load preset configuration");
            Serial.println("  zone save                - Save config to NVS");
            Serial.println("  zone load                - Load config from NVS");
            Serial.println("========================\n");
            printSerialHelp();
        }
        else if (cmd.length() > 0) {
            Serial.println("Unknown command. Type 'help' for list.");
        }
    }
}

void loop() {
    static uint32_t loopCounter = 0;
    static uint32_t lastDebugPrint = 0;
    static uint32_t frameStartTime = 0;
    static uint32_t totalFrameTime = 0;
    static uint32_t maxFrameTime = 0;
    static uint32_t minFrameTime = 999999;
    
    // Precise frame timing measurement
    uint32_t currentTime = micros();
    uint32_t frameTime = currentTime - frameStartTime;
    frameStartTime = currentTime;
    
    // Debug print every 5 seconds
    if (millis() - lastDebugPrint > 5000) {
        lastDebugPrint = millis();
        float fps = (float)loopCounter / 5.0f;  // Convert to per-second rate
        float avgFrameTime = (loopCounter > 0) ? (float)totalFrameTime / (float)loopCounter : 0;
        float avgFPS = (avgFrameTime > 0) ? 1000000.0f / avgFrameTime : 0;
        Serial.printf("[PERF] FPS: %.1f (avg: %.1f), Frame time: %.1fµs (min: %luµs, max: %luµs), Effect: %s\n", 
                      fps, avgFPS, avgFrameTime, minFrameTime, maxFrameTime, effects[currentEffect].name);
        
        // Additional debug: Check if any LEDs are lit
        bool anyLit = false;
        for (int i = 0; i < 10; i++) {  // Check first 10 LEDs
            if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0) {
                anyLit = true;
                break;
            }
        }
#if FEATURE_DEBUG_OUTPUT
        Serial.printf("[DEBUG] LEDs lit: %s, Brightness: %d\n", 
                      anyLit ? "YES" : "NO", FastLED.getBrightness());
#endif
        
        loopCounter = 0;
        totalFrameTime = 0;
        maxFrameTime = 0;
        minFrameTime = 999999;
    }

#if FEATURE_PERFORMANCE_MONITOR
    perfMon.startFrame();
#endif
    loopCounter++;
    
    // Track frame time statistics
    if (frameTime > 0 && frameTime < 1000000) {  // Sanity check
        totalFrameTime += frameTime;
        if (frameTime > maxFrameTime) maxFrameTime = frameTime;
        if (frameTime < minFrameTime) minFrameTime = frameTime;
    }
    
    // Handle serial commands (keyboard-driven control)
    handleSerial();

#if FEATURE_ROTATE8_ENCODER
    // Process encoder events from I2C task (NON-BLOCKING)
    QueueHandle_t encoderEventQueue = encoderManager.getEventQueue();
    if (encoderEventQueue != NULL) {
        EncoderEvent event;
        
        // Process all available events in queue (non-blocking)
        while (xQueueReceive(encoderEventQueue, &event, 0) == pdTRUE) {
            char buffer[64];
            
            // Special handling for encoder 0 (effect selection) - ALWAYS process normally
            if (event.encoder_id == 0) {
                // Effect selection - one effect change per physical detent click
                uint8_t newEffect;
                if (event.delta > 0) {
                    newEffect = (currentEffect + 1) % NUM_EFFECTS;
                } else {
                    newEffect = currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1;
                }
                
                // Only transition if actually changing effects
                if (newEffect != currentEffect) {
                    startAdvancedTransition(newEffect);
                    Serial.printf("🎨 MAIN: Effect changed to %s (index %d)\n", effects[currentEffect].name, currentEffect);

                    // Log mode transition if effect type changed
                    if (effects[newEffect].type != effects[currentEffect].type) {
                        Serial.printf("🔄 MAIN: Encoder mode switched to %s\n", 
                                     effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE ? "WAVE ENGINE" : "STANDARD");
                    }
                }
                
            } else if (effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE) {
                // Wave engine parameter control for encoders 1-7 (disabled for now)
                // handleWaveEncoderInput(event.encoder_id, event.delta, waveEngine);
                Serial.printf("🌊 WAVE: Would handle encoder %d, delta %d\n", event.encoder_id, event.delta);
            } else {
                // Standard effect control for encoders 1-7
                switch (event.encoder_id) {
                    case 1: // Brightness
                        {
                            int brightness = FastLED.getBrightness() + (event.delta > 0 ? 16 : -16);
                            brightness = constrain(brightness, 16, 255);
                            FastLED.setBrightness(brightness);
                            brightnessVal = brightness;
                            Serial.printf("💡 MAIN: Brightness changed to %d\n", brightness);
                            // if (encoderFeedback) encoderFeedback->flashEncoder(1, 255, 255, 255, 200);
                        }
                        break;
                        
                    case 2: // Palette selection
                        if (event.delta > 0) {
                            currentPaletteIndex = (currentPaletteIndex + 1) % gMasterPaletteCount;
                        } else {
                            currentPaletteIndex = currentPaletteIndex > 0 ? currentPaletteIndex - 1 : gMasterPaletteCount - 1;
                        }
                        targetPalette = CRGBPalette16(gMasterPalettes[currentPaletteIndex]);
                        Serial.printf("🎨 MAIN: Palette changed to %d\n", currentPaletteIndex);
                        // if (encoderFeedback) encoderFeedback->flashEncoder(2, 255, 0, 255, 200);
                        break;
                        
                    case 3: // Speed control
                        paletteSpeed = constrain(paletteSpeed + (event.delta > 0 ? 2 : -2), 1, 50);
                        Serial.printf("⚡ MAIN: Speed changed to %d\n", paletteSpeed);
                        break;
                        
                    case 4: // Intensity/Amplitude
                        visualParams.intensity = constrain(visualParams.intensity + (event.delta > 0 ? 4 : -4), 0, 255);
                        Serial.printf("🔥 MAIN: Intensity changed to %d (%.1f%%)\n", visualParams.intensity, visualParams.getIntensityNorm() * 100);
                        break;
                        
                    case 5: // Saturation
                        visualParams.saturation = constrain(visualParams.saturation + (event.delta > 0 ? 4 : -4), 0, 255);
                        Serial.printf("🎨 MAIN: Saturation changed to %d (%.1f%%)\n", visualParams.saturation, visualParams.getSaturationNorm() * 100);
                        break;
                        
                    case 6: // Complexity/Detail
                        visualParams.complexity = constrain(visualParams.complexity + (event.delta > 0 ? 4 : -4), 0, 255);
                        Serial.printf("✨ MAIN: Complexity changed to %d (%.1f%%)\n", visualParams.complexity, visualParams.getComplexityNorm() * 100);
                        break;

                    case 7: // Variation/Mode
                        visualParams.variation = constrain(visualParams.variation + (event.delta > 0 ? 4 : -4), 0, 255);
                        Serial.printf("🔄 MAIN: Variation changed to %d (%.1f%%)\n", visualParams.variation, visualParams.getVariationNorm() * 100);
                        break;
                }
            }
        }
    }
    
#endif // FEATURE_ROTATE8_ENCODER

#if FEATURE_SERIAL_MENU
    // Process serial commands for full system control
#if FEATURE_PERFORMANCE_MONITOR
    perfMon.startSection();
#endif
    serialMenu.update();
#if FEATURE_PERFORMANCE_MONITOR
    perfMon.endSerialProcessing();
#endif
#endif
    
    // Update palette blending
    updatePalette();
    
    // HMI REMOVED - encoder feedback disabled
    
    // Update web server
#if FEATURE_WEB_SERVER
    webServer.update();
#endif

    // Process scroll encoder events
    // processScrollEncoder(); // Removed - old implementation
    
    // Audio and rendering are now handled by the dedicated task
    // No need to update them in the main loop
    
    // Main loop runs at full speed, timing is handled by FastLED
    EVERY_N_MILLISECONDS(20) {
        
#if FEATURE_DEBUG_OUTPUT
        static unsigned long lastDebugPrint = 0;
        if (millis() - lastDebugPrint > 1000) {  // Only print once per second
            lastDebugPrint = millis();
            Serial.printf("[DEBUG] Strip1 lit: %d/%d, Strip2 lit: %d/%d, gHue: %d, Brightness: %d\n",
                      strip1LitCount, HardwareConfig::STRIP_LENGTH, strip2LitCount, HardwareConfig::STRIP_LENGTH, gHue, FastLED.getBrightness());
        
        // Also check if any LED in the unified buffer is lit
        int unifiedLitCount = 0;
        for (int i = 0; i < HardwareConfig::TOTAL_LEDS; i++) {
            if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0) {
                unifiedLitCount++;
            }
        }
            Serial.printf("[DEBUG] Unified buffer lit: %d/%d\n", unifiedLitCount, HardwareConfig::TOTAL_LEDS);
        }
#endif
    }
    
    // Call render callbacks directly (audio/render task disabled)
#if FEATURE_PERFORMANCE_MONITOR
    perfMon.startSection();
#endif
    effectUpdateCallback();  // Run the current effect
    renderUpdateCallback();  // Handle transitions
#if FEATURE_PERFORMANCE_MONITOR
    perfMon.endEffectProcessing();
#endif
    
    // Enforce palette-based brightness cap (Phase 3: power safety)
    uint8_t effectiveBrightness = getEffectiveBrightness();
    if (FastLED.getBrightness() > effectiveBrightness) {
        FastLED.setBrightness(effectiveBrightness);
    }

    // Show LEDs (audio/render task disabled)
#if FEATURE_PERFORMANCE_MONITOR
    perfMon.startSection();
#endif
    FastLED.show();
#if FEATURE_PERFORMANCE_MONITOR
    perfMon.endFastLEDShow();
#endif

#if FEATURE_AUDIO_SYNC
    i2sMic.markLedFrameComplete(micros());
#endif
    
    // Advance the global hue
    EVERY_N_MILLISECONDS(20) {
        gHue++;
    }
    
    // HMI REMOVED - WiFi status LED feedback disabled
    
    // Status every 30 seconds
    EVERY_N_SECONDS(30) {
        Serial.print("Effect: ");
        Serial.print(effects[currentEffect].name);
        Serial.print(", Palette: ");
        Serial.print(currentPaletteIndex);
        Serial.print(", Free heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");
        
#if FEATURE_WEB_SERVER
        // WiFi status
        WiFiManager& wifi = WiFiManager::getInstance();
        Serial.printf("WiFi: %s", wifi.getStateString().c_str());
        if (wifi.isConnected()) {
            Serial.printf(" - %s (%d dBm)", wifi.getSSID().c_str(), wifi.getRSSI());
        }
        Serial.println();
#endif
        
        // Quick encoder performance summary
        // Commented out - using scroll encoder instead
        /*
        if (encoderManager.isAvailable()) {
            const EncoderMetrics& metrics = encoderManager.getMetrics();
            Serial.printf("Encoder: %u queue depth, %.1f%% I2C success\n",
                         metrics.current_queue_depth,
                         metrics.i2c_transactions > 0 ? 
                         (100.0f * (metrics.i2c_transactions - metrics.i2c_failures) / metrics.i2c_transactions) : 100.0f);
        }
        */
        
        // Update wave engine performance stats if using wave effects (disabled for now)
        // if (effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE) {
        //     updateWavePerformanceStats(waveEngine);
        // }
    }
    
    
#if FEATURE_PERFORMANCE_MONITOR
    perfMon.endFrame();
#endif

    // NO DELAY - Run at maximum possible FPS for best performance
    // The ESP32-S3 and FastLED will naturally limit to a sustainable rate
    // Removing artificial delays allows the system to run at its full potential
}
