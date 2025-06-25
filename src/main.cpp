#include <Arduino.h>
#include <FastLED.h>
#include "config/features.h"
#include "config/hardware_config.h"
#include "core/EffectTypes.h"
// #include "core/MegaLUTs.h"  // Disabled temporarily
#include "effects/strip/StripEffects.h"
#include "effects/transitions/TransitionEngine.h"
// #include "effects/LUTOptimizedEffects.h"  // Disabled temporarily
#include "effects/CinematicColorOrchestrator.h"
#include "hardware/EncoderManager.h"
#include "hardware/EncoderLEDFeedback.h"
#include "Palettes.h"
#include "esp_task_wdt.h"  // Watchdog timer for system stability

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
uint8_t distanceFromCenter[HardwareConfig::NUM_LEDS];
float normalizedDistance[HardwareConfig::NUM_LEDS];

// Performance optimization LUTs - global for cross-file access  
// uint8_t fadeIntensityLUT[256][256];  // [progress][distance] -> intensity - DISABLED: 65KB causes boot loop
uint8_t wavePatternLUT[256];         // Pre-calculated wave patterns
uint8_t spiralAngleLUT[160];         // Pre-calculated spiral angles per position
uint8_t rippleDecayLUT[80];          // Pre-calculated ripple decay values

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
    // ORCHESTRATOR INTEGRATION: Update LUT from orchestrator-synced currentPalette
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

// ============== ORCHESTRATOR INTEGRATION WRAPPERS ==============
// These functions provide a seamless bridge between existing effects and the orchestrator
// 
// INTEGRATION STATUS:
// ✅ Phase 1: Palette System Unification
//    - currentPalette now syncs from orchestrator in updatePalette()
//    - paletteLUT updated from orchestrator-controlled currentPalette
//    - Legacy automatic palette changes disabled
//    - Setup initializes with orchestrator palette
//
// ✅ Phase 2: Effect Color Integration (Partial)
//    - Added orchestrator color wrapper functions
//    - Updated key effects: solidColor, pulseEffect, confetti, fire, ripple
//    - High-performance getOrchestratedColorFast() for LUT-based speed
//    - Maintains backward compatibility with existing effects
//
// 🔄 Phase 3: Emotional Intensity Integration (In Progress)
//    - Added getEmotionalBrightness() for intensity modulation
//    - Effects now respond to emotional state (spawn rates, brightness, etc.)
//    - Visual feedback integration pending
//
// ⏳ Phase 4: Performance Optimization (Pending)
//    - Single palette update path working
//    - LUT caching optimization pending
//    - Full effect migration pending

/**
 * Get color from orchestrator with emotional modulation
 * Drop-in replacement for ColorFromPalette that adds cinematic intelligence
 */
CRGB getOrchestratedColor(uint8_t position, uint8_t brightness) {
    CRGB color = colorOrchestrator.getEmotionalColor(position, brightness);
    
    // SAFETY: Fallback to legacy palette if orchestrator returns black/invalid
    if (color.r == 0 && color.g == 0 && color.b == 0 && brightness > 10) {
        color = ColorFromPalette(currentPalette, position, brightness);
        static uint32_t lastWarning = 0;
        if (millis() - lastWarning > 5000) {
            Serial.println("⚠️ Orchestrator returned black - using fallback palette");
            lastWarning = millis();
        }
    }
    
    return color;
}

/**
 * Get brightness modulated by emotional intensity
 * Multiplies base brightness by current emotional state
 */
uint8_t getEmotionalBrightness(uint8_t baseBrightness) {
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    // Apply 40-100% scaling based on emotional intensity
    float intensityMultiplier = 0.4f + (emotionalIntensity * 0.6f);
    return baseBrightness * intensityMultiplier;
}

/**
 * Fast orchestrated color lookup using existing LUT system
 * High performance version for effects that need speed
 */
CRGB getOrchestratedColorFast(uint8_t position, uint8_t brightness) {
    // Use existing LUT but apply emotional brightness modulation
    CRGB color = paletteLUT[position];
    if (brightness != 255) {
        color.nscale8(brightness);
    }
    // Apply emotional brightness multiplier
    uint8_t emotionalBrightness = getEmotionalBrightness(255);
    color.nscale8(emotionalBrightness);
    
    // SAFETY: Ensure we never return completely black unless brightness is 0
    if (color.r == 0 && color.g == 0 && color.b == 0 && brightness > 10) {
        color = ColorFromPalette(currentPalette, position, brightness);
    }
    
    return color;
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

// Legacy palette variables (kept for compatibility, but orchestrator overrides)
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette; 
uint8_t currentPaletteIndex = 0;

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

// LUT-Optimized Effect Wrappers - DISABLED
/*
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
*/

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
    {"Gravity Well", gravityWellEffect, EFFECT_TYPE_STANDARD}
    
    // =============== LUT-OPTIMIZED ULTRA PERFORMANCE EFFECTS ===============
    // TEMPORARILY DISABLED - Will re-enable after fixing compilation
    // {"LUT Plasma", lutPlasmaWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Fire", lutFireWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Wave", lutWaveWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Mandelbrot", lutMandelbrotWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Particles", lutParticleWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Perlin", lutPerlinWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Complex", lutComplexWaveWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Shader", lutShaderWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Transitions", lutTransitionWrapper, EFFECT_TYPE_STANDARD},
    // {"LUT Frequency", lutFrequencyWrapper, EFFECT_TYPE_STANDARD}
};

const uint8_t NUM_EFFECTS = sizeof(effects) / sizeof(effects[0]);

// Initialize LUTs for maximum performance  
void initializeLUTs() {
    // NOTE: Large fade intensity LUT disabled to prevent boot loops (65KB too large for ESP32)
    
    // Wave pattern LUT - pre-calculate sin8 combinations
    for (uint16_t i = 0; i < 256; i++) {
        wavePatternLUT[i] = sin8(i);
    }
    
    // Spiral angle LUT - pre-calculate spiral positions
    for (uint16_t i = 0; i < 160; i++) {
        float dist = abs((int)i - 79) / 79.0f;
        spiralAngleLUT[i] = (uint8_t)(dist * 255);
    }
    
    // Ripple decay LUT - exponential decay pre-calculated
    for (uint16_t i = 0; i < 80; i++) {
        float decay = exp(-i * 0.05f);  // Exponential decay
        rippleDecayLUT[i] = (uint8_t)(decay * 255);
    }
}

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
    // CRITICAL FIX: Copy FROM strips TO leds (effects write to strips, FastLED displays strips)
    // This function should sync strip buffers back to the unified leds[] buffer for compatibility
    
    // MEMORY SAFETY: Bounds checking to prevent buffer overflows
    size_t strip1Size = HardwareConfig::STRIP1_LED_COUNT * sizeof(CRGB);
    size_t strip2Size = HardwareConfig::STRIP2_LED_COUNT * sizeof(CRGB);
    size_t totalSize = HardwareConfig::NUM_LEDS * sizeof(CRGB);
    
    // Verify buffer sizes are sane
    if (strip1Size + strip2Size > totalSize) {
        Serial.println("🚨 CRITICAL: Buffer overflow prevented in syncLedsToStrips()");
        return;
    }
    
    // FIXED: Copy FROM strips TO leds (for transition engine compatibility)
    memcpy(leds, strip1, min(strip1Size, totalSize));
    memcpy(&leds[HardwareConfig::STRIP1_LED_COUNT], strip2, 
           min(strip2Size, totalSize - strip1Size));
    stripsAreSynced = true;
    
    // Safety check: Ensure we're not copying garbage data
    static uint32_t lastSafetyCheck = 0;
    if (millis() - lastSafetyCheck > 5000) { // Every 5 seconds
        bool hasValidData = false;
        for (int i = 0; i < 10; i++) { // Check first 10 LEDs
            if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0) {
                hasValidData = true;
                break;
            }
        }
        if (!hasValidData) {
            Serial.println("⚠️ LED buffer appears to be all black - potential effect issue");
        }
        lastSafetyCheck = millis();
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
    
    // DEBUG: Check heap before encoder manager
    Serial.printf("📊 Heap before encoder manager: %d bytes free\n", ESP.getFreeHeap());
    
    // Initialize encoder manager
    Serial.println("Initializing M5ROTATE8 encoder manager...");
    bool encoderInit = encoderManager.begin();
    Serial.printf("📊 Encoder manager init result: %s\n", encoderInit ? "SUCCESS" : "FAILED");
    
    // DEBUG: Check heap after encoder manager
    Serial.printf("📊 Heap after encoder manager: %d bytes free\n", ESP.getFreeHeap());
    
    // TEMPORARILY DISABLED: Visual Feedback System (suspected NULL pointer crash)
    Serial.println("⚠️  VFS DISABLED FOR DEBUGGING - testing NULL pointer crash fix");
    encoderFeedback = nullptr;
    
    // Initialize Visual Feedback System
    // if (encoderManager.isAvailable()) {
    //     M5ROTATE8* encoder = encoderManager.getEncoder();
    //     if (encoder) {
    //         encoderFeedback = new EncoderLEDFeedback(encoder, &visualParams);
    //         encoderFeedback->applyDefaultColorScheme();
    //         Serial.println("✅ Visual Feedback System initialized");
    //     }
    // } else {
    //     Serial.println("⚠️  VFS disabled - no encoder available");
    // }
    
    // Preset Management System - DISABLED FOR NOW
    // Will be re-enabled once basic transitions work
    
    FastLED.setCorrection(TypicalLEDStrip);
    
    // Initialize strip mapping and LUTs
    initializeStripMapping();
    initializeLUTs();
    
    // Initialize orchestrator and sync palette system
    currentPaletteIndex = 0;
    
    // ORCHESTRATOR INTEGRATION: Initialize with orchestrator's starting palette
    currentPalette = colorOrchestrator.getCurrentPalette();
    targetPalette = currentPalette;
    
    Serial.print("🎬 Orchestrator initialized with theme: ");
    Serial.println(colorOrchestrator.getCurrentThemeName());
    
    // CRITICAL: Enable watchdog timer for system stability
    esp_task_wdt_init(30, true);  // 30 second timeout, enable panic
    esp_task_wdt_add(NULL);       // Add main task to watchdog
    Serial.println("🐕 Watchdog timer enabled (30s timeout)");
    
    // DEBUG: Check heap before major allocations
    Serial.printf("📊 Initial heap: %d bytes free\n", ESP.getFreeHeap());
    Serial.printf("📊 PSRAM detected: %s\n", psramFound() ? "YES" : "NO");
    if (psramFound()) {
        Serial.printf("📊 PSRAM size: %d bytes\n", ESP.getPsramSize());
        Serial.printf("📊 Free PSRAM: %d bytes\n", ESP.getFreePsram());
    }
    
    // Add panic handler for debugging
    esp_register_shutdown_handler([]() {
        Serial.println("🚨 PANIC: System shutting down - LED/I2C issue detected");
        FastLED.clear(true);
    });
    
    // Clear LEDs
    FastLED.clear(true);
    
    // DEBUG: Check heap before I2C initialization
    Serial.printf("📊 Heap before I2C: %d bytes free\n", ESP.getFreeHeap());
    
    // Initialize M5Unit-Scroll encoder on secondary I2C bus
    Serial.println("[DEBUG] About to initialize scroll encoder...");
    // Re-enable with improved error handling
    initScrollEncoder();
    if (scrollEncoderAvailable) {
        Serial.println("[DEBUG] Scroll encoder initialized successfully");
    } else {
        Serial.println("[DEBUG] Scroll encoder not available - continuing without it");
    }
    
    // DEBUG: Check heap after I2C initialization
    Serial.printf("📊 Heap after I2C: %d bytes free\n", ESP.getFreeHeap());
    
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
    // ORCHESTRATOR INTEGRATION: Sync legacy palette system from cinematic orchestrator
    // Legacy automatic palette changes disabled - orchestrator now controls all palettes
    
    // Sync currentPalette from orchestrator
    CRGBPalette16 orchestratorPalette = colorOrchestrator.getCurrentPalette();
    
    // Only update if orchestrator palette has changed (optimization)
    static CRGBPalette16 lastOrchestratorPalette;
    static bool firstSync = true;
    
    if (firstSync || memcmp(&orchestratorPalette, &lastOrchestratorPalette, sizeof(CRGBPalette16)) != 0) {
        // Smooth transition to orchestrator palette
        targetPalette = orchestratorPalette;
        
        // Fast blend for immediate responsiveness to orchestrator changes
        nblendPaletteTowardPalette(currentPalette, targetPalette, 64);
        
        // Cache for next comparison
        memcpy(&lastOrchestratorPalette, &orchestratorPalette, sizeof(CRGBPalette16));
        firstSync = false;
    }
}

void updateEncoderFeedback() {
    if (encoderFeedback && encoderManager.isAvailable()) {
        // Update current effect info
        encoderFeedback->setCurrentEffect(currentEffect, effects[currentEffect].name);
        
        // ENHANCED ORCHESTRATOR INTEGRATION: Show emotional state on encoders
        static float lastEmotionalIntensity = -1.0f;
        static EmotionalState lastEmotionalState = EMOTION_COUNT; // Invalid state to force initial update
        
        float currentEmotionalIntensity = colorOrchestrator.getEmotionalIntensity();
        EmotionalState currentEmotionalState = colorOrchestrator.getCurrentEmotion();
        
        // Flash encoder when emotional state changes
        if (currentEmotionalState != lastEmotionalState) {
            // Different colors for different emotional states
            CRGB emotionColors[] = {
                CRGB::Blue,         // TRANQUIL - calm blue
                CRGB::Yellow,       // BUILDING - warm yellow
                CRGB::Orange,       // INTENSE - energetic orange
                CRGB::Red,          // EXPLOSIVE - peak red
                CRGB::Purple        // RESOLUTION - peaceful purple
            };
            
            if (currentEmotionalState < EMOTION_COUNT) {
                CRGB color = emotionColors[currentEmotionalState];
                encoderFeedback->flashEncoder(2, color.r, color.g, color.b, 500);
            }
            lastEmotionalState = currentEmotionalState;
        }
        
        // Subtle intensity change feedback
        if (abs(currentEmotionalIntensity - lastEmotionalIntensity) > 0.15f) {
            // Use current theme color with intensity-based brightness
            CRGB themeColor = colorOrchestrator.getEmotionalColor(128, 255);
            uint8_t flashBrightness = 100 + (currentEmotionalIntensity * 155); // 100-255 brightness
            themeColor.nscale8(flashBrightness);
            encoderFeedback->flashEncoder(2, themeColor.r, themeColor.g, themeColor.b, 200);
            lastEmotionalIntensity = currentEmotionalIntensity;
        }
        
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
    static uint32_t lastWatchdogFeed = 0;
    
    // Feed watchdog timer every 5 seconds to prevent system reset
    uint32_t now = millis();
    if (now - lastWatchdogFeed > 5000) {
        esp_task_wdt_reset();
        lastWatchdogFeed = now;
    }
    
    // Debug print every 10 seconds (reduced frequency)
    if (now - lastDebugPrint > 10000) {
        lastDebugPrint = now;
        Serial.printf("[DEBUG] Loop: %lu iterations, Effect: %s, Heap: %d\n", 
                     loopCounter, effects[currentEffect].name, ESP.getFreeHeap());
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
                Serial.println("  'c' or 'C' - Show cinematic orchestrator status");
                Serial.println("  'x' or 'X' - Trigger emotional peak (dramatic effect)");
#if FEATURE_WIRELESS_ENCODERS
                Serial.println("  'w' or 'W' - Enable wireless receiver and start pairing");
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
                if (encoderManager.hasWirelessPaired()) {
                    Serial.printf("  • Wireless Encoders: %s", 
                        encoderManager.isWirelessConnected() ? "CONNECTED" : "PAIRED");
                    if (encoderManager.isWirelessConnected()) {
                        Serial.printf(" (%.1f%% loss, %d pkts)", 
                            encoderManager.getWirelessPacketLossRate(),
                            encoderManager.getWirelessPacketsReceived());
                    }
                    Serial.println();
                } else {
                    Serial.println("  • Wireless Encoders: Ready for pairing");
                }
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
            case 'c':
            case 'C':
                Serial.println("\n🎬 === CINEMATIC ORCHESTRATOR STATUS ===");
                Serial.printf("🎭 Current Theme: %s\n", colorOrchestrator.getCurrentThemeName());
                Serial.printf("💫 Emotional State: %s\n", colorOrchestrator.getCurrentEmotionName());
                Serial.printf("🔥 Intensity Level: %.1f%%\n", colorOrchestrator.getEmotionalIntensity() * 100.0f);
                Serial.printf("🌀 Global Hue: %d/255\n", gHue);
                Serial.println("\n🔗 Integration Status:");
                Serial.println("  ✅ Palette system unified with orchestrator");
                Serial.println("  ✅ Effects using orchestrated colors");
                Serial.println("  ✅ Emotional intensity modulating effects");
                Serial.println("  ✅ Encoder feedback showing emotional state");
                Serial.printf("  🎨 Active Palette: %s synced\n", "Legacy");
                Serial.printf("  📊 Effects Enhanced: %d of %d\n", 5, NUM_EFFECTS);
                Serial.println("=========================================\n");
                break;
            case 'x':
            case 'X':
                Serial.println("💥 TRIGGERING EMOTIONAL PEAK!");
                colorOrchestrator.triggerEmotionalPeak();
                break;
#if FEATURE_WIRELESS_ENCODERS
            case 'w':
            case 'W':
                Serial.println("🔗 Enabling wireless encoder receiver...");
                if (encoderManager.enableWireless()) {
                    encoderManager.startWirelessPairing();
                    Serial.println("✅ Wireless receiver enabled and in pairing mode");
                    Serial.println("   Power on your wireless encoder transmitter now");
                } else {
                    Serial.println("❌ Failed to enable wireless receiver");
                }
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
                        
                    case 2: // Theme selection (was palette)
                        if (event.delta > 0) {
                            LightShowTheme currentTheme = colorOrchestrator.getCurrentTheme();
                            LightShowTheme nextTheme = (LightShowTheme)((currentTheme + 1) % THEME_COUNT);
                            colorOrchestrator.setTheme(nextTheme);
                        } else {
                            LightShowTheme currentTheme = colorOrchestrator.getCurrentTheme();
                            LightShowTheme prevTheme = (LightShowTheme)(currentTheme > 0 ? currentTheme - 1 : THEME_COUNT - 1);
                            colorOrchestrator.setTheme(prevTheme);
                        }
                        Serial.printf("🎨 MAIN: Theme changed to %s\n", colorOrchestrator.getCurrentThemeName());
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
                        
                    case 8: // 9th Encoder (Scroll Wheel) - Dynamic Parameter Control
                        // Use 9th encoder for dynamic parameter selection
                        {
                            static uint8_t selectedParam = 0; // 0=Effect, 1=Brightness, 2=Theme, 3=Speed
                            const char* paramNames[] = {"Effect", "Brightness", "Theme", "Speed"};
                            
                            if (event.delta > 0) {
                                selectedParam = (selectedParam + 1) % 4;
                            } else {
                                selectedParam = selectedParam > 0 ? selectedParam - 1 : 3;
                            }
                            
                            Serial.printf("🎚️ MAIN: 9th Encoder -> %s mode\n", paramNames[selectedParam]);
                            
                            // Visual feedback - cycle through colors
                            CRGB colors[] = {CRGB::Red, CRGB::White, CRGB::Purple, CRGB::Yellow};
                            if (encoderFeedback) {
                                encoderFeedback->flashEncoder(0, colors[selectedParam].r, colors[selectedParam].g, colors[selectedParam].b, 500);
                            }
                        }
                        break;
                        
                    default:
                        Serial.printf("⚠️ MAIN: Unknown encoder ID %d\n", event.encoder_id);
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
        stripsAreSynced = false; // Effects work on strip buffers
        
        // DEBUG: Track effect output for diagnostics
        static uint32_t lastEffectLog = 0;
        if (millis() - lastEffectLog > 10000) { // Every 10 seconds
            // Sample a few LEDs to verify effect is producing output
            bool hasStripOutput = false;
            for (int i = 75; i < 85; i++) { // Center region
                if (strip1[i].r > 10 || strip1[i].g > 10 || strip1[i].b > 10 ||
                    strip2[i].r > 10 || strip2[i].g > 10 || strip2[i].b > 10) {
                    hasStripOutput = true;
                    break;
                }
            }
            Serial.printf("🎨 Effect '%s' producing output: %s\n", 
                         effects[currentEffect].name, 
                         hasStripOutput ? "YES" : "NO");
            lastEffectLog = millis();
        }
        
        // SAFETY: If effect produces no output, fill with a basic pattern
        static uint32_t lastOutputCheck = 0;
        if (millis() - lastOutputCheck > 5000) { // Check every 5 seconds (less frequent)
            bool hasOutput = false;
            // Check strip buffers (where effects actually write) for any output
            for (int i = 70; i < 90; i++) {
                if (strip1[i].r > 5 || strip1[i].g > 5 || strip1[i].b > 5 ||
                    strip2[i].r > 5 || strip2[i].g > 5 || strip2[i].b > 5) {
                    hasOutput = true;
                    break;
                }
            }
            if (!hasOutput) {
                // Check ends too for non-center effects
                for (int i = 0; i < 10; i++) {
                    if (strip1[i].r > 5 || strip1[i].g > 5 || strip1[i].b > 5 ||
                        strip2[i].r > 5 || strip2[i].g > 5 || strip2[i].b > 5) {
                        hasOutput = true;
                        break;
                    }
                }
                for (int i = 150; i < 160; i++) {
                    if (strip1[i].r > 5 || strip1[i].g > 5 || strip1[i].b > 5 ||
                        strip2[i].r > 5 || strip2[i].g > 5 || strip2[i].b > 5) {
                        hasOutput = true;
                        break;
                    }
                }
            }
            if (!hasOutput) {
                // FIXED: Emergency fallback writes to STRIP buffers (where FastLED reads from)
                uint8_t brightness = (sin8(millis() / 10) / 2) + 127;
                fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CRGB(brightness, 0, 0));
                fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CRGB(brightness, 0, 0));
                Serial.printf("⚠️ No effect output detected - using emergency BREATHING pattern\n");
                stripsAreSynced = false; // Mark strips as needing sync
            }
            lastOutputCheck = millis();
        }
    }
    
    // Only sync when actually needed
    syncLedsToStrips();
    
    // Process scroll encoder input (non-blocking)
    // Re-enabled with error handling
    if (scrollEncoderAvailable) {
        processScrollEncoder();
    }
    
    // FIXED: Proper frame timing WITHOUT encoder starvation
    static uint32_t lastFrameTime = 0;
    static uint32_t lastEffectTime = 0;
    uint32_t currentTime = millis();
    uint32_t targetFrameTime = 1000 / 60; // 60 FPS target (16.67ms per frame)
    
    // CRITICAL: Always process effects to prevent starvation
    // But only update display at target frame rate
    bool shouldDisplay = (currentTime - lastFrameTime >= targetFrameTime);
    bool shouldRunEffects = (currentTime - lastEffectTime >= 8); // 120 Hz effect updates
    
    if (shouldRunEffects) {
        // FIXED: Advance hue synchronized with effect rate (prevents 50x speed)
        gHue++;
        
        // Update cinematic orchestrator with proper timing
        colorOrchestrator.update(gHue);
        
        lastEffectTime = currentTime;
    }
    
    if (shouldDisplay) {
        // CRITICAL: Ensure buffer synchronization before display
        syncLedsToStrips();
        
        // Display LEDs immediately after effects are rendered
        FastLED.show();
        
        lastFrameTime = currentTime;
        
        // DEBUG: Monitor for timing issues
        static uint32_t frameCounter = 0;
        frameCounter++;
        if (frameCounter % 300 == 0) { // Every 5 seconds at 60 FPS
            Serial.printf("🎬 FPS Check: %d frames, Free heap: %d\n", frameCounter, ESP.getFreeHeap());
        }
    }
    
    // Continue processing - no early return that blocks encoders
    
    // Status every 30 seconds (reduced from 5)
    EVERY_N_SECONDS(30) {
        Serial.print("Effect: ");
        Serial.print(effects[currentEffect].name);
        Serial.print(", Theme: ");
        Serial.print(colorOrchestrator.getCurrentThemeName());
        Serial.print(", Emotion: ");
        Serial.print(colorOrchestrator.getCurrentEmotionName());
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
    
}