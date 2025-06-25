#include <Arduino.h>
#include <FastLED.h>
#include "config/features.h"
#include "config/hardware_config.h"
#include "core/EffectTypes.h"
#include "core/MegaLUTs.h"
#include "effects/strip/StripEffects.h"
#include "effects/transitions/TransitionEngine.h"
#include "effects/LUTOptimizedEffects.h"
#include "hardware/EncoderManager.h"
#include "hardware/EncoderLEDFeedback.h"

#if FEATURE_PERFORMANCE_MONITOR
#include "utils/PerformanceOptimizer.h"
#endif

#if FEATURE_SERIAL_MENU
#include "utils/SerialMenu.h"
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

// Pre-calculated distance lookup table for CENTER ORIGIN effects
// NOTE: These are now defined in MegaLUTs.cpp as part of the massive LUT system
extern uint8_t* distanceFromCenterLUT;
float normalizedDistance[HardwareConfig::NUM_LEDS];

// Legacy LUT declarations for backward compatibility
// These map to the new MegaLUT system
#define distanceFromCenter distanceFromCenterLUT
#define wavePatternLUT wavePatternLUT[0]  // Use first wave pattern
#define spiralAngleLUT spiralAngleLUT      // Direct mapping

// LUT-optimized effect instances
LUTPlasmaEffect* lutPlasmaEffect;
LUTFireEffect* lutFireEffect;
LUTWaveEffect* lutWaveEffect;
LUTMandelbrotEffect* lutMandelbrotEffect;
LUTParticleEffect* lutParticleEffect;
LUTPerlinNoiseEffect* lutPerlinEffect;
LUTComplexWaveEffect* lutComplexWaveEffect;
LUTShaderEffect* lutShaderEffect;
LUTTransitionShowcase* lutTransitionEffect;
LUTFrequencyEffect* lutFrequencyEffect;

// Initialize legacy LUTs - minimal since MegaLUTs handles everything
void initializeLUTs() {
    // Legacy compatibility - MegaLUTs handles all pre-calculations
    // This function is kept for backward compatibility only
    Serial.println("[LUT] Legacy LUT initialization skipped - using MegaLUTs");
    }
}

// Strip-specific globals
HardwareConfig::SyncMode currentSyncMode = HardwareConfig::SYNC_SYNCHRONIZED;
HardwareConfig::PropagationMode currentPropagationMode = HardwareConfig::PROPAGATE_OUTWARD;

// Strip mapping arrays for spatial effects
uint8_t angles[HardwareConfig::NUM_LEDS];
uint8_t radii[HardwareConfig::NUM_LEDS];

// Color palette LUT for fast lookups
CRGB paletteLUT[256];  // Current palette expanded to 256 entries
static uint32_t lastPaletteUpdate = 0;

void updatePaletteLUT() {
    // Only update if palette changed
    static CRGBPalette16 lastPalette;
    if (memcmp(&currentPalette, &lastPalette, sizeof(CRGBPalette16)) != 0 || 
        millis() - lastPaletteUpdate > 100) {
        
        for (uint16_t i = 0; i < 256; i++) {
            paletteLUT[i] = ColorFromPalette(currentPalette, i, 255);
        }
        memcpy(&lastPalette, &currentPalette, sizeof(CRGBPalette16));
        lastPaletteUpdate = millis();
    }
}

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

// Preset Management System - DISABLED
// PresetManager* presetManager = nullptr;

// Palette management
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;
uint8_t currentPaletteIndex = 0;

// Include palette definitions
#include "Palettes.h"

// M5Unit-Scroll encoder on secondary I2C bus
#include "hardware/scroll_encoder.h"

// Dual-Strip Wave Engine (disabled for now - files don't exist yet)
// #include "effects/waves/DualStripWaveEngine.h"
// #include "effects/waves/WaveEffects.h"
// #include "effects/waves/WaveEncoderControl.h"

// Global wave engine instance (disabled for now)
// DualStripWaveEngine waveEngine;

#if FEATURE_SERIAL_MENU
// Serial menu instance
SerialMenu serialMenu;
// Dummy wave engine for compatibility
DummyWave2D fxWave2D;
#endif

// Effect function pointer type
typedef void (*EffectFunction)();

// Array of effects - forward declaration
enum EffectType {
    EFFECT_TYPE_STANDARD,
    EFFECT_TYPE_WAVE_ENGINE
};

struct Effect {
    const char* name;
    EffectFunction function;
    EffectType type;
};

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

// LUT-Optimized Effect Wrappers
void lutPlasmaWrapper() {
    if (lutPlasmaEffect) {
        lutPlasmaEffect->update();
        // Handle encoder changes through the effect
        static int16_t lastEncoderValues[8] = {0};
        for (uint8_t i = 3; i < 8; i++) {
            int16_t currentValue = encoderManager.getEncoder()->getCounter(i);
            int16_t delta = currentValue - lastEncoderValues[i];
            if (delta != 0) {
                lutPlasmaEffect->onEncoderChange(i, delta);
                lastEncoderValues[i] = currentValue;
            }
        }
    }
}

void lutFireWrapper() {
    if (lutFireEffect) lutFireEffect->update();
}

void lutWaveWrapper() {
    if (lutWaveEffect) lutWaveEffect->update();
}

void lutMandelbrotWrapper() {
    if (lutMandelbrotEffect) lutMandelbrotEffect->update();
}

void lutParticleWrapper() {
    if (lutParticleEffect) lutParticleEffect->update();
}

void lutPerlinWrapper() {
    if (lutPerlinEffect) lutPerlinEffect->update();
}

void lutComplexWaveWrapper() {
    if (lutComplexWaveEffect) lutComplexWaveEffect->update();
}

void lutShaderWrapper() {
    if (lutShaderEffect) lutShaderEffect->update();
}

void lutTransitionWrapper() {
    if (lutTransitionEffect) lutTransitionEffect->update();
}

void lutFrequencyWrapper() {
    if (lutFrequencyEffect) lutFrequencyEffect->update();
}

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
    {"Strip Interference", stripInterference, EFFECT_TYPE_STANDARD},
    {"Strip BPM", stripBPM, EFFECT_TYPE_STANDARD},
    {"Strip Plasma", stripPlasma, EFFECT_TYPE_STANDARD},
    
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
    {"Heartbeat", heartbeatEffect, EFFECT_TYPE_STANDARD},
    {"Breathing", breathingEffect, EFFECT_TYPE_STANDARD},
    {"Shockwave", shockwaveEffect, EFFECT_TYPE_STANDARD},
    {"Vortex", vortexEffect, EFFECT_TYPE_STANDARD},
    {"Collision", collisionEffect, EFFECT_TYPE_STANDARD},
    {"Gravity Well", gravityWellEffect, EFFECT_TYPE_STANDARD},
    
    // =============== LUT-OPTIMIZED ULTRA PERFORMANCE EFFECTS ===============
    {"LUT Plasma", lutPlasmaWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Fire", lutFireWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Wave", lutWaveWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Mandelbrot", lutMandelbrotWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Particles", lutParticleWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Perlin", lutPerlinWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Complex", lutComplexWaveWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Shader", lutShaderWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Transitions", lutTransitionWrapper, EFFECT_TYPE_STANDARD},
    {"LUT Frequency", lutFrequencyWrapper, EFFECT_TYPE_STANDARD}
};

const uint8_t NUM_EFFECTS = sizeof(effects) / sizeof(effects[0]);

// Initialize strip mapping for spatial effects
void initializeStripMapping() {
    // Pre-calculate all distance values to avoid per-frame calculations
    for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        uint16_t stripPos = i % HardwareConfig::STRIP_LENGTH;
        float normalized = (float)stripPos / HardwareConfig::STRIP_LENGTH;
        
        // Linear angle mapping for strips
        angles[i] = normalized * 255;
        
        // Distance from center point for outward propagation
        float distFromCenter = abs((float)stripPos - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_HALF_LENGTH;
        radii[i] = distFromCenter * 255;
        
        // Pre-calculate absolute and normalized distances
        // Note: distanceFromCenter is now part of MegaLUTs
        normalizedDistance[i] = distFromCenter;
    }
}

// Synchronization functions optimized for speed
// Use pointer aliasing to avoid unnecessary copies where possible
static bool stripsAreSynced = true;

void syncLedsToStrips() {
    // Only copy if we've been working in the unified buffer
    if (!stripsAreSynced) {
        memcpy(strip1, leds, HardwareConfig::STRIP1_LED_COUNT * sizeof(CRGB));
        memcpy(strip2, &leds[HardwareConfig::STRIP1_LED_COUNT], HardwareConfig::STRIP2_LED_COUNT * sizeof(CRGB));
        stripsAreSynced = true;
    }
}

void syncStripsToLeds() {
    // Only copy if we've been working in the strip buffers
    if (stripsAreSynced) {
        memcpy(leds, strip1, HardwareConfig::STRIP1_LED_COUNT * sizeof(CRGB));
        memcpy(&leds[HardwareConfig::STRIP1_LED_COUNT], strip2, HardwareConfig::STRIP2_LED_COUNT * sizeof(CRGB));
        stripsAreSynced = false;
    }
}

// ============== BASIC EFFECTS ==============
// Effects have been moved to src/effects/strip/StripEffects.cpp


// ============== ADVANCED TRANSITION SYSTEM ==============

void startAdvancedTransition(uint8_t newEffect) {
    if (newEffect == currentEffect) return;
    
    // Prevent rapid transitions - minimum 100ms between transitions
    static uint32_t lastTransitionTime = 0;
    uint32_t now = millis();
    if (now - lastTransitionTime < 100) {
        return;  // Too soon, ignore
    }
    lastTransitionTime = now;
    
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
        
        // Vary duration based on transition type - OPTIMIZED FOR SMOOTH TRAILING FADES
        switch (transType) {
            case TRANSITION_FADE:
                duration = 600;  // Faster for smoother experience
                curve = EASE_IN_OUT_QUAD;
                break;
            case TRANSITION_WIPE_OUT:
            case TRANSITION_WIPE_IN:
                duration = 800;  // Reduced from 1200
                curve = EASE_OUT_CUBIC;
                break;
            case TRANSITION_PHASE_SHIFT:
                duration = 1000;  // Reduced from 1400
                curve = EASE_IN_OUT_CUBIC;
                break;
            case TRANSITION_SPIRAL:
                duration = 1200;  // Reduced from 2000
                curve = EASE_IN_OUT_QUAD;  // Changed from elastic
                break;
            case TRANSITION_RIPPLE:
                duration = 1500;  // Reduced from 2500
                curve = EASE_OUT_QUAD;  // Changed from cubic
                break;
            default:
                duration = 800;
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
        "FADE", "WIPE_OUT", "WIPE_IN", "PHASE_SHIFT",
        "SPIRAL", "RIPPLE"
    };
    Serial.printf("%s (%dms)\n", transitionNames[transType], duration);
    
    // Flash effect encoder to indicate transition
    if (encoderFeedback) {
        encoderFeedback->flashEncoder(0, 255, 255, 255, duration / 2);  // White flash for half transition duration
    }
}


void setup() {
    // Initialize serial with proper USB CDC setup
    Serial.begin(115200);
    delay(1000);
    while (!Serial && millis() < 3000) {
        delay(10);  // Wait up to 3 seconds for USB
    }
    
    Serial.println("\n=== Light Crystals - Dual LED Strips ===");
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
    
    // Initialize I2C at MAXIMUM speed
    Wire.begin(HardwareConfig::I2C_SDA, HardwareConfig::I2C_SCL);
    Wire.setClock(1000000);  // 1 MHz for maximum performance
    
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
    
    // Preset Management System - DISABLED FOR NOW
    // Will be re-enabled once basic transitions work
    
    FastLED.setCorrection(TypicalLEDStrip);
    
    // Initialize strip mapping and LUTs
    initializeStripMapping();
    
    // Initialize MEGA LUT System - MAXIMUM PERFORMANCE!
    Serial.println("\n[LUT] Initializing MEGA LUT System for maximum performance...");
    initializeMegaLUTs();  // This will use 200-250KB of RAM for LUTs
    
    // Initialize LUT-optimized effect instances
    lutPlasmaEffect = new LUTPlasmaEffect();
    lutFireEffect = new LUTFireEffect();
    lutWaveEffect = new LUTWaveEffect();
    lutMandelbrotEffect = new LUTMandelbrotEffect();
    lutParticleEffect = new LUTParticleEffect();
    lutPerlinEffect = new LUTPerlinNoiseEffect();
    lutComplexWaveEffect = new LUTComplexWaveEffect();
    lutShaderEffect = new LUTShaderEffect();
    lutTransitionEffect = new LUTTransitionShowcase();
    lutFrequencyEffect = new LUTFrequencyEffect();
    
    Serial.println("[LUT] LUT-optimized effects initialized");
    
    // Legacy LUT initialization (minimal, for backward compatibility)
    initializeLUTs();
    
    // Initialize palette
    currentPaletteIndex = 0;
    currentPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
    targetPalette = currentPalette;
    
    // Clear LEDs
    FastLED.clear(true);
    
    // Initialize M5Unit-Scroll encoder on secondary I2C bus
    Serial.println("[DEBUG] About to initialize scroll encoder...");
    // Re-enable with improved error handling
    initScrollEncoder();
    if (scrollEncoderAvailable) {
        Serial.println("[DEBUG] Scroll encoder initialized successfully");
    } else {
        Serial.println("[DEBUG] Scroll encoder not available - continuing without it");
    }
    
    // Set up scroll encoder callbacks
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
                // Serial.printf("SCROLL: Mirroring encoder %d, delta %d\n", mirroredEncoder, delta);  // Removed spam
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
            
            // Serial.printf("SCROLL: Switched to %s mode (encoder %d)\n", 
            //              modeName[nextMode], nextMode);  // Removed spam
        }
    );
    
#if FEATURE_SERIAL_MENU
    // Initialize serial menu system
    serialMenu.begin();
#endif
    
    Serial.println("=== Setup Complete ===");
    Serial.println("🎭 Advanced Transition System Active");
    // Serial.println("💾 Preset Management System Active");
    Serial.println("   't' = Toggle random transitions");
    Serial.println("   'n' = Next effect with transition");
    // Serial.println("   's' = Quick save preset");
    // Serial.println("   'l' = Quick load preset");
    Serial.println("");
    Serial.println("[DEBUG] Entering main loop with MAXIMUM single-core performance...");
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
    if (encoderFeedback && encoderManager.isAvailable()) {
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

void loop() {
    static uint32_t loopCounter = 0;
    static uint32_t lastDebugPrint = 0;
    
    // Debug print every 10 seconds (reduced frequency)
    if (millis() - lastDebugPrint > 10000) {
        lastDebugPrint = millis();
        Serial.printf("[DEBUG] Loop: %lu iterations, Effect: %s\n", loopCounter, effects[currentEffect].name);
        loopCounter = 0;
    }
    loopCounter++;
    
#if FEATURE_BUTTON_CONTROL
    // Use button control for boards that have buttons
    handleButton();
#endif

    // Simple transition controls via Serial
    if (Serial.available()) {
        char cmd = Serial.read();
        Serial.printf("*** RECEIVED COMMAND: '%c' ***\n", cmd);
        switch (cmd) {
            case 'h':
            case 'H':
                Serial.println("\n🎮 === LIGHT CRYSTALS SERIAL COMMANDS ===");
                Serial.println("📋 Available Commands:");
                Serial.println("  'n' or 'N' - Next effect (cycles through all effects)");
                Serial.println("  't' or 'T' - Toggle random transitions on/off");
#if FEATURE_WIRELESS_ENCODERS
                Serial.println("  'w' or 'W' - Start wireless encoder pairing");
#endif
                Serial.println("  'h' or 'H' - Show this help menu");
                Serial.println("\n🎨 Current Status:");
                Serial.printf("  Effect: %s (%d/%d)\n", effects[currentEffect].name, currentEffect + 1, NUM_EFFECTS);
                Serial.printf("  Transitions: %s\n", useRandomTransitions ? "RANDOM" : "FADE ONLY");
                Serial.printf("  Palette: %d\n", currentPaletteIndex);
                Serial.println("\n🎯 Hardware:");
                Serial.println("  • M5Stack 8-Encoder: GPIO 13/14");
                Serial.println("  • M5Unit-Scroll: GPIO 15/21");
                Serial.println("  • LED Strips: GPIO 11/12 (160 LEDs each)");
#if FEATURE_WIRELESS_ENCODERS
                Serial.printf("  • Wireless Encoders: %s\n", 
                    encoderManager.isWirelessConnected() ? "CONNECTED" : "Ready");
#endif
                Serial.println("=========================================\n");
                break;
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
#if FEATURE_WIRELESS_ENCODERS
            case 'w':
            case 'W':
                encoderManager.startWirelessPairing();
                break;
#endif
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

    // Process encoder events from I2C task (NON-BLOCKING)
    QueueHandle_t encoderEventQueue = encoderManager.getEventQueue();
    if (encoderEventQueue != NULL) {
        EncoderEvent event;
        
        // Limit events processed per loop to prevent backlog
        int eventsProcessed = 0;
        const int MAX_EVENTS_PER_LOOP = 3;
        
        // Process ALL available events in queue to prevent backlog
        // This ensures no lag from queued encoder inputs
        while (xQueueReceive(encoderEventQueue, &event, 0) == pdTRUE) {
            eventsProcessed++;
            
            // Batch process after collecting events
            if (eventsProcessed >= 10) {
                break; // Process in next frame to maintain 120 FPS
            }
            char buffer[64];
            
            // Special handling for encoder 0 (effect selection) - ALWAYS process normally
            if (event.encoder_id == 0) {
                // Ignore input if transition is active to prevent "caught between" issue
                if (transitionEngine.isActive()) {
                    continue;  // Silently ignore
                }
                
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
                    // Serial.printf("🎨 MAIN: Effect changed to %s (index %d)\n", effects[currentEffect].name, currentEffect);  // Reduced spam
                    
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
    
    // Update palette blending and LUT
    updatePalette();
    updatePaletteLUT();
    
    // Update encoder LED feedback
    updateEncoderFeedback();
    
    // Update transition system with optimized buffer management
    if (transitionEngine.isActive()) {
        // During transition: generate new effect frame and let transition engine handle blending
        effects[currentEffect].function();
        
        // Update transition directly on unified buffer to avoid copies
        bool stillActive = transitionEngine.update();
        
        if (!stillActive) {
            // Transition completed - no delay needed for 120 FPS
            stripsAreSynced = false; // Mark that we need to sync
        }
    } else {
        // Normal operation: just run the effect
        effects[currentEffect].function();
        stripsAreSynced = false; // Effects work on unified buffer
    }
    
    // Only sync when actually needed
    syncLedsToStrips();
    
    // Process scroll encoder input (non-blocking)
    // Re-enabled with error handling
    if (scrollEncoderAvailable) {
        processScrollEncoder();
    }
    
    // Show the LEDs
    FastLED.show();
    
    // Advance the global hue
    EVERY_N_MILLISECONDS(20) {
        gHue++;
    }
    
    // Status every 30 seconds (reduced from 5)
    EVERY_N_SECONDS(30) {
        Serial.print("Effect: ");
        Serial.print(effects[currentEffect].name);
        Serial.print(", Palette: ");
        Serial.print(currentPaletteIndex);
        Serial.print(", Free heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");
        
        // Warn if heap is getting low
        if (ESP.getFreeHeap() < 50000) {
            Serial.println("⚠️  WARNING: Low memory! Consider reducing effect complexity.");
        }
        
        // Quick encoder performance summary
        if (encoderManager.isAvailable()) {
            const EncoderMetrics& metrics = encoderManager.getMetrics();
            Serial.printf("Encoder: %u queue depth, %.1f%% I2C success\n",
                         metrics.current_queue_depth,
                         metrics.i2c_transactions > 0 ? 
                         (100.0f * (metrics.i2c_transactions - metrics.i2c_failures) / metrics.i2c_transactions) : 100.0f);
        }
    }
    
    
    // Frame rate control - maintain 120 FPS target
    static uint32_t frameStartTime = 0;
    static uint32_t targetFrameTime = 8333; // 8.33ms for 120 FPS
    
    uint32_t frameEndTime = micros();
    uint32_t frameTime = frameEndTime - frameStartTime;
    
    // Only delay if we're running faster than 120 FPS
    if (frameTime < targetFrameTime) {
        delayMicroseconds(targetFrameTime - frameTime);
    }
    
    frameStartTime = micros();
    
    // Performance warning if we can't maintain 120 FPS
    static uint32_t slowFrameCount = 0;
    static uint32_t lastPerfWarning = 0;
    if (frameTime > targetFrameTime + 1000) { // Allow 1ms tolerance
        slowFrameCount++;
        if (millis() - lastPerfWarning > 5000 && slowFrameCount > 10) {
            Serial.printf("⚠️ Performance: %d slow frames (>%.1fms) in last 5s\n", 
                         slowFrameCount, targetFrameTime / 1000.0);
            lastPerfWarning = millis();
            slowFrameCount = 0;
        }
    }
}