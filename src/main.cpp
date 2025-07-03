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
#endif

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


// Forward declare wrapper functions
void stripInterferenceWrapper();
void heartbeatEffectWrapper();
void breathingEffectWrapper();
void stripPlasmaWrapper();
void vortexEffectWrapper();


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

// Include effects header
#include "effects.h"

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
    {"Gravity Well", gravityWellEffect, EFFECT_TYPE_STANDARD}
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
    Serial.println("[DEBUG] About to initialize scroll encoder...");
    // DISABLED - CAUSING CRASHES
    // initScrollEncoder();
    Serial.println("[DEBUG] Scroll encoder init SKIPPED - was causing crashes");
    
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
    
#if FEATURE_SERIAL_MENU
    // Initialize serial menu system
    serialMenu.begin();
#endif
    
    // Wait a moment to ensure serial monitor is fully connected
    Serial.println("\n=== Waiting 3 seconds before WiFi initialization ===");
    Serial.println("This ensures serial monitor captures all debug output...");
    delay(3000);
    
    // Initialize web server
#if FEATURE_WEB_SERVER
    Serial.println("\n=== Initializing Web Server ===");
    if (webServer.begin()) {
        Serial.println("✅ Web server started successfully");
        Serial.print("📱 Access the control panel at: http://");
        Serial.print(WiFi.localIP());
        Serial.println(" or http://lightwaveos.local");
    } else {
        Serial.println("⚠️  Web server failed to start");
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
    Serial.println("[DEBUG] Entering main loop...");
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
        Serial.printf("[DEBUG] LEDs lit: %s, Brightness: %d\n", 
                      anyLit ? "YES" : "NO", FastLED.getBrightness());
        
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
    
    // Update transition system
    if (transitionEngine.isActive()) {
        // During transition: generate new effect frame and let transition engine handle blending
        effects[currentEffect].function();
        syncStripsToLeds();  // Sync current effect to leds buffer
        transitionEngine.update();  // This blends and writes to leds buffer
        syncLedsToStrips();  // Sync blended result back to strips
    } else {
        // Normal operation: just run the effect
        effects[currentEffect].function();
        // Check which buffer the effect uses and sync appropriately
        // Most strip effects write directly to strip1/strip2, so sync back to leds
        syncStripsToLeds();  // Sync strips back to unified LED buffer
    }
    
    // Process scroll encoder input (non-blocking)
    // DISABLED - CAUSING CRASHES
    // processScrollEncoder();
    
    // Debug output every 10 seconds to minimize overhead
    EVERY_N_SECONDS(10) {
        // Count lit LEDs in each strip
        int strip1LitCount = 0;
        int strip2LitCount = 0;
        
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            if (strip1[i].r > 0 || strip1[i].g > 0 || strip1[i].b > 0) {
                strip1LitCount++;
            }
            if (strip2[i].r > 0 || strip2[i].g > 0 || strip2[i].b > 0) {
                strip2LitCount++;
            }
        }
        
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
    
    // Show the LEDs
    FastLED.show();
    
    // Advance the global hue
    EVERY_N_MILLISECONDS(20) {
        gHue++;
    }
    
    // Status every 30 seconds
    EVERY_N_SECONDS(30) {
        Serial.print("Effect: ");
        Serial.print(effects[currentEffect].name);
        Serial.print(", Palette: ");
        Serial.print(currentPaletteIndex);
        Serial.print(", Free heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");
        
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