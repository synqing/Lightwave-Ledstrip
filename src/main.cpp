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

// Universal visual parameters for encoders 4-7
struct VisualParams {
    uint8_t intensity = 128;      // Effect intensity/amplitude (0-255)
    uint8_t saturation = 255;     // Color saturation (0-255)
    uint8_t complexity = 128;     // Effect complexity/detail (0-255)
    uint8_t variation = 0;        // Effect variation/mode (0-255)
    
    // Helper functions for normalized access
    float getIntensityNorm() { return intensity / 255.0f; }
    float getSaturationNorm() { return saturation / 255.0f; }
    float getComplexityNorm() { return complexity / 255.0f; }
    float getVariationNorm() { return variation / 255.0f; }
} visualParams;

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

// Balanced detent handling for M5ROTATE8 - responsive but stable
struct DetentDebounce {
    int32_t pending_count = 0;          // Current pending count
    uint32_t last_event_time = 0;       // Last encoder activity
    uint32_t last_emit_time = 0;        // Last time we sent an event
    bool expecting_pair = false;        // Expecting second count of a pair
    
    // Handle M5ROTATE8's variable count behavior (usually ±2, sometimes ±1)
    bool processRawDelta(int32_t raw_delta, uint32_t now) {
        // No change - don't emit anything
        if (raw_delta == 0) {
            return false;
        }
        
        // Update timing
        last_event_time = now;
        
        // Handle different count scenarios
        if (abs(raw_delta) == 2) {
            // Full detent in one read - emit immediately if not too soon
            pending_count = (raw_delta > 0) ? 1 : -1;
            expecting_pair = false;
            
            if (now - last_emit_time >= 60) { // 60ms minimum between events
                last_emit_time = now;
                return true;
            }
            pending_count = 0; // Too soon, discard
            
        } else if (abs(raw_delta) == 1) {
            // Single count - might be half a detent or a full slow detent
            if (!expecting_pair) {
                // First single count
                pending_count = raw_delta;
                expecting_pair = true;
                // Don't emit yet - wait to see if we get a pair
            } else {
                // Second single count
                if ((pending_count > 0 && raw_delta > 0) || 
                    (pending_count < 0 && raw_delta < 0)) {
                    // Same direction - treat as full detent
                    expecting_pair = false;
                    pending_count = (pending_count > 0) ? 1 : -1;
                    
                    if (now - last_emit_time >= 60) {
                        last_emit_time = now;
                        return true;
                    }
                } else {
                    // Direction change - start over
                    pending_count = raw_delta;
                    expecting_pair = true;
                }
            }
        } else {
            // Unusual count (>2) - emit normalized if not too soon
            pending_count = (raw_delta > 0) ? 1 : -1;
            expecting_pair = false;
            
            if (now - last_emit_time >= 60) {
                last_emit_time = now;
                return true;
            }
        }
        
        return false;
    }
    
    // Get normalized delta (always ±1)
    int32_t getNormalizedDelta() {
        int32_t result = pending_count;
        pending_count = 0;
        expecting_pair = false;
        return result;
    }
};

DetentDebounce encoderDebounce[8];

// Performance monitoring system
struct EncoderMetrics {
    // Event tracking
    uint32_t total_events = 0;
    uint32_t successful_events = 0;
    uint32_t dropped_events = 0;
    uint32_t queue_full_count = 0;
    
    // I2C health
    uint32_t i2c_transactions = 0;
    uint32_t i2c_failures = 0;
    uint32_t connection_losses = 0;
    uint32_t successful_reconnects = 0;
    
    // Timing metrics
    uint32_t total_response_time_us = 0;
    uint32_t max_response_time_us = 0;
    uint32_t min_response_time_us = UINT32_MAX;
    
    // Queue metrics
    uint8_t max_queue_depth = 0;
    uint8_t current_queue_depth = 0;
    
    // Reporting
    uint32_t last_report_time = 0;
    uint32_t report_interval_ms = 10000;  // 10 seconds
    
    void recordEvent(bool queued, uint32_t response_time_us) {
        total_events++;
        if (queued) {
            successful_events++;
            total_response_time_us += response_time_us;
            max_response_time_us = max(max_response_time_us, response_time_us);
            min_response_time_us = min(min_response_time_us, response_time_us);
        } else {
            dropped_events++;
            queue_full_count++;
        }
    }
    
    void recordI2CTransaction(bool success) {
        i2c_transactions++;
        if (!success) {
            i2c_failures++;
        }
    }
    
    void recordConnectionLoss() {
        connection_losses++;
    }
    
    void recordReconnect() {
        successful_reconnects++;
    }
    
    void updateQueueDepth(uint8_t depth) {
        current_queue_depth = depth;
        max_queue_depth = max(max_queue_depth, depth);
    }
    
    void printReport() {
        uint32_t now = millis();
        if (now - last_report_time < report_interval_ms) return;
        
        last_report_time = now;
        
        Serial.println("\n📊 === ENCODER PERFORMANCE REPORT ===");
        
        // Calculate success rates
        float event_success_rate = total_events > 0 ? 
            (100.0f * successful_events / total_events) : 100.0f;
        float i2c_success_rate = i2c_transactions > 0 ? 
            (100.0f * (i2c_transactions - i2c_failures) / i2c_transactions) : 100.0f;
        
        // Calculate average response time
        float avg_response_ms = successful_events > 0 ? 
            (total_response_time_us / 1000.0f / successful_events) : 0.0f;
        
        Serial.printf("🔄 I2C Health: %.1f%% success (%u/%u transactions)\n", 
                     i2c_success_rate, i2c_transactions - i2c_failures, i2c_transactions);
        Serial.printf("📡 Connection: %u losses, %u successful reconnects\n",
                     connection_losses, successful_reconnects);
        Serial.printf("🎮 Events: %u total, %.1f%% delivered, %u dropped\n",
                     total_events, event_success_rate, dropped_events);
        Serial.printf("⏱️ Response: %.2fms avg, %.2fms max, %.2fms min\n",
                     avg_response_ms, max_response_time_us / 1000.0f, 
                     min_response_time_us == UINT32_MAX ? 0.0f : min_response_time_us / 1000.0f);
        Serial.printf("📦 Queue: %u/%u current, %u max depth, %u overflows\n",
                     current_queue_depth, 16, max_queue_depth, queue_full_count);
        
        // Calculate events per minute
        float events_per_min = (total_events * 60000.0f) / report_interval_ms;
        Serial.printf("⚡ Activity: %.1f events/min\n", events_per_min);
        
        Serial.println("=====================================\n");
        
        // Reset counters for next period
        total_events = 0;
        successful_events = 0;
        dropped_events = 0;
        i2c_transactions = 0;
        i2c_failures = 0;
        total_response_time_us = 0;
        max_response_time_us = 0;
        min_response_time_us = UINT32_MAX;
        max_queue_depth = current_queue_depth;
    }
};

EncoderMetrics encoderMetrics;

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

// Dual-Strip Wave Engine
#if LED_STRIPS_MODE
#include "effects/waves/DualStripWaveEngine.h"
#include "effects/waves/WaveEffects.h"
#include "effects/waves/WaveEncoderControl.h"

// Global wave engine instance (stack-allocated, ~200 bytes)
DualStripWaveEngine waveEngine;
#endif

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
                    
                    // Record successful reconnection
                    encoderMetrics.recordReconnect();
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
        
        for (int i = 0; i < 8; i++) {
            // Track I2C transaction start time
            uint32_t transaction_start = micros();
            
            if (!encoder.isConnected()) {
                connectionLost = true;
                encoderMetrics.recordI2CTransaction(false);
                break;
            }
            
            // Read relative counter with error handling
            int32_t raw_delta = encoder.getRelCounter(i);
            
            // Reset the counter after reading to prevent accumulation
            if (raw_delta != 0) {
                encoder.resetCounter(i);
            }
            encoderMetrics.recordI2CTransaction(true);
            
            // Process through detent-aware debouncing
            DetentDebounce& debounce = encoderDebounce[i];
            
            if (debounce.processRawDelta(raw_delta, now)) {
                // Get normalized delta
                int32_t delta = debounce.getNormalizedDelta();
                
                // Create event for main loop
                EncoderEvent event = {
                    .encoder_id = (uint8_t)i,
                    .delta = delta,
                    .button_pressed = false,
                    .timestamp = now
                };
                
                // Calculate response time
                uint32_t response_time_us = micros() - transaction_start;
                
                // Update queue depth before sending
                UBaseType_t queue_spaces = uxQueueSpacesAvailable(encoderEventQueue);
                encoderMetrics.updateQueueDepth(16 - queue_spaces);
                
                // Send to main loop via queue (non-blocking)
                bool queued = (xQueueSend(encoderEventQueue, &event, 0) == pdTRUE);
                
                // Record metrics
                encoderMetrics.recordEvent(queued, response_time_us);
                
                if (!queued) {
                    Serial.println("⚠️ TASK: Encoder event queue full - dropping event");
                } else if (i == 0) {
                    // Only log encoder 0 (effect selection) to reduce spam
                    Serial.printf("📤 TASK: Encoder %d delta %+d (raw: %+d)\n", 
                                 i, delta, raw_delta);
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
            encoderMetrics.recordConnectionLoss();
            continue;
        }
        
        // Print performance report periodically
        encoderMetrics.printReport();
        
        // Update encoder LEDs every 100ms to show status
        static uint32_t lastLEDUpdate = 0;
        if (now - lastLEDUpdate >= 100) {
            lastLEDUpdate = now;
            
            for (int i = 0; i < 8; i++) {
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
                            case 4: encoder.writeRGB(i, 16, 0, 8); break;   // Orange - Intensity
                            case 5: encoder.writeRGB(i, 0, 16, 16); break;  // Cyan - Saturation
                            case 6: encoder.writeRGB(i, 8, 16, 0); break;   // Lime - Complexity
                            case 7: encoder.writeRGB(i, 16, 0, 16); break;  // Magenta - Variation
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
    
    // Spawn new ripples at CENTER ONLY - frequency controlled by complexity
    uint8_t spawnChance = 30 * visualParams.getComplexityNorm();
    if (random8() < spawnChance) {
        for (uint8_t i = 0; i < 5; i++) {
            if (!ripples[i].active) {
                ripples[i].radius = 0;
                ripples[i].speed = (0.5f + (random8() / 255.0f) * 2.0f) * visualParams.getIntensityNorm();
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
                brightness = brightness * visualParams.getIntensityNorm();
                
                CRGB color = ColorFromPalette(currentPalette, ripples[r].hue + distFromCenter, brightness);
                // Apply saturation control
                color = blend(CRGB::White, color, visualParams.saturation);
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
    
    // Ignite new sparks at CENTER (79/80) - intensity controls spark frequency and heat
    uint8_t sparkChance = 120 * visualParams.getIntensityNorm();
    if(random8() < sparkChance) {
        int center = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80
        uint8_t heatAmount = 160 + (95 * visualParams.getIntensityNorm());
        heat[center] = qadd8(heat[center], random8(160, heatAmount));
    }
    
    // Map heat to both strips with CENTER ORIGIN
    for(int j = 0; j < HardwareConfig::STRIP_LENGTH; j++) {
        // Scale heat by intensity
        uint8_t scaledHeat = heat[j] * visualParams.getIntensityNorm();
        CRGB color = HeatColor(scaledHeat);
        
        // Apply saturation control (desaturate towards white)
        color = blend(CRGB::White, color, visualParams.saturation);
        
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

// ============== NEW CENTER ORIGIN EFFECTS ==============

// HEARTBEAT - Pulses emanate from center like a beating heart
void heartbeatEffect() {
    #if LED_STRIPS_MODE
    static float phase = 0;
    static float lastBeat = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Heartbeat rhythm: lub-dub... lub-dub...
    float beatPattern = sin(phase) + sin(phase * 2.1f) * 0.4f;
    
    if (beatPattern > 1.8f && phase - lastBeat > 2.0f) {
        lastBeat = phase;
        // Generate pulse from CENTER
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
            
            // Pulse intensity decreases with distance
            uint8_t brightness = 255 * (1.0f - normalizedDist);
            CRGB color = ColorFromPalette(currentPalette, gHue + normalizedDist * 50, brightness);
            
            strip1[i] += color;
            strip2[i] += color;
        }
    }
    
    phase += paletteSpeed / 200.0f;
    #endif
}

// BREATHING - Smooth expansion and contraction from center
void breathingEffect() {
    #if LED_STRIPS_MODE
    static float breathPhase = 0;
    
    // Smooth breathing curve
    float breath = (sin(breathPhase) + 1.0f) / 2.0f;
    float radius = breath * HardwareConfig::STRIP_HALF_LENGTH;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 15);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 15);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        if (distFromCenter <= radius) {
            float intensity = 1.0f - (distFromCenter / radius) * 0.5f;
            uint8_t brightness = 255 * intensity * breath;
            
            CRGB color = ColorFromPalette(currentPalette, gHue + distFromCenter * 3, brightness);
            strip1[i] = color;
            strip2[i] = color;
        }
    }
    
    breathPhase += paletteSpeed / 100.0f;
    #endif
}

// SHOCKWAVE - Explosive rings emanate from center
void shockwaveEffect() {
    #if LED_STRIPS_MODE
    static float shockwaves[5] = {-1, -1, -1, -1, -1};
    static uint8_t waveHues[5] = {0};
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 25);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 25);
    
    // Spawn new shockwave from CENTER - complexity controls frequency
    uint8_t spawnChance = 20 * visualParams.getComplexityNorm();
    if (random8() < spawnChance) {
        for (int w = 0; w < 5; w++) {
            if (shockwaves[w] < 0) {
                shockwaves[w] = 0;
                waveHues[w] = gHue + random8(64);
                break;
            }
        }
    }
    
    // Update and render shockwaves
    for (int w = 0; w < 5; w++) {
        if (shockwaves[w] >= 0) {
            shockwaves[w] += (paletteSpeed / 20.0f) * visualParams.getIntensityNorm();
            
            if (shockwaves[w] > HardwareConfig::STRIP_HALF_LENGTH) {
                shockwaves[w] = -1;
                continue;
            }
            
            // Render shockwave ring
            for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
                float ringDist = abs(distFromCenter - shockwaves[w]);
                
                // Ring thickness controlled by complexity
                float ringThickness = 3.0f + (3.0f * visualParams.getComplexityNorm());
                if (ringDist < ringThickness) {
                    float intensity = 1.0f - (ringDist / ringThickness);
                    uint8_t brightness = 255 * intensity * (1.0f - shockwaves[w] / HardwareConfig::STRIP_HALF_LENGTH);
                    brightness = brightness * visualParams.getIntensityNorm();
                    
                    CRGB color = ColorFromPalette(currentPalette, waveHues[w], brightness);
                    // Apply saturation control
                    color = blend(CRGB::White, color, visualParams.saturation);
                    strip1[i] += color;
                    strip2[i] += color;
                }
            }
        }
    }
    #endif
}

// VORTEX - Spiral patterns emanating from center
void vortexEffect() {
    #if LED_STRIPS_MODE
    static float vortexAngle = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Spiral calculation
        float spiralOffset = normalizedDist * 8.0f + vortexAngle;
        float intensity = sin(spiralOffset) * 0.5f + 0.5f;
        intensity *= (1.0f - normalizedDist * 0.5f); // Fade towards edges
        
        uint8_t brightness = 255 * intensity;
        uint8_t hue = gHue + distFromCenter * 5 + vortexAngle * 20;
        
        CRGB color = ColorFromPalette(currentPalette, hue, brightness);
        
        // Opposite spiral direction for strip2
        if (i < HardwareConfig::STRIP_CENTER_POINT) {
            strip1[i] = color;
            strip2[HardwareConfig::STRIP_LENGTH - 1 - i] = color;
        } else {
            strip1[i] = color;
            strip2[HardwareConfig::STRIP_LENGTH - 1 - i] = color;
        }
    }
    
    vortexAngle += paletteSpeed / 50.0f;
    #endif
}

// COLLISION - Particles shoot from edges to center and explode
void collisionEffect() {
    #if LED_STRIPS_MODE
    static float particle1Pos = 0;
    static float particle2Pos = HardwareConfig::STRIP_LENGTH - 1;
    static bool exploding = false;
    static float explosionRadius = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 30);
    
    if (!exploding) {
        // Move particles toward center
        particle1Pos += paletteSpeed / 10.0f;
        particle2Pos -= paletteSpeed / 10.0f;
        
        // Draw particles
        for (int trail = 0; trail < 10; trail++) {
            int pos1 = particle1Pos - trail;
            int pos2 = particle2Pos + trail;
            
            if (pos1 >= 0 && pos1 < HardwareConfig::STRIP_LENGTH) {
                uint8_t brightness = 255 - (trail * 25);
                strip1[pos1] = ColorFromPalette(currentPalette, gHue, brightness);
                strip2[pos1] = ColorFromPalette(currentPalette, gHue + 128, brightness);
            }
            
            if (pos2 >= 0 && pos2 < HardwareConfig::STRIP_LENGTH) {
                uint8_t brightness = 255 - (trail * 25);
                strip1[pos2] = ColorFromPalette(currentPalette, gHue + 128, brightness);
                strip2[pos2] = ColorFromPalette(currentPalette, gHue, brightness);
            }
        }
        
        // Check for collision at center
        if (particle1Pos >= HardwareConfig::STRIP_CENTER_POINT - 5 && 
            particle2Pos <= HardwareConfig::STRIP_CENTER_POINT + 5) {
            exploding = true;
            explosionRadius = 0;
        }
    } else {
        // Explosion expanding from center
        explosionRadius += paletteSpeed / 5.0f;
        
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            
            if (distFromCenter <= explosionRadius && distFromCenter >= explosionRadius - 10) {
                float intensity = 1.0f - ((distFromCenter - (explosionRadius - 10)) / 10.0f);
                uint8_t brightness = 255 * intensity;
                
                CRGB color = ColorFromPalette(currentPalette, gHue + random8(64), brightness);
                strip1[i] += color;
                strip2[i] += color;
            }
        }
        
        // Reset when explosion reaches edges
        if (explosionRadius > HardwareConfig::STRIP_HALF_LENGTH + 10) {
            exploding = false;
            particle1Pos = 0;
            particle2Pos = HardwareConfig::STRIP_LENGTH - 1;
        }
    }
    #endif
}

// GRAVITY WELL - Everything gets pulled toward center
void gravityWellEffect() {
    #if LED_STRIPS_MODE
    static struct GravityParticle {
        float position;
        float velocity;
        uint8_t hue;
        bool active;
    } particles[20];
    
    static bool initialized = false;
    if (!initialized) {
        for (int p = 0; p < 20; p++) {
            particles[p].position = random16(HardwareConfig::STRIP_LENGTH);
            particles[p].velocity = 0;
            particles[p].hue = random8();
            particles[p].active = true;
        }
        initialized = true;
    }
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Update particles with gravity toward center
    for (int p = 0; p < 20; p++) {
        if (particles[p].active) {
            float distFromCenter = particles[p].position - HardwareConfig::STRIP_CENTER_POINT;
            float gravity = -distFromCenter * 0.01f * paletteSpeed / 10.0f;
            
            particles[p].velocity += gravity;
            particles[p].velocity *= 0.95f; // Damping
            particles[p].position += particles[p].velocity;
            
            // Respawn at edges if particle reaches center
            if (abs(particles[p].position - HardwareConfig::STRIP_CENTER_POINT) < 2) {
                particles[p].position = random8(2) ? 0 : HardwareConfig::STRIP_LENGTH - 1;
                particles[p].velocity = 0;
                particles[p].hue = random8();
            }
            
            // Draw particle with motion blur
            int pos = particles[p].position;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                strip1[pos] += ColorFromPalette(currentPalette, particles[p].hue, 255);
                strip2[pos] += ColorFromPalette(currentPalette, particles[p].hue + 64, 255);
                
                // Motion blur
                for (int blur = 1; blur < 4; blur++) {
                    int blurPos = pos - (particles[p].velocity > 0 ? blur : -blur);
                    if (blurPos >= 0 && blurPos < HardwareConfig::STRIP_LENGTH) {
                        uint8_t brightness = 255 / (blur + 1);
                        strip1[blurPos] += ColorFromPalette(currentPalette, particles[p].hue, brightness);
                        strip2[blurPos] += ColorFromPalette(currentPalette, particles[p].hue + 64, brightness);
                    }
                }
            }
        }
    }
    #endif
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
enum EffectType {
    EFFECT_TYPE_STANDARD,
    EFFECT_TYPE_WAVE_ENGINE
};

struct Effect {
    const char* name;
    EffectFunction function;
    EffectType type;
};

#if LED_STRIPS_MODE
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
};
#else
Effect effects[] = {
    // Signature effects
    {"Fire", fire, EFFECT_TYPE_STANDARD},
    {"Ocean", ocean, EFFECT_TYPE_STANDARD},
    {"Plasma", plasma, EFFECT_TYPE_STANDARD},
    
    // Wave dynamics  
    {"Wave", waveEffect, EFFECT_TYPE_STANDARD},
    {"Ripple", rippleEffect, EFFECT_TYPE_STANDARD},
    {"Interference", interferenceEffect, EFFECT_TYPE_STANDARD},
    
    // Mathematical patterns
    {"Fibonacci", fibonacciSpiral, EFFECT_TYPE_STANDARD},
    {"Kaleidoscope", kaleidoscope, EFFECT_TYPE_STANDARD},
    
    // Motion effects
    {"Confetti", confetti, EFFECT_TYPE_STANDARD},
    {"Sinelon", sinelon, EFFECT_TYPE_STANDARD},
    {"Juggle", juggle, EFFECT_TYPE_STANDARD},
    {"BPM", bpm, EFFECT_TYPE_STANDARD},
    
    // Palette showcase
    {"Solid Blue", solidColor, EFFECT_TYPE_STANDARD},
    {"Pulse Effect", pulseEffect, EFFECT_TYPE_STANDARD}
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
            Serial.println("  4: Intensity         5: Saturation");
            Serial.println("  6: Complexity        7: Variation");
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
                    startTransition(newEffect);
                    Serial.printf("🎨 MAIN: Effect changed to %s (index %d)\n", effects[currentEffect].name, currentEffect);
                    
                    // Log mode transition if effect type changed
                    if (effects[newEffect].type != effects[currentEffect].type) {
                        Serial.printf("🔄 MAIN: Encoder mode switched to %s\n", 
                                     effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE ? "WAVE ENGINE" : "STANDARD");
                    }
                }
                
            } else if (effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE) {
                // Wave engine parameter control for encoders 1-7
                handleWaveEncoderInput(event.encoder_id, event.delta, waveEngine);
            } else {
                // Standard effect control for encoders 1-7
                switch (event.encoder_id) {
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
                        
                    case 4: // Intensity/Amplitude
                        visualParams.intensity = constrain(visualParams.intensity + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("🔥 MAIN: Intensity changed to %d (%.1f%%)\n", visualParams.intensity, visualParams.getIntensityNorm() * 100);
                        break;
                        
                    case 5: // Saturation
                        visualParams.saturation = constrain(visualParams.saturation + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("🎨 MAIN: Saturation changed to %d (%.1f%%)\n", visualParams.saturation, visualParams.getSaturationNorm() * 100);
                        break;
                        
                    case 6: // Complexity/Detail
                        visualParams.complexity = constrain(visualParams.complexity + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("✨ MAIN: Complexity changed to %d (%.1f%%)\n", visualParams.complexity, visualParams.getComplexityNorm() * 100);
                        break;
                        
                    case 7: // Variation/Mode
                        visualParams.variation = constrain(visualParams.variation + (event.delta > 0 ? 16 : -16), 0, 255);
                        Serial.printf("🔄 MAIN: Variation changed to %d (%.1f%%)\n", visualParams.variation, visualParams.getVariationNorm() * 100);
                        break;
                }
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
        
        // Quick encoder performance summary
        #if LED_STRIPS_MODE
        if (encoderAvailable) {
            Serial.printf("Encoder: %u queue depth, %.1f%% I2C success\n",
                         encoderMetrics.current_queue_depth,
                         encoderMetrics.i2c_transactions > 0 ? 
                         (100.0f * (encoderMetrics.i2c_transactions - encoderMetrics.i2c_failures) / encoderMetrics.i2c_transactions) : 100.0f);
        }
        #endif
        
        // Update wave engine performance stats if using wave effects
        if (effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE) {
            updateWavePerformanceStats(waveEngine);
        }
    }
    
    
    // Frame rate control
    delay(1000/HardwareConfig::DEFAULT_FPS);
}