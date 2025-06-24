#include <Arduino.h>
#include <FastLED.h>
#include "config/features.h"
#include "config/hardware_config.h"

#if FEATURE_PERFORMANCE_MONITOR
#include "utils/PerformanceOptimizer.h"
#endif

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
    
    // Combined virtual buffer for compatibility with EffectBase expectations
    // EffectBase expects leds[NUM_LEDS] where NUM_LEDS = 320 for strips mode
    // We create a unified buffer that can handle both strips
    CRGB leds[HardwareConfig::NUM_LEDS];  // Full 320 LED buffer for EffectBase compatibility
    CRGB transitionBuffer[HardwareConfig::NUM_LEDS];
    
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

// M5ROTATE8 encoder for strips mode - NON-BLOCKING I2C TASK ARCHITECTURE
#if LED_STRIPS_MODE
#include "m5rotate8.h"

// Single encoder instance (default address 0x41)
M5ROTATE8 encoder;
bool encoderAvailable = false;

// FreeRTOS task and queue for non-blocking I2C
TaskHandle_t encoderTaskHandle = NULL;
QueueHandle_t encoderEventQueue = NULL;

// Encoder event structure for queue communication
struct EncoderEvent {
    uint8_t encoder_id;
    int32_t delta;
    bool button_pressed;
    uint32_t timestamp;
};

// Connection health and recovery
uint32_t lastConnectionCheck = 0;
uint32_t encoderFailCount = 0;
uint32_t reconnectBackoffMs = 1000;  // Start with 1 second backoff
bool encoderSuspended = false;

// LED flash timing (handled by I2C task)
uint32_t ledFlashTime[8] = {0};
bool ledNeedsUpdate[8] = {false};

// Serial rate limiting
uint32_t lastSerialOutput = 0;
const uint32_t SERIAL_RATE_LIMIT_MS = 100;  // Max 10 messages per second
#endif

// Light Guide Mode disabled - implementation removed

#if FEATURE_SERIAL_MENU
// Serial menu instance
SerialMenu serialMenu;
// Dummy wave engine for compatibility
DummyWave2D fxWave2D;
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

#if LED_STRIPS_MODE
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

// ============== NON-BLOCKING I2C ENCODER TASK ==============

#if LED_STRIPS_MODE
// Dedicated I2C task running on Core 0 (separate from main loop on Core 1)
void encoderTask(void* parameter) {
    Serial.println("🚀 I2C Encoder Task started on Core 0");
    Serial.println("📡 Task will handle all I2C operations independently");
    Serial.println("⚡ Main loop is now NON-BLOCKING for encoder operations");
    
    while (true) {
        uint32_t now = millis();
        
        // Connection health check with exponential backoff
        if (!encoderAvailable || encoderSuspended) {
            if (now - lastConnectionCheck >= reconnectBackoffMs) {
                lastConnectionCheck = now;
                
                Serial.printf("🔄 TASK: Attempting encoder reconnection (backoff: %dms)...\n", reconnectBackoffMs);
                
                if (encoder.begin() && encoder.isConnected()) {
                    Serial.println("✅ TASK: Encoder reconnected successfully!");
                    Serial.printf("📋 TASK: Firmware V%d\n", encoder.getVersion());
                    encoderAvailable = true;
                    encoderSuspended = false;
                    encoderFailCount = 0;
                    reconnectBackoffMs = 1000;  // Reset backoff
                    
                    // Initialize encoder LEDs to idle state
                    encoder.setAll(0, 0, 16);
                    Serial.println("💡 TASK: Encoder LEDs initialized to idle state");
                } else {
                    encoderFailCount++;
                    // Exponential backoff: 1s, 2s, 4s, 8s, max 30s
                    uint32_t maxShift = min(encoderFailCount, (uint32_t)5);
                    reconnectBackoffMs = min((uint32_t)(1000 * (1 << maxShift)), (uint32_t)30000);
                    Serial.printf("❌ TASK: Reconnection failed (%d attempts), next try in %dms\n", 
                                 encoderFailCount, reconnectBackoffMs);
                }
            }
            
            // Wait longer when disconnected to avoid spam
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        
        // Poll encoders safely in dedicated task
        bool connectionLost = false;
        
        for (int i = 0; i < 4; i++) {
            if (!encoder.isConnected()) {
                connectionLost = true;
                break;
            }
            
            // Read relative counter with error handling
            int32_t delta = encoder.getRelCounter(i);
            
            if (delta != 0) {
                // Normalize step size (M5ROTATE8 sometimes returns ±2, sometimes ±1)
                if (abs(delta) >= 2) {
                    delta = delta / 2;
                }
                
                // Create event for main loop
                EncoderEvent event = {
                    .encoder_id = (uint8_t)i,
                    .delta = delta,
                    .button_pressed = false,  // Button handling can be added later
                    .timestamp = now
                };
                
                // Send to main loop via queue (non-blocking)
                if (xQueueSend(encoderEventQueue, &event, 0) != pdTRUE) {
                    // Queue full - drop event (main loop is too slow)
                    Serial.println("⚠️ TASK: Encoder event queue full - dropping event");
                } else {
                    Serial.printf("📤 TASK: Encoder %d delta %+d queued\n", i, delta);
                }
                
                // Flash LED to indicate activity
                ledFlashTime[i] = now;
                ledNeedsUpdate[i] = true;
            }
            
            // Small delay between encoder reads to prevent I2C bus saturation
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        
        // Handle connection loss
        if (connectionLost) {
            Serial.println("💥 TASK: Encoder connection lost, suspending I2C operations...");
            encoderAvailable = false;
            encoderSuspended = true;
            lastConnectionCheck = now;
            continue;
        }
        
        // Update encoder LEDs every 100ms to show status
        static uint32_t lastLEDUpdate = 0;
        if (now - lastLEDUpdate >= 100) {
            lastLEDUpdate = now;
            
            for (int i = 0; i < 4; i++) {
                if (ledNeedsUpdate[i]) {
                    if (now - ledFlashTime[i] < 300) {
                        // Green flash for activity
                        encoder.writeRGB(i, 0, 64, 0);
                    } else {
                        // Return to colored idle state
                        switch (i) {
                            case 0: encoder.writeRGB(i, 16, 0, 0); break;   // Red - Effect
                            case 1: encoder.writeRGB(i, 16, 16, 16); break; // White - Brightness
                            case 2: encoder.writeRGB(i, 8, 0, 16); break;   // Purple - Palette
                            case 3: encoder.writeRGB(i, 16, 8, 0); break;   // Yellow - Speed
                        }
                        ledNeedsUpdate[i] = false;
                    }
                }
            }
        }
        
        // Task runs at 50Hz (20ms intervals)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Safe serial output with rate limiting (only for encoder changes)
void rateLimitedSerial(const char* message) {
    uint32_t now = millis();
    if (now - lastSerialOutput >= SERIAL_RATE_LIMIT_MS) {
        lastSerialOutput = now;
        Serial.print(message);
    }
}

// ALWAYS print debug messages (no rate limiting)
void debugSerial(const char* message) {
    Serial.print(message);
}
#endif

#endif

// ============== BASIC EFFECTS ==============

void solidColor() {
    #if LED_STRIPS_MODE
    fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CRGB::Blue);
    fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CRGB::Blue);
    #else
    fill_solid(leds, HardwareConfig::NUM_LEDS, CRGB::Blue);
    #endif
}

void pulseEffect() {
    uint8_t brightness = beatsin8(30, 50, 255);
    #if LED_STRIPS_MODE
    fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CHSV(160, 255, brightness));
    fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CHSV(160, 255, brightness));
    #else
    fill_solid(leds, HardwareConfig::NUM_LEDS, CHSV(160, 255, brightness));
    #endif
}

void confetti() {
#if LED_STRIPS_MODE
    // CENTER ORIGIN CONFETTI - ALL effects MUST originate from CENTER LEDs 79/80
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 10);
    
    // Spawn confetti ONLY at center LEDs 79/80 (MANDATORY CENTER ORIGIN)
    if (random8() < 80) {
        int centerPos = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80 only
        leds[centerPos] += CHSV(gHue + random8(64), 200, 255);
    }
    
    // Move confetti outward from center with fading
    for (int i = HardwareConfig::STRIP_CENTER_POINT - 1; i >= 0; i--) {
        if (leds[i+1]) {
            leds[i] = leds[i+1];
            leds[i].fadeToBlackBy(25);
        }
    }
    for (int i = HardwareConfig::STRIP_CENTER_POINT + 2; i < HardwareConfig::NUM_LEDS; i++) {
        if (leds[i-1]) {
            leds[i] = leds[i-1];
            leds[i].fadeToBlackBy(25);
        }
    }
#else
    // Original matrix mode
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 10);
    int pos = random16(HardwareConfig::NUM_LEDS);
    leds[pos] += CHSV(gHue + random8(64), 200, 255);
#endif
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN CONFETTI
void stripConfetti() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN CONFETTI - Sparks spawn at center LEDs 79/80 and fade as they move outward
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 10);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 10);
    
    // Spawn new confetti at CENTER (LEDs 79/80)
    if (random8() < 80) {
        int centerPos = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80
        CRGB color = CHSV(gHue + random8(64), 200, 255);
        strip1[centerPos] += color;
        strip2[centerPos] += color;
    }
    
    // Move existing confetti outward from center with fading
    for (int i = HardwareConfig::STRIP_CENTER_POINT - 1; i >= 0; i--) {
        if (strip1[i+1]) {
            strip1[i] = strip1[i+1];
            strip1[i].fadeToBlackBy(30);
            strip2[i] = strip2[i+1];
            strip2[i].fadeToBlackBy(30);
        }
    }
    for (int i = HardwareConfig::STRIP_CENTER_POINT + 1; i < HardwareConfig::STRIP_LENGTH; i++) {
        if (strip1[i-1]) {
            strip1[i] = strip1[i-1];
            strip1[i].fadeToBlackBy(30);
            strip2[i] = strip2[i-1];
            strip2[i].fadeToBlackBy(30);
        }
    }
    #endif
}

void sinelon() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN SINELON - Oscillates outward from center LEDs 79/80
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Oscillate from center outward (0 to STRIP_HALF_LENGTH)
    int distFromCenter = beatsin16(13, 0, HardwareConfig::STRIP_HALF_LENGTH);
    
    // Set both sides of center
    int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
    int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
    
    if (pos1 < HardwareConfig::STRIP_LENGTH) {
        strip1[pos1] += CHSV(gHue, 255, 192);
        strip2[pos1] += CHSV(gHue, 255, 192);
    }
    if (pos2 >= 0) {
        strip1[pos2] += CHSV(gHue + 128, 255, 192);  // Different hue
        strip2[pos2] += CHSV(gHue + 128, 255, 192);
    }
    #else
    // Original matrix mode
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    int pos = beatsin16(13, 0, HardwareConfig::NUM_LEDS-1);
    leds[pos] += CHSV(gHue, 255, 192);
    #endif
}

void juggle() {
#if LED_STRIPS_MODE
    // CENTER ORIGIN JUGGLE - ALL effects MUST originate from CENTER LEDs 79/80
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    
    uint8_t dothue = 0;
    for(int i = 0; i < 8; i++) {
        // Oscillate from center outward (MANDATORY CENTER ORIGIN)
        int distFromCenter = beatsin16(i+7, 0, HardwareConfig::STRIP_HALF_LENGTH);
        
        // Set positions on both sides of center (79/80)
        int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
        int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
        
        CRGB color = CHSV(dothue, 200, 255);
        
        if (pos1 < HardwareConfig::NUM_LEDS) {
            leds[pos1] |= color;
        }
        if (pos2 >= 0) {
            leds[pos2] |= color;
        }
        
        dothue += 32;
    }
#else
    // Original matrix mode
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    uint8_t dothue = 0;
    for(int i = 0; i < 8; i++) {
        leds[beatsin16(i+7, 0, HardwareConfig::NUM_LEDS-1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
#endif
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN JUGGLE
void stripJuggle() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN JUGGLE - Multiple dots oscillate outward from center LEDs 79/80
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    uint8_t dothue = 0;
    for(int i = 0; i < 8; i++) {
        // Oscillate from center outward (0 to STRIP_HALF_LENGTH)
        int distFromCenter = beatsin16(i+7, 0, HardwareConfig::STRIP_HALF_LENGTH);
        
        // Set positions on both sides of center
        int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
        int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
        
        CRGB color = CHSV(dothue, 200, 255);
        
        if (pos1 < HardwareConfig::STRIP_LENGTH) {
            strip1[pos1] |= color;
            strip2[pos1] |= color;
        }
        if (pos2 >= 0) {
            strip1[pos2] |= color;
            strip2[pos2] |= color;
        }
        
        dothue += 32;
    }
    #endif
}

void bpm() {
#if LED_STRIPS_MODE
    // CENTER ORIGIN BPM - ALL effects MUST originate from CENTER LEDs 79/80
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    
    // Calculate distance from center for each LED (MANDATORY CENTER ORIGIN)
    for(int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Intensity decreases with distance from center
        uint8_t intensity = beat - (distFromCenter * 3);
        intensity = max(intensity, (uint8_t)32); // Minimum brightness
        
        leds[i] = ColorFromPalette(currentPalette, 
                                  gHue + (distFromCenter * 2), 
                                  intensity);
    }
#else
    // Original matrix mode
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for(int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(currentPalette, gHue+(i*2), beat-gHue+(i*10));
    }
#endif
}

// ============== ADVANCED WAVE EFFECTS ==============

void waveEffect() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN WAVES - Start from center LEDs 79/80 and propagate outward
    static uint16_t wavePosition = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeAmount);
    
    uint16_t waveSpeed = map(paletteSpeed, 1, 50, 100, 10);
    wavePosition += waveSpeed;
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Wave propagates outward from center
        uint8_t brightness = sin8((distFromCenter * 15) + (wavePosition >> 4));
        uint8_t colorIndex = (distFromCenter * 8) + (wavePosition >> 6);
        
        CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
    #else
    // Original matrix mode
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
    #endif
}

void rippleEffect() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN RIPPLES - Always start from CENTER LEDs 79/80 and move outward
    static struct {
        float radius;
        float speed;
        uint8_t hue;
        bool active;
    } ripples[5];
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeAmount);
    
    // Spawn new ripples at CENTER ONLY
    if (random8() < 30) {
        for (uint8_t i = 0; i < 5; i++) {
            if (!ripples[i].active) {
                ripples[i].radius = 0;
                ripples[i].speed = 0.5f + (random8() / 255.0f) * 2.0f;
                ripples[i].hue = random8();
                ripples[i].active = true;
                break;
            }
        }
    }
    
    // Update and render CENTER ORIGIN ripples
    for (uint8_t r = 0; r < 5; r++) {
        if (!ripples[r].active) continue;
        
        ripples[r].radius += ripples[r].speed * (paletteSpeed / 10.0f);
        
        if (ripples[r].radius > HardwareConfig::STRIP_HALF_LENGTH) {
            ripples[r].active = false;
            continue;
        }
        
        // Draw ripple moving outward from center
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            float wavePos = distFromCenter - ripples[r].radius;
            
            if (abs(wavePos) < 3.0f) {
                uint8_t brightness = 255 - (abs(wavePos) * 85);
                brightness = (brightness * (HardwareConfig::STRIP_HALF_LENGTH - ripples[r].radius)) / HardwareConfig::STRIP_HALF_LENGTH;
                
                CRGB color = ColorFromPalette(currentPalette, ripples[r].hue + distFromCenter, brightness);
                strip1[i] += color;
                strip2[i] += color;
            }
        }
    }
    #else
    // Original matrix mode with center origin
    static struct {
        float center;
        float radius;
        float speed;
        uint8_t hue;
        bool active;
    } ripples[5];
    
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
    
    if (random8() < 20) {
        for (uint8_t i = 0; i < 5; i++) {
            if (!ripples[i].active) {
                ripples[i].center = HardwareConfig::NUM_LEDS / 2; // Matrix center
                ripples[i].radius = 0;
                ripples[i].speed = 0.5f + (random8() / 255.0f) * 2.0f;
                ripples[i].hue = random8();
                ripples[i].active = true;
                break;
            }
        }
    }
    
    for (uint8_t r = 0; r < 5; r++) {
        if (!ripples[r].active) continue;
        
        ripples[r].radius += ripples[r].speed * (paletteSpeed / 10.0f);
        
        if (ripples[r].radius > HardwareConfig::NUM_LEDS / 2) {
            ripples[r].active = false;
            continue;
        }
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            float distance = abs((float)i - ripples[r].center);
            float wavePos = distance - ripples[r].radius;
            
            if (abs(wavePos) < 5.0f) {
                uint8_t brightness = 255 - (abs(wavePos) * 51);
                CRGB color = ColorFromPalette(currentPalette, ripples[r].hue + distance, brightness);
                leds[i] += color;
            }
        }
    }
    #endif
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

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN INTERFERENCE
void stripInterference() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN INTERFERENCE - Waves emanate from center LEDs 79/80 and interfere
    static float wave1Phase = 0;
    static float wave2Phase = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeAmount);
    
    wave1Phase += paletteSpeed / 20.0f;
    wave2Phase -= paletteSpeed / 30.0f;
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Two waves emanating from center with different frequencies
        float wave1 = sin(normalizedDist * PI * 4 + wave1Phase) * 127 + 128;
        float wave2 = sin(normalizedDist * PI * 6 + wave2Phase) * 127 + 128;
        
        // Interference pattern from CENTER ORIGIN
        float interference = (wave1 + wave2) / 2.0f;
        uint8_t brightness = interference;
        
        // Hue varies with distance from center
        uint8_t hue = (uint8_t)(wave1Phase * 20) + (distFromCenter * 8);
        
        CRGB color = ColorFromPalette(currentPalette, hue, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
    #endif
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN BPM
void stripBPM() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN BPM - Pulses emanate from center LEDs 79/80
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        uint8_t colorIndex = gHue + (distFromCenter * 2);
        uint8_t brightness = beat - gHue + (distFromCenter * 10);
        
        CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
    #endif
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN PLASMA
void stripPlasma() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN PLASMA - Plasma field generated from center LEDs 79/80
    static uint16_t time = 0;
    time += paletteSpeed;
    
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Plasma calculations from center outward
        float v1 = sin(normalizedDist * 8.0f + time / 100.0f);
        float v2 = sin(normalizedDist * 5.0f - time / 150.0f);
        float v3 = sin(normalizedDist * 3.0f + time / 200.0f);
        
        uint8_t hue = (uint8_t)((v1 + v2 + v3) * 42.5f + 127.5f) + gHue;
        uint8_t brightness = (uint8_t)((v1 + v2) * 63.75f + 191.25f);
        
        CRGB color = CHSV(hue, 255, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
    #endif
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

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN OCEAN
void stripOcean() {
    #if LED_STRIPS_MODE
    // CENTER ORIGIN OCEAN - Waves emanate from center LEDs 79/80
    static uint16_t waterOffset = 0;
    waterOffset += paletteSpeed / 2;
    
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Create wave-like motion from center outward
        uint8_t wave1 = sin8((distFromCenter * 10) + waterOffset);
        uint8_t wave2 = sin8((distFromCenter * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Ocean colors from deep blue to cyan
        uint8_t hue = 160 + (combinedWave >> 3);  // Blue range
        uint8_t brightness = 100 + (combinedWave >> 1);
        uint8_t saturation = 255 - (combinedWave >> 2);
        
        CRGB color = CHSV(hue, saturation, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
    #endif
}

// ============== LIGHT GUIDE PLATE EFFECTS ==============
// LGP implementation has been removed - awaiting Dual-Strip Wave Engine

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

#if LED_STRIPS_MODE
Effect effects[] = {
    // =============== LIGHT GUIDE PLATE EFFECTS ===============
    // LGP implementation removed - awaiting Dual-Strip Wave Engine
    
    // =============== BASIC STRIP EFFECTS ===============
    // Signature effects with CENTER ORIGIN
    {"Fire", fire},
    {"Ocean", stripOcean},
    
    // Wave dynamics (already CENTER ORIGIN)
    {"Wave", waveEffect},
    {"Ripple", rippleEffect},
    
    // NEW Strip-specific CENTER ORIGIN effects
    {"Strip Confetti", stripConfetti},
    {"Strip Juggle", stripJuggle},
    {"Strip Interference", stripInterference},
    {"Strip BPM", stripBPM},
    {"Strip Plasma", stripPlasma},
    
    // Motion effects (ALL NOW CENTER ORIGIN COMPLIANT)
    {"Confetti", confetti},
    {"Sinelon", sinelon},
    {"Juggle", juggle},
    {"BPM", bpm},
    
    // Palette showcase
    {"Solid Blue", solidColor},
    {"Pulse Effect", pulseEffect}
};
#else
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
#endif

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
    
    #if LED_STRIPS_MODE
    // Initialize I2C and M5ROTATE8 with dedicated task architecture
    Serial.println("Initializing M5ROTATE8 with dedicated I2C task...");
    Wire.begin(HardwareConfig::I2C_SDA, HardwareConfig::I2C_SCL);
    Wire.setClock(400000);  // Max supported speed (400 KHz)
    
    // Create encoder event queue (16 events max)
    encoderEventQueue = xQueueCreate(16, sizeof(EncoderEvent));
    if (encoderEventQueue == NULL) {
        Serial.println("ERROR: Failed to create encoder event queue");
        encoderAvailable = false;
    } else {
        Serial.println("Encoder event queue created successfully");
        
        // Initial connection attempt
        encoderAvailable = encoder.begin();
        if (encoderAvailable && encoder.isConnected()) {
            Serial.print("M5ROTATE8 connected! Firmware: V");
            Serial.println(encoder.getVersion());
            
            // Set all LEDs to dim blue idle state
            encoder.setAll(0, 0, 16);
            
            Serial.println("Encoder mapping:");
            Serial.println("  0: Effect selection  1: Brightness");
            Serial.println("  2: Palette           3: Speed");
            Serial.println("  4-7: Reserved");
        } else {
            Serial.println("M5ROTATE8 not found initially - task will attempt reconnection");
            encoderAvailable = false;
        }
        
        // Create dedicated I2C task on Core 0 (main loop runs on Core 1)
        xTaskCreatePinnedToCore(
            encoderTask,           // Task function
            "EncoderI2CTask",      // Task name
            4096,                  // Stack size (4KB)
            NULL,                  // Parameters
            2,                     // Priority (higher than idle)
            &encoderTaskHandle,    // Task handle
            0                      // Core 0 (main loop on Core 1)
        );
        
        if (encoderTaskHandle != NULL) {
            Serial.println("✅ I2C Encoder Task created on Core 0");
            Serial.println("🚀 Main loop now NON-BLOCKING for encoder operations");
            Serial.println("📋 Task will attempt encoder connection and handle all I2C operations");
            Serial.println("💬 Look for TASK: messages from I2C task, MAIN: from main loop");
        } else {
            Serial.println("❌ ERROR: Failed to create encoder task");
        }
    }
    #endif
    
    // Light Guide Mode disabled - implementation removed
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

#if LED_STRIPS_MODE
    // Process encoder events from I2C task (NON-BLOCKING)
    if (encoderEventQueue != NULL) {
        EncoderEvent event;
        
        // Process all available events in queue (non-blocking)
        while (xQueueReceive(encoderEventQueue, &event, 0) == pdTRUE) {
            char buffer[64];
            
            switch (event.encoder_id) {
                case 0: // Effect selection
                    if (event.delta > 0) {
                        startTransition((currentEffect + 1) % NUM_EFFECTS);
                    } else {
                        startTransition(currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1);
                    }
                    Serial.printf("🎨 MAIN: Effect changed to %s (index %d)\n", effects[currentEffect].name, currentEffect);
                    break;
                    
                case 1: // Brightness
                    {
                        int brightness = FastLED.getBrightness() + (event.delta > 0 ? 16 : -16);
                        brightness = constrain(brightness, 16, 255);
                        FastLED.setBrightness(brightness);
                        Serial.printf("💡 MAIN: Brightness changed to %d\n", brightness);
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
                    break;
                    
                case 3: // Speed control
                    paletteSpeed = constrain(paletteSpeed + (event.delta > 0 ? 2 : -2), 1, 50);
                    Serial.printf("⚡ MAIN: Speed changed to %d\n", paletteSpeed);
                    break;
            }
        }
    }
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
    
    // LED strips are updated directly by effects - no sync needed
    
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
    
    // Auto-cycle effects every 15 seconds for demonstration
    EVERY_N_SECONDS(15) {
        uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
        startTransition(nextEffect);
        Serial.print("Auto-switching to: ");
        Serial.println(effects[nextEffect].name);
    }
    
    // Frame rate control
    delay(1000/HardwareConfig::DEFAULT_FPS);
}