#include <Arduino.h>
#include <FastLED.h>
#include "config/features.h"
#include "config/hardware_config.h"

#if FEATURE_SERIAL_MENU
#include "utils/SerialMenu.h"
#endif

// LED Type Configuration
#define LED_TYPE WS2812
#define COLOR_ORDER GRB

// Mode-specific LED buffer allocation
#if LED_STRIPS_MODE
    // ==================== LED STRIPS MODE ====================
    // Dual strip buffers for 160 LEDs each
    CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
    CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
    CRGB strip1_transition[HardwareConfig::STRIP1_LED_COUNT];
    CRGB strip2_transition[HardwareConfig::STRIP2_LED_COUNT];
    
    // Combined virtual buffer for compatibility
    CRGB* leds = strip1;  // Primary strip for legacy effects
    CRGB* transitionBuffer = strip1_transition;
    
    // Strip-specific globals
    HardwareConfig::SyncMode currentSyncMode = HardwareConfig::SYNC_SYNCHRONIZED;
    HardwareConfig::PropagationMode currentPropagationMode = HardwareConfig::PROPAGATE_OUTWARD;
    
#else
    // ==================== LED MATRIX MODE ====================
    // Original 9x9 matrix configuration
    CRGB leds[HardwareConfig::NUM_LEDS];
    CRGB transitionBuffer[HardwareConfig::NUM_LEDS];
    
#endif

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

// Transition parameters
bool inTransition = false;
uint32_t transitionStart = 0;
uint32_t transitionDuration = 1000;  // 1 second transitions
float transitionProgress = 0.0f;

// Palette management
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;
uint8_t currentPaletteIndex = 0;

// Include palette definitions
#include "Palettes.h"

// Include encoder support for strips mode
#if LED_STRIPS_MODE
#include "hardware/encoders.h"
#endif

#if FEATURE_SERIAL_MENU
// Serial menu instance
SerialMenu serialMenu;
#endif

// Effect function pointer type
typedef void (*EffectFunction)();

// Initialize strip mapping for spatial effects
void initializeStripMapping() {
#if LED_STRIPS_MODE
    // Linear strip mapping for dual strips
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float normalized = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Linear angle mapping for strips
        angles[i] = normalized * 255;
        
        // Distance from center point for outward propagation
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_HALF_LENGTH;
        radii[i] = distFromCenter * 255;
    }
#else
    // Circular mapping for matrix effects
    for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        float normalized = (float)i / HardwareConfig::NUM_LEDS;
        angles[i] = normalized * 255;
        
        // Create interesting radius mapping for matrix
        float wave = sin(normalized * PI * 2) * 0.5f + 0.5f;
        radii[i] = wave * 255;
    }
#endif
}

// ============== BASIC EFFECTS ==============

void solidColor() {
    fill_solid(leds, HardwareConfig::NUM_LEDS, CRGB::Blue);
}

void pulseEffect() {
    uint8_t brightness = beatsin8(30, 50, 255);
    fill_solid(leds, HardwareConfig::NUM_LEDS, CHSV(160, 255, brightness));
}

void confetti() {
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 10);
    int pos = random16(HardwareConfig::NUM_LEDS);
    leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void sinelon() {
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    int pos = beatsin16(13, 0, HardwareConfig::NUM_LEDS-1);
    leds[pos] += CHSV(gHue, 255, 192);
}

void juggle() {
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    uint8_t dothue = 0;
    for(int i = 0; i < 8; i++) {
        leds[beatsin16(i+7, 0, HardwareConfig::NUM_LEDS-1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

void bpm() {
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for(int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(currentPalette, gHue+(i*2), beat-gHue+(i*10));
    }
}

// ============== ADVANCED WAVE EFFECTS ==============

void waveEffect() {
    static uint16_t wavePosition = 0;
    
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
    
    uint16_t waveSpeed = map(paletteSpeed, 1, 50, 100, 10);
    wavePosition += waveSpeed;
    
    for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        uint8_t brightness = sin8((i * 10) + (wavePosition >> 4));
        uint8_t colorIndex = angles[i] + (wavePosition >> 6);
        
        CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
        leds[i] = color;
    }
}

void rippleEffect() {
    static struct {
        float center;
        float radius;
        float speed;
        uint8_t hue;
        bool active;
    } ripples[5];
    
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
    
    // Spawn new ripples
    if (random8() < 20) {
        for (uint8_t i = 0; i < 5; i++) {
            if (!ripples[i].active) {
                ripples[i].center = random16(HardwareConfig::NUM_LEDS);
                ripples[i].radius = 0;
                ripples[i].speed = 0.5f + (random8() / 255.0f) * 2.0f;
                ripples[i].hue = random8();
                ripples[i].active = true;
                break;
            }
        }
    }
    
    // Update and render ripples
    for (uint8_t r = 0; r < 5; r++) {
        if (!ripples[r].active) continue;
        
        ripples[r].radius += ripples[r].speed * (paletteSpeed / 10.0f);
        
        if (ripples[r].radius > HardwareConfig::NUM_LEDS) {
            ripples[r].active = false;
            continue;
        }
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            float distance = abs((float)i - ripples[r].center);
            float wavePos = distance - ripples[r].radius;
            
            if (abs(wavePos) < 5.0f) {
                uint8_t brightness = 255 - (abs(wavePos) * 51);
                brightness = (brightness * (HardwareConfig::NUM_LEDS - ripples[r].radius)) / HardwareConfig::NUM_LEDS;
                
                CRGB color = ColorFromPalette(currentPalette, ripples[r].hue + distance, brightness);
                leds[i] += color;
            }
        }
    }
}

void interferenceEffect() {
    static float wave1Phase = 0;
    static float wave2Phase = 0;
    
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
    
    wave1Phase += paletteSpeed / 20.0f;
    wave2Phase -= paletteSpeed / 30.0f;
    
    for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        float pos = (float)i / HardwareConfig::NUM_LEDS;
        
        float wave1 = sin(pos * PI * 4 + wave1Phase) * 127 + 128;
        float wave2 = sin(pos * PI * 6 + wave2Phase) * 127 + 128;
        
        float interference = (wave1 + wave2) / 2.0f;
        uint8_t brightness = interference;
        
        uint8_t hue = (uint8_t)(wave1Phase * 20) + angles[i];
        
        CRGB color = ColorFromPalette(currentPalette, hue, brightness);
        leds[i] = color;
    }
}

// ============== MATHEMATICAL PATTERNS ==============

void fibonacciSpiral() {
    static float spiralPhase = 0;
    
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
    spiralPhase += paletteSpeed / 50.0f;
    
    // Fibonacci sequence positions
    int fib[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55};
    
    for (int f = 0; f < 10; f++) {
        int pos = (fib[f] + (int)spiralPhase) % HardwareConfig::NUM_LEDS;
        uint8_t hue = f * 25 + gHue;
        uint8_t brightness = 255 - (f * 20);
        
        // Draw with falloff
        for (int i = -3; i <= 3; i++) {
            int ledPos = (pos + i + HardwareConfig::NUM_LEDS) % HardwareConfig::NUM_LEDS;
            uint8_t fadeBrightness = brightness - (abs(i) * 60);
            leds[ledPos] += CHSV(hue, 255, fadeBrightness);
        }
    }
}

void kaleidoscope() {
    static uint16_t offset = 0;
    offset += paletteSpeed;
    
    // Create symmetrical patterns
    for (int i = 0; i < HardwareConfig::NUM_LEDS / 2; i++) {
        uint8_t hue = sin8(i * 10 + offset) + gHue;
        uint8_t brightness = sin8(i * 15 + offset * 2);
        
        CRGB color = CHSV(hue, 255, brightness);
        
        leds[i] = color;
        leds[HardwareConfig::NUM_LEDS - 1 - i] = color;  // Mirror
    }
}

void plasma() {
    static uint16_t time = 0;
    time += paletteSpeed;
    
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        float v1 = sin((float)i / 8.0f + time / 100.0f);
        float v2 = sin((float)i / 5.0f - time / 150.0f);
        float v3 = sin((float)i / 3.0f + time / 200.0f);
        
        uint8_t hue = (uint8_t)((v1 + v2 + v3) * 42.5f + 127.5f) + gHue;
        uint8_t brightness = (uint8_t)((v1 + v2) * 63.75f + 191.25f);
        
        leds[i] = CHSV(hue, 255, brightness);
    }
}

// ============== NATURE-INSPIRED EFFECTS ==============

void fire() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN FIRE - Starts from center LEDs 79/80 and spreads outward
    static byte heat[HardwareConfig::STRIP_LENGTH];
    
    // Cool down every cell a little
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / HardwareConfig::STRIP_LENGTH) + 2));
    }
    
    // Heat diffuses from center outward
    for(int k = 1; k < HardwareConfig::STRIP_LENGTH - 1; k++) {
        heat[k] = (heat[k - 1] + heat[k] + heat[k + 1]) / 3;
    }
    
    // Ignite new sparks at CENTER (79/80)
    if(random8() < 120) {
        int center = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80
        heat[center] = qadd8(heat[center], random8(160, 255));
    }
    
    // Map heat to both strips with CENTER ORIGIN
    for(int j = 0; j < HardwareConfig::STRIP_LENGTH; j++) {
        CRGB color = HeatColor(heat[j]);
        strip1[j] = color;
        strip2[j] = color;
    }
    #else
    // Original matrix mode
    static byte heat[HardwareConfig::NUM_LEDS];
    for(int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / HardwareConfig::NUM_LEDS) + 2));
    }
    for(int k = HardwareConfig::NUM_LEDS - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    if(random8() < 120) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }
    for(int j = 0; j < HardwareConfig::NUM_LEDS; j++) {
        leds[j] = HeatColor(heat[j]);
    }
    #endif
}

void ocean() {
    static uint16_t waterOffset = 0;
    waterOffset += paletteSpeed / 2;
    
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        // Create wave-like motion
        uint8_t wave1 = sin8((i * 10) + waterOffset);
        uint8_t wave2 = sin8((i * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Ocean colors from deep blue to cyan
        uint8_t hue = 160 + (combinedWave >> 3);  // Blue range
        uint8_t brightness = 100 + (combinedWave >> 1);
        uint8_t saturation = 255 - (combinedWave >> 2);
        
        leds[i] = CHSV(hue, saturation, brightness);
    }
}

// ============== TRANSITION SYSTEM ==============

void startTransition(uint8_t newEffect) {
    if (newEffect == currentEffect) return;
    
    // Save current LED state
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        transitionBuffer[i] = leds[i];
    }
    
    previousEffect = currentEffect;
    currentEffect = newEffect;
    inTransition = true;
    transitionStart = millis();
    transitionProgress = 0.0f;
}

void updateTransition() {
    if (!inTransition) return;
    
    uint32_t elapsed = millis() - transitionStart;
    transitionProgress = (float)elapsed / transitionDuration;
    
    if (transitionProgress >= 1.0f) {
        transitionProgress = 1.0f;
        inTransition = false;
    }
    
    // Smooth easing function (ease-in-out)
    float t = transitionProgress;
    float eased = t < 0.5f 
        ? 2 * t * t 
        : 1 - pow(-2 * t + 2, 2) / 2;
    
    // Blend between old and new effect
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        CRGB oldColor = transitionBuffer[i];
        CRGB newColor = leds[i];
        
        leds[i] = blend(oldColor, newColor, eased * 255);
    }
}

// Array of effects
struct Effect {
    const char* name;
    EffectFunction function;
};

Effect effects[] = {
    // Signature effects
    {"Fire", fire},
    {"Ocean", ocean},
    {"Plasma", plasma},
    
    // Wave dynamics  
    {"Wave", waveEffect},
    {"Ripple", rippleEffect},
    {"Interference", interferenceEffect},
    
    // Mathematical patterns
    {"Fibonacci", fibonacciSpiral},
    {"Kaleidoscope", kaleidoscope},
    
    // Motion effects
    {"Confetti", confetti},
    {"Sinelon", sinelon},
    {"Juggle", juggle},
    {"BPM", bpm},
    
    // Palette showcase
    {"Solid Blue", solidColor},
    {"Pulse Effect", pulseEffect}
};

const uint8_t NUM_EFFECTS = sizeof(effects) / sizeof(effects[0]);

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);
    
#if LED_STRIPS_MODE
    Serial.println("\n=== Light Crystals STRIPS Mode ===");
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
#else
    Serial.println("\n=== Light Crystals MATRIX Mode ===");
    Serial.println("Mode: 9x9 LED Matrix");
    Serial.print("LED Pin: GPIO");
    Serial.println(HardwareConfig::LED_DATA_PIN);
    Serial.print("LEDs: ");
    Serial.println(HardwareConfig::NUM_LEDS);
#endif
    
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
    
    // Initialize LEDs based on mode
#if LED_STRIPS_MODE
    // Dual strip initialization
    FastLED.addLeds<LED_TYPE, HardwareConfig::STRIP1_DATA_PIN, COLOR_ORDER>(
        strip1, HardwareConfig::STRIP1_LED_COUNT);
    FastLED.addLeds<LED_TYPE, HardwareConfig::STRIP2_DATA_PIN, COLOR_ORDER>(
        strip2, HardwareConfig::STRIP2_LED_COUNT);
    FastLED.setBrightness(HardwareConfig::STRIP_BRIGHTNESS);
    
    Serial.println("Dual strip FastLED initialized");
    
    // Initialize M5Stack 8Encoder (disabled for stability)
    initEncoders();
    Serial.println("System running in STABLE MODE - using button control");
#else
    // Single matrix initialization  
    FastLED.addLeds<LED_TYPE, HardwareConfig::LED_DATA_PIN, COLOR_ORDER>(
        leds, HardwareConfig::NUM_LEDS);
    FastLED.setBrightness(HardwareConfig::DEFAULT_BRIGHTNESS);
    
    Serial.println("Matrix FastLED initialized");
#endif
    
    FastLED.setCorrection(TypicalLEDStrip);
    
    // Initialize strip mapping
    initializeStripMapping();
    
    // Initialize palette
    currentPaletteIndex = 0;
    currentPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
    targetPalette = currentPalette;
    
    // Clear LEDs
    FastLED.clear(true);
    
#if FEATURE_SERIAL_MENU
    // Initialize serial menu system
    serialMenu.begin();
#endif
    
    Serial.println("=== Setup Complete ===\n");
}

void handleButton() {
    if (digitalRead(HardwareConfig::BUTTON_PIN) == LOW) {
        if (millis() - lastButtonPress > HardwareConfig::BUTTON_DEBOUNCE_MS) {
            lastButtonPress = millis();
            
            // Start transition to next effect
            uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
            startTransition(nextEffect);
            
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

void loop() {
#if FEATURE_BUTTON_CONTROL
    // Use button control for boards that have buttons
    handleButton();
#endif
    
#if FEATURE_SERIAL_MENU
    // Process serial commands for full system control
    serialMenu.update();
#endif
    
    // Update palette blending
    updatePalette();
    
    // If not in transition, run current effect
    if (!inTransition) {
        effects[currentEffect].function();
    } else {
        // Run the new effect to generate target colors
        effects[currentEffect].function();
        // Then apply transition blending
        updateTransition();
    }
    
    // Show the LEDs
    FastLED.show();
    
    // Advance the global hue
    EVERY_N_MILLISECONDS(20) {
        gHue++;
    }
    
    // Status every 5 seconds
    EVERY_N_SECONDS(5) {
        Serial.print("Effect: ");
        Serial.print(effects[currentEffect].name);
        Serial.print(", Palette: ");
        Serial.print(currentPaletteIndex);
        Serial.print(", Free heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");
    }
    
    // Frame rate control
    delay(1000/HardwareConfig::DEFAULT_FPS);
}