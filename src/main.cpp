#include <Arduino.h>
#include <FastLED.h>
#include "config/features.h"
#include "config/hardware_config.h"
#include "core/EffectTypes.h"
#include "effects/strip/StripEffects.h"
#include "effects/strip/OptimizedStripEffects.h"
#include "effects/transitions/TransitionEngine.h"
#include "hardware/EncoderManager.h"
#include "hardware/EncoderLEDFeedback.h"

#if FEATURE_WEB_SERVER
#include "network/WebServer.h"
#include "network/WiFiManager.h"
#endif

#if FEATURE_PERFORMANCE_MONITOR
#include "utils/PerformanceOptimizer.h"
#endif

#include "core/AudioRenderTask.h"

#if FEATURE_SERIAL_MENU
#include "utils/SerialMenu.h"
#endif

#if FEATURE_AUDIO_SYNC
#include "audio/audio_sync.h"
#include "audio/AudioSystem.h"
#include "audio/audio_effects.h"
#include "audio/MicTest.h"
#include "audio/GoertzelUtils.h"
#endif


// LED Type Configuration
#define LED_TYPE WS2812
#define COLOR_ORDER GRB

// LED buffer allocation - Dual 160-LED strips (320 total)
// Matrix mode has been surgically removed
// ==================== LED STRIPS CONFIGURATION ====================
// Dual strip buffers for 160 LEDs each
CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
CRGB strip1_transition[HardwareConfig::STRIP1_LED_COUNT];
CRGB strip2_transition[HardwareConfig::STRIP2_LED_COUNT];

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

// Universal visual parameters for encoders 4-7
VisualParams visualParams;

// Advanced Transition System
TransitionEngine transitionEngine(HardwareConfig::NUM_LEDS);
CRGB transitionSourceBuffer[HardwareConfig::NUM_LEDS];
bool useRandomTransitions = true;

// Visual Feedback System
EncoderLEDFeedback* encoderFeedback = nullptr;

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

// Include palette definitions
#include "Palettes.h"

// M5Unit-Scroll encoder manager
#include "hardware/ScrollEncoderManager.h"

// Dual-Strip Wave Engine (disabled for now - files don't exist yet)
// #include "effects/waves/DualStripWaveEngine.h"
// #include "effects/waves/WaveEffects.h"
// #include "effects/waves/WaveEncoderControl.h"

// Global wave engine instance (disabled for now)
// DualStripWaveEngine waveEngine;

// Global I2C mutex for thread-safe Wire operations
SemaphoreHandle_t i2cMutex = NULL;

#if FEATURE_SERIAL_MENU
// Serial menu instance
SerialMenu serialMenu;
// Dummy wave engine for compatibility
DummyWave2D fxWave2D;
#endif


// Forward declare wrapper functions
void stripInterferenceWrapper();
void heartbeatEffectWrapper();
void breathingEffectWrapper();
void stripPlasmaWrapper();
void vortexEffectWrapper();

// Audio/render task callbacks
void audioUpdateCallback();
void renderUpdateCallback();
void effectUpdateCallback();


// Forward declare all effect functions
extern void solidColor();
extern void pulseEffect();
extern void confetti();
extern void stripConfetti();
extern void sinelon();
extern void juggle();
extern void stripJuggle();
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
extern void spectrumLightshowEngine();

// Include effects header
#include "effects.h"

// LGP Effect Declarations
#include "effects/strip/LGPInterferenceEffects.h"
#include "effects/strip/LGPGeometricEffects.h"
#include "effects/strip/LGPAdvancedEffects.h"
#include "effects/strip/LGPOrganicEffects.h"
#include "effects/strip/LGPQuantumEffects.h"
#include "effects/strip/LGPColorMixingEffects.h"
// #include "effects/strip/LGPAudioReactive.h"  // Removed: all old audio-reactive modes retired

// Effects array - Matrix mode has been surgically removed
Effect effects[] = {
    // =============== BASIC STRIP EFFECTS ===============
    // Signature effects with CENTER ORIGIN
    {"Fire", fire, EFFECT_TYPE_STANDARD},
    {"Ocean", stripOcean, EFFECT_TYPE_STANDARD},
    
    // Wave dynamics (already CENTER ORIGIN)
    {"Wave", waveEffect, EFFECT_TYPE_STANDARD},
    {"Ripple", rippleEffect, EFFECT_TYPE_STANDARD},
    
    // NEW Strip-specific CENTER ORIGIN effects
    {"Strip Confetti", stripConfetti, EFFECT_TYPE_STANDARD},
    {"Strip Juggle", stripJuggle, EFFECT_TYPE_STANDARD},
    {"Strip Interference", stripInterferenceWrapper, EFFECT_TYPE_STANDARD},
    {"Strip BPM", stripBPM, EFFECT_TYPE_STANDARD},
    {"Strip Plasma", stripPlasmaWrapper, EFFECT_TYPE_STANDARD},
    
    // Motion effects (ALL NOW CENTER ORIGIN COMPLIANT)
    {"Confetti", confetti, EFFECT_TYPE_STANDARD},
    {"Sinelon", sinelon, EFFECT_TYPE_STANDARD},
    {"Juggle", juggle, EFFECT_TYPE_STANDARD},
    {"BPM", bpm, EFFECT_TYPE_STANDARD},
    
    // Palette showcase
    {"Solid Blue", solidColor, EFFECT_TYPE_STANDARD},
    {"Pulse Effect", pulseEffect, EFFECT_TYPE_STANDARD},
    
    // =============== NEW CENTER ORIGIN EFFECTS ===============
    // These replace the rubbish wave effects with proper CENTER ORIGIN compliance
    {"Heartbeat", heartbeatEffectWrapper, EFFECT_TYPE_STANDARD},
    {"Breathing", breathingEffectWrapper, EFFECT_TYPE_STANDARD},
    {"Shockwave", shockwaveEffect, EFFECT_TYPE_STANDARD},
    {"Vortex", vortexEffectWrapper, EFFECT_TYPE_STANDARD},
    {"Collision", collisionEffect, EFFECT_TYPE_STANDARD},
    {"Gravity Well", gravityWellEffect, EFFECT_TYPE_STANDARD},
    
    // =============== LGP INTERFERENCE EFFECTS ===============
    // Specifically designed for Light Guide Plate optics
    {"LGP Box Wave", lgpBoxWave, EFFECT_TYPE_STANDARD},
    {"LGP Holographic", lgpHolographic, EFFECT_TYPE_STANDARD},
    {"LGP Modal Resonance", lgpModalResonance, EFFECT_TYPE_STANDARD},
    {"LGP Interference Scanner", lgpInterferenceScanner, EFFECT_TYPE_STANDARD},
    {"LGP Wave Collision", lgpWaveCollision, EFFECT_TYPE_STANDARD},
    
    // =============== LGP GEOMETRIC EFFECTS ===============
    // Advanced shapes leveraging waveguide physics
    {"LGP Diamond Lattice", lgpDiamondLattice, EFFECT_TYPE_STANDARD},
    {"LGP Hexagonal Grid", lgpHexagonalGrid, EFFECT_TYPE_STANDARD},
    {"LGP Spiral Vortex", lgpSpiralVortex, EFFECT_TYPE_STANDARD},
    {"LGP Sierpinski", lgpSierpinskiTriangles, EFFECT_TYPE_STANDARD},
    {"LGP Chevron Waves", lgpChevronWaves, EFFECT_TYPE_STANDARD},
    {"LGP Concentric Rings", lgpConcentricRings, EFFECT_TYPE_STANDARD},
    {"LGP Star Burst", lgpStarBurst, EFFECT_TYPE_STANDARD},
    {"LGP Mesh Network", lgpMeshNetwork, EFFECT_TYPE_STANDARD},
    
    // =============== LGP ADVANCED EFFECTS ===============
    // Next-gen optical patterns
    {"LGP Moiré Curtains", lgpMoireCurtains, EFFECT_TYPE_STANDARD},
    {"LGP Radial Ripple", lgpRadialRipple, EFFECT_TYPE_STANDARD},
    {"LGP Holographic Vortex", lgpHolographicVortex, EFFECT_TYPE_STANDARD},
    {"LGP Evanescent Drift", lgpEvanescentDrift, EFFECT_TYPE_STANDARD},
    {"LGP Chromatic Shear", lgpChromaticShear, EFFECT_TYPE_STANDARD},
    {"LGP Modal Cavity", lgpModalCavity, EFFECT_TYPE_STANDARD},
    {"LGP Fresnel Zones", lgpFresnelZones, EFFECT_TYPE_STANDARD},
    {"LGP Photonic Crystal", lgpPhotonicCrystal, EFFECT_TYPE_STANDARD},
    
    // =============== LGP ORGANIC EFFECTS ===============
    // Nature-inspired optical phenomena
    {"LGP Aurora Borealis", lgpAuroraBorealis, EFFECT_TYPE_STANDARD},
    {"LGP Bioluminescent", lgpBioluminescentWaves, EFFECT_TYPE_STANDARD},
    {"LGP Plasma Membrane", lgpPlasmaMembrane, EFFECT_TYPE_STANDARD},
    {"LGP Neural Network", lgpNeuralNetwork, EFFECT_TYPE_STANDARD},
    {"LGP Crystal Growth", lgpCrystallineGrowth, EFFECT_TYPE_STANDARD},
    {"LGP Fluid Dynamics", lgpFluidDynamics, EFFECT_TYPE_STANDARD},
    
    // =============== LGP QUANTUM EFFECTS ===============
    // Mind-bending physics simulations
    {"LGP Quantum Tunneling", lgpQuantumTunneling, EFFECT_TYPE_STANDARD},
    {"LGP Gravitational Lens", lgpGravitationalLensing, EFFECT_TYPE_STANDARD},
    {"LGP Sonic Boom", lgpSonicBoom, EFFECT_TYPE_STANDARD},
    {"LGP Time Crystal", lgpTimeCrystal, EFFECT_TYPE_STANDARD},
    {"LGP Soliton Waves", lgpSolitonWaves, EFFECT_TYPE_STANDARD},
    {"LGP Metamaterial Cloak", lgpMetamaterialCloaking, EFFECT_TYPE_STANDARD},
    
    // =============== LGP COLOR MIXING EFFECTS ===============
    {"LGP Color Temperature", lgpColorTemperature, EFFECT_TYPE_STANDARD},
    {"LGP RGB Prism", lgpRGBPrism, EFFECT_TYPE_STANDARD},
    {"LGP Complementary Mix", lgpComplementaryMixing, EFFECT_TYPE_STANDARD},
    {"LGP Additive/Subtractive", lgpAdditiveSubtractive, EFFECT_TYPE_STANDARD},
    {"LGP Quantum Colors", lgpQuantumColors, EFFECT_TYPE_STANDARD},
    {"LGP Doppler Shift", lgpDopplerShift, EFFECT_TYPE_STANDARD},
    {"LGP Chromatic Aberration", lgpChromaticAberration, EFFECT_TYPE_STANDARD},
    {"LGP HSV Cylinder", lgpHSVCylinder, EFFECT_TYPE_STANDARD},
    {"LGP Perceptual Blend", lgpPerceptualBlend, EFFECT_TYPE_STANDARD},
    {"LGP Metameric Colors", lgpMetamericColors, EFFECT_TYPE_STANDARD},
    {"LGP Color Accelerator", lgpColorAccelerator, EFFECT_TYPE_STANDARD},
    {"LGP DNA Helix", lgpDNAHelix, EFFECT_TYPE_STANDARD},
    {"LGP Phase Transition", lgpPhaseTransition, EFFECT_TYPE_STANDARD},
    
#if FEATURE_AUDIO_SYNC
    // =============== AUDIO REACTIVE EFFECTS ===============
    // Old music-reactive effects removed
    
    // =============== LGP AUDIO REACTIVE EFFECTS ===============
    // Removed LGP audio reactive modes – superseded by structured engine
    // Keep new structured engine below
    {"Spectrum LS Engine", spectrumLightshowEngine, EFFECT_TYPE_STANDARD},
#endif
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
    // Copy first half of leds buffer to strip1, second half to strip2
    memcpy(strip1, leds, HardwareConfig::STRIP1_LED_COUNT * sizeof(CRGB));
    memcpy(strip2, &leds[HardwareConfig::STRIP1_LED_COUNT], HardwareConfig::STRIP2_LED_COUNT * sizeof(CRGB));
}

void syncStripsToLeds() {
    // Copy strips back to unified buffer (for Light Guide effects that read current state)
    memcpy(leds, strip1, HardwareConfig::STRIP1_LED_COUNT * sizeof(CRGB));
    memcpy(&leds[HardwareConfig::STRIP1_LED_COUNT], strip2, HardwareConfig::STRIP2_LED_COUNT * sizeof(CRGB));
}

// ============== BASIC EFFECTS ==============
// Effects have been moved to src/effects/strip/StripEffects.cpp

// ============== EFFECT WRAPPERS ==============
// These functions dynamically choose between original and optimized versions

void stripInterferenceWrapper() {
    if (useOptimizedEffects) {
        stripInterferenceOptimized();
    } else {
        stripInterference();
    }
}

void heartbeatEffectWrapper() {
    if (useOptimizedEffects) {
        heartbeatEffectOptimized();
    } else {
        heartbeatEffect();
    }
}

void breathingEffectWrapper() {
    if (useOptimizedEffects) {
        breathingEffectOptimized();
    } else {
        breathingEffect();
    }
}

void stripPlasmaWrapper() {
    if (useOptimizedEffects) {
        stripPlasmaOptimized();
    } else {
        stripPlasma();
    }
}

void vortexEffectWrapper() {
    if (useOptimizedEffects) {
        vortexEffectOptimized();
    } else {
        vortexEffect();
    }
}


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
        
        // Vary duration based on transition type
        switch (transType) {
            case TRANSITION_FADE:
                duration = 800;
                curve = EASE_IN_OUT_QUAD;
                break;
            case TRANSITION_WIPE_OUT:
            case TRANSITION_WIPE_IN:
            case TRANSITION_WIPE_LR:
            case TRANSITION_WIPE_RL:
                duration = 1200;
                curve = EASE_OUT_CUBIC;
                break;
            case TRANSITION_DISSOLVE:
                duration = 1500;
                curve = EASE_LINEAR;
                break;
            case TRANSITION_ZOOM_IN:
            case TRANSITION_ZOOM_OUT:
                duration = 1000;
                curve = EASE_IN_OUT_ELASTIC;
                break;
            case TRANSITION_SHATTER:
                duration = 2000;
                curve = EASE_OUT_BOUNCE;
                break;
            case TRANSITION_MELT:
                duration = 1800;
                curve = EASE_IN_CUBIC;
                break;
            case TRANSITION_GLITCH:
                duration = 600;
                curve = EASE_LINEAR;
                break;
            case TRANSITION_PHASE_SHIFT:
                duration = 1400;
                curve = EASE_IN_OUT_CUBIC;
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
    
    // Debug transition type names
    const char* transitionNames[] = {
        "FADE", "WIPE_LR", "WIPE_RL", "WIPE_OUT", "WIPE_IN", 
        "DISSOLVE", "ZOOM_IN", "ZOOM_OUT", "MELT", "SHATTER", 
        "GLITCH", "PHASE_SHIFT"
    };
    Serial.printf("%s (%dms)\n", transitionNames[transType], duration);
    
    // Flash effect encoder to indicate transition
    if (encoderFeedback) {
        encoderFeedback->flashEncoder(0, 255, 255, 255, duration / 2);  // White flash for half transition duration
    }
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
    
    // Initialize dual LED strips
    FastLED.addLeds<LED_TYPE, HardwareConfig::STRIP1_DATA_PIN, COLOR_ORDER>(
        strip1, HardwareConfig::STRIP1_LED_COUNT);
    FastLED.addLeds<LED_TYPE, HardwareConfig::STRIP2_DATA_PIN, COLOR_ORDER>(
        strip2, HardwareConfig::STRIP2_LED_COUNT);
    FastLED.setBrightness(HardwareConfig::STRIP_BRIGHTNESS);
    
    Serial.println("Dual strip FastLED initialized");
    
    // Create I2C mutex for thread-safe operations
    i2cMutex = xSemaphoreCreateMutex();
    if (i2cMutex == NULL) {
        Serial.println("ERROR: Failed to create I2C mutex!");
    } else {
        Serial.println("I2C mutex created for thread-safe operations");
    }
    
    // Initialize I2C bus with scroll encoder pins (since 8encoder is disabled)
    Wire.begin(HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL);
    Wire.setClock(400000);  // 400kHz
    
    // Scan I2C bus to see what's connected
    Serial.println("\n🔍 Scanning I2C bus...");
    Serial.printf("   SDA: GPIO%d, SCL: GPIO%d\n", 
                  HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL);
    byte error, address;
    int nDevices = 0;
    for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("   Found device at 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            if (address == 0x40) Serial.print(" (M5Unit-Scroll)");
            else if (address == 0x41) Serial.print(" (M5Stack 8Encoder)");
            Serial.println();
            nDevices++;
        }
    }
    if (nDevices == 0) {
        Serial.println("   No I2C devices found!");
    } else {
        Serial.printf("   Found %d device(s)\n", nDevices);
    }
    Serial.println();
    
    // Initialize M5Unit-Scroll encoder manager
    if (scrollManager.begin()) {
        Serial.println("✅ Scroll encoder system initialized");
        
        // Set up callbacks to handle parameter changes
        scrollManager.setParamChangeCallback([](ScrollParameter param, uint8_t value) {
            switch (param) {
                case PARAM_BRIGHTNESS:
                    FastLED.setBrightness(value);
                    Serial.printf("Brightness: %d\n", value);
                    break;
                case PARAM_PALETTE:
                    currentPaletteIndex = value % gGradientPaletteCount;
                    targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
                    Serial.printf("Palette: %d\n", currentPaletteIndex);
                    break;
                case PARAM_SPEED:
                    paletteSpeed = value / 4;  // Scale to reasonable range
                    Serial.printf("Speed: %d\n", paletteSpeed);
                    break;
                default:
                    // Other parameters are handled by updateVisualParams
                    break;
            }
        });
        
        // Effect changes are handled internally by ScrollEncoderManager
    } else {
        Serial.println("⚠️  Scroll encoder not found - continuing without it");
    }
    
    // Commenting out M5Stack 8encoder initialization
    /*
    // Initialize I2C and M5ROTATE8 with dedicated task architecture
    Wire.begin(HardwareConfig::I2C_SDA, HardwareConfig::I2C_SCL);
    Wire.setClock(400000);  // Max supported speed (400 KHz)
    
    // Initialize encoder manager
    encoderManager.begin();
    
    // Initialize Visual Feedback System
    if (encoderManager.isAvailable()) {
        M5ROTATE8* encoder = encoderManager.getEncoder();
        if (encoder) {
            encoderFeedback = new EncoderLEDFeedback(encoder, &visualParams);
            encoderFeedback->applyDefaultColorScheme();
            Serial.println("✅ Visual Feedback System initialized");
        }
    } else {
        Serial.println("⚠️  VFS disabled - no encoder available");
    }
    */
    
    // Preset Management System - DISABLED FOR NOW
    // Will be re-enabled once basic transitions work
    
    // FastLED built-in optimizations
    FastLED.setCorrection(TypicalLEDStrip);  // Color correction for WS2812
    FastLED.setDither(BINARY_DITHER);         // Enable temporal dithering
    
    // Initialize strip mapping
    initializeStripMapping();
    
    // Initialize optimized effect lookup tables
    initOptimizedEffects();
    
    // Initialize palette
    currentPaletteIndex = 0;
    currentPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
    targetPalette = currentPalette;
    
    // Clear LEDs
    FastLED.clear(true);
    
    // Initialize M5Unit-Scroll encoder on secondary I2C bus
#if FEATURE_DEBUG_OUTPUT
    Serial.println("[DEBUG] About to initialize scroll encoder...");
#endif
    // DISABLED - CAUSING CRASHES
    // initScrollEncoder();
#if FEATURE_DEBUG_OUTPUT
    Serial.println("[DEBUG] Scroll encoder init SKIPPED - was causing crashes");
#endif
    
    // DISABLED - Setting up callbacks when encoder not available causes crashes
    // Only set up scroll encoder callbacks if main encoder is available
    /*
    if (encoderManager.isAvailable()) {
        setScrollEncoderCallbacks(
            // Value change callback - mirrors M5ROTATE8 encoder functions
            [](int32_t delta) {
                uint8_t mirroredEncoder = getScrollMirroredEncoder();
                
                // Create an encoder event to process through the same logic
                EncoderEvent event;
                event.encoder_id = mirroredEncoder;
                event.delta = delta;
                event.button_pressed = false;
                event.timestamp = millis();
                
                // Send to encoder event queue if available
                QueueHandle_t queue = encoderManager.getEventQueue();
                if (queue != NULL) {
                    xQueueSend(queue, &event, 0);
                    Serial.printf("SCROLL: Mirroring encoder %d, delta %d\n", mirroredEncoder, delta);
                }
            },
            // Button press callback - cycle through encoder modes
            []() {
                uint8_t currentMode = getScrollMirroredEncoder();
                uint8_t nextMode = (currentMode + 1) % 8;
                setScrollMirroredEncoder(nextMode);
                
                const char* modeName[] = {
                    "Effect", "Brightness", "Palette", "Speed",
                    "Intensity", "Saturation", "Complexity", "Variation"
                };
                
                Serial.printf("SCROLL: Switched to %s mode (encoder %d)\n", 
                             modeName[nextMode], nextMode);
            }
        );
    }
    */
    
    // Start the consolidated audio/render task FIRST for deterministic timing
    Serial.println("\n=== Starting Audio/Render Task (PRIORITY) ===");
    if (!audioRenderTask.start(audioUpdateCallback, renderUpdateCallback, effectUpdateCallback)) {
        Serial.println("⚠️  CRITICAL: Failed to start audio/render task!");
        Serial.println("⚠️  System performance will be degraded");
    } else {
        Serial.println("✅ Started 8ms audio/render task on Core 1");
    }
    
#if FEATURE_SERIAL_MENU
    // Initialize serial menu system
    serialMenu.begin();
#endif
    
    // Initialize WiFi Manager (non-blocking)
#if FEATURE_WEB_SERVER
    Serial.println("\n=== Starting Non-Blocking WiFi Manager ===");
    WiFiManager& wifiManager = WiFiManager::getInstance();
    
    // Configure WiFi
    wifiManager.setCredentials(NetworkConfig::WIFI_SSID, NetworkConfig::WIFI_PASSWORD);
    
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

    Serial.println("\n=== Setup Complete ===");
    Serial.println("🎭 Advanced Transition System Active");
    Serial.println("⚡ FastLED Optimizations ENABLED");
    Serial.println("   't' = Toggle random transitions");
    Serial.println("   'n' = Next effect with transition");
    Serial.println("   'o' = Toggle optimized effects");
    Serial.println("   'p' = Performance comparison (when available)");
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
    
    if (millis() - lastPaletteChange > 5000) {  // Change palette every 5 seconds
        lastPaletteChange = millis();
        currentPaletteIndex = (currentPaletteIndex + 1) % gGradientPaletteCount;
        targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
    }
    
    // Blend towards target palette
    nblendPaletteTowardPalette(currentPalette, targetPalette, 24);
}

void updateEncoderFeedback() {
    // Check both that encoderFeedback is not NULL AND encoder is available
    if (encoderFeedback != nullptr && encoderManager.isAvailable()) {
        // Update current effect info
        encoderFeedback->setCurrentEffect(currentEffect, effects[currentEffect].name);
        
        // Update performance metrics (simplified for now)
        float frameTime = 1000.0f / HardwareConfig::DEFAULT_FPS;
        float cpuUsage = (frameTime / 8.33f) * 100.0f;  // Estimate based on target 120 FPS
        encoderFeedback->updatePerformanceMetrics(cpuUsage, HardwareConfig::DEFAULT_FPS);
        
        // Update the LED feedback
        encoderFeedback->update();
    }
}

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
    if (effects[currentEffect].function) {
        effects[currentEffect].function();
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
    loopCounter++;
    
    // Track frame time statistics
    if (frameTime > 0 && frameTime < 1000000) {  // Sanity check
        totalFrameTime += frameTime;
        if (frameTime > maxFrameTime) maxFrameTime = frameTime;
        if (frameTime < minFrameTime) minFrameTime = frameTime;
    }
    
#if FEATURE_BUTTON_CONTROL
    // Use button control for boards that have buttons
    handleButton();
#endif

    // Simple transition controls via Serial
    if (Serial.available()) {
        char cmd = Serial.read();
        Serial.printf("*** RECEIVED COMMAND: '%c' (0x%02X) ***\n", cmd, cmd);
        switch (cmd) {
            case 't':
            case 'T':
                useRandomTransitions = !useRandomTransitions;
                Serial.printf("🎭 Random transitions: %s\n", 
                             useRandomTransitions ? "ENABLED" : "DISABLED");
                break;
            case 'n':
            case 'N':
                {
                    uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
                    startAdvancedTransition(nextEffect);
                }
                break;
            case 'o':
            case 'O':
                useOptimizedEffects = !useOptimizedEffects;
                Serial.printf("⚡ Optimized effects: %s\n", 
                             useOptimizedEffects ? "ENABLED" : "DISABLED");
                break;
            case 'p':
            case 'P':
                if (effects[currentEffect].name == "Strip Interference" ||
                    effects[currentEffect].name == "Heartbeat" ||
                    effects[currentEffect].name == "Breathing" ||
                    effects[currentEffect].name == "Strip Plasma" ||
                    effects[currentEffect].name == "Vortex") {
                    comparePerformance();
                } else {
                    Serial.println("Performance comparison not available for this effect");
                }
                break;
            case 'w':
            case 'W':
                #if FEATURE_WEB_SERVER
                Serial.println("\n=== Retrying WiFi Connection ===");
                webServer.stop();
                delay(100);
                if (webServer.begin()) {
                    Serial.println("✅ Web server restarted successfully");
                } else {
                    Serial.println("⚠️  Web server failed to restart");
                }
                #else
                Serial.println("Web server feature not enabled");
                #endif
                break;
            // Preset commands disabled for now
            // case 's':
            // case 'S':
            //     if (presetManager) {
            //         presetManager->quickSave(0);  // Save to slot 0
            //         Serial.println("💾 Quick saved current state");
            //     }
            //     break;
            // case 'l':
            // case 'L':
            //     if (presetManager) {
            //         presetManager->quickLoad(0);  // Load from slot 0
            //         Serial.println("📁 Quick loaded preset");
            //     }
            //     break;
        }
    }

    // Update scroll encoder
    scrollManager.update();
    
    // Update visual parameters from scroll encoder
    scrollManager.updateVisualParams(visualParams);
    
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
                    
                    // Update the effect callback in the audio/render task
                    audioRenderTask.setEffectCallback(effectUpdateCallback);
                    
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
                            Serial.printf("💡 MAIN: Brightness changed to %d\n", brightness);
                            if (encoderFeedback) encoderFeedback->flashEncoder(1, 255, 255, 255, 200);
                        }
                        break;
                        
                    case 2: // Palette selection
                        if (event.delta > 0) {
                            currentPaletteIndex = (currentPaletteIndex + 1) % gGradientPaletteCount;
                        } else {
                            currentPaletteIndex = currentPaletteIndex > 0 ? currentPaletteIndex - 1 : gGradientPaletteCount - 1;
                        }
                        targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
                        Serial.printf("🎨 MAIN: Palette changed to %d\n", currentPaletteIndex);
                        if (encoderFeedback) encoderFeedback->flashEncoder(2, 255, 0, 255, 200);
                        break;
                        
                    case 3: // Speed control
                        paletteSpeed = constrain(paletteSpeed + (event.delta > 0 ? 2 : -2), 1, 50);
                        Serial.printf("⚡ MAIN: Speed changed to %d\n", paletteSpeed);
                        if (encoderFeedback) encoderFeedback->flashEncoder(3, 255, 255, 0, 200);
                        break;
                        
                    case 4: // Intensity/Amplitude
                        visualParams.intensity = constrain(visualParams.intensity + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("🔥 MAIN: Intensity changed to %d (%.1f%%)\n", visualParams.intensity, visualParams.getIntensityNorm() * 100);
                        if (encoderFeedback) encoderFeedback->flashEncoder(4, 255, 128, 0, 200);
                        break;
                        
                    case 5: // Saturation
                        visualParams.saturation = constrain(visualParams.saturation + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("🎨 MAIN: Saturation changed to %d (%.1f%%)\n", visualParams.saturation, visualParams.getSaturationNorm() * 100);
                        if (encoderFeedback) encoderFeedback->flashEncoder(5, 0, 255, 255, 200);
                        break;
                        
                    case 6: // Complexity/Detail
                        visualParams.complexity = constrain(visualParams.complexity + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("✨ MAIN: Complexity changed to %d (%.1f%%)\n", visualParams.complexity, visualParams.getComplexityNorm() * 100);
                        if (encoderFeedback) encoderFeedback->flashEncoder(6, 128, 255, 0, 200);
                        break;
                        
                    case 7: // Variation/Mode
                        visualParams.variation = constrain(visualParams.variation + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("🔄 MAIN: Variation changed to %d (%.1f%%)\n", visualParams.variation, visualParams.getVariationNorm() * 100);
                        if (encoderFeedback) encoderFeedback->flashEncoder(7, 255, 0, 255, 200);
                        break;
                }
            }
        }
    }
    
#if FEATURE_SERIAL_MENU
    // Process serial commands for full system control
    serialMenu.update();
#endif
    
    // Update palette blending
    updatePalette();
    
    // Update encoder LED feedback
    updateEncoderFeedback();
    
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
        // Process encoder events if available
        if (encoderFeedback) {
            encoderFeedback->update();
        }
        
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
    
    // LED showing is now handled by the audio/render task
    
    // Advance the global hue
    EVERY_N_MILLISECONDS(20) {
        gHue++;
    }
    
    // WiFi status LED feedback
#if FEATURE_WEB_SERVER
    EVERY_N_SECONDS(2) {
        static WiFiManager::WiFiState lastWiFiState = WiFiManager::STATE_WIFI_INIT;
        WiFiManager& wifi = WiFiManager::getInstance();
        WiFiManager::WiFiState currentWiFiState = wifi.getState();
        
        // Update encoder LED based on WiFi state
        if (encoderFeedback && currentWiFiState != lastWiFiState) {
            switch (currentWiFiState) {
                case WiFiManager::STATE_WIFI_SCANNING:
                    encoderFeedback->flashEncoder(7, 0, 0, 255, 500); // Blue flash for scanning
                    break;
                case WiFiManager::STATE_WIFI_CONNECTING:
                    encoderFeedback->flashEncoder(7, 255, 255, 0, 300); // Yellow flash for connecting
                    break;
                case WiFiManager::STATE_WIFI_CONNECTED:
                    encoderFeedback->flashEncoder(7, 0, 255, 0, 1000); // Green flash for connected
                    break;
                case WiFiManager::STATE_WIFI_AP_MODE:
                    encoderFeedback->flashEncoder(7, 255, 128, 0, 2000); // Orange for AP mode
                    break;
                case WiFiManager::STATE_WIFI_FAILED:
                    encoderFeedback->flashEncoder(7, 255, 0, 0, 500); // Red flash for failed
                    break;
                default:
                    break;
            }
            lastWiFiState = currentWiFiState;
        }
    }
#endif
    
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
        if (encoderManager.isAvailable()) {
            const EncoderMetrics& metrics = encoderManager.getMetrics();
            Serial.printf("Encoder: %u queue depth, %.1f%% I2C success\n",
                         metrics.current_queue_depth,
                         metrics.i2c_transactions > 0 ? 
                         (100.0f * (metrics.i2c_transactions - metrics.i2c_failures) / metrics.i2c_transactions) : 100.0f);
        }
        
        // Update wave engine performance stats if using wave effects (disabled for now)
        // if (effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE) {
        //     updateWavePerformanceStats(waveEngine);
        // }
    }
    
    
    // NO DELAY - Run at maximum possible FPS for best performance
    // The ESP32-S3 and FastLED will naturally limit to a sustainable rate
    // Removing artificial delays allows the system to run at its full potential
}