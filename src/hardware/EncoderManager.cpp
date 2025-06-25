#include "EncoderManager.h"

// Global instance
EncoderManager encoderManager;

// External dependencies
extern uint8_t currentEffect;
extern const uint8_t NUM_EFFECTS;
extern void startTransition(uint8_t newEffect);

// Constructor
EncoderManager::EncoderManager() {
    // Initialize arrays
    memset(ledFlashTime, 0, sizeof(ledFlashTime));
    memset(ledNeedsUpdate, 0, sizeof(ledNeedsUpdate));
}

// Destructor
EncoderManager::~EncoderManager() {
    if (encoderTaskHandle != NULL) {
        vTaskDelete(encoderTaskHandle);
    }
    if (encoderEventQueue != NULL) {
        vQueueDelete(encoderEventQueue);
    }
}

// Initialize encoder system
bool EncoderManager::begin() {
    Serial.println("Initializing M5Stack 8Encoder with dedicated I2C task...");
    
    // Create encoder event queue (16 events max)
    encoderEventQueue = xQueueCreate(16, sizeof(EncoderEvent));
    if (encoderEventQueue == NULL) {
        Serial.println("ERROR: Failed to create encoder event queue");
        return false;
    }
    
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
        encoderTaskWrapper,        // Task function
        "EncoderI2CTask",         // Task name
        4096,                     // Stack size (4KB)
        this,                     // Parameters (this object)
        2,                        // Priority (higher than idle)
        &encoderTaskHandle,       // Task handle
        0                         // Core 0 (main loop on Core 1)
    );
    
    if (encoderTaskHandle != NULL) {
        Serial.println("✅ I2C Encoder Task created on Core 0");
        Serial.println("🚀 Main loop now NON-BLOCKING for encoder operations");
        Serial.println("📋 Task will attempt encoder connection and handle all I2C operations");
        Serial.println("💬 Look for TASK: messages from I2C task, MAIN: from main loop");
        return true;
    } else {
        Serial.println("❌ ERROR: Failed to create encoder task");
        return false;
    }
}

// Static wrapper for FreeRTOS task
void EncoderManager::encoderTaskWrapper(void* parameter) {
    EncoderManager* manager = static_cast<EncoderManager*>(parameter);
    manager->encoderTask();
}

// Main encoder task
void EncoderManager::encoderTask() {
    Serial.println("🚀 I2C Encoder Task started on Core 0");
    Serial.println("📡 Task will handle all I2C operations independently");
    Serial.println("⚡ Main loop is now NON-BLOCKING for encoder operations");
    
    while (true) {
        uint32_t now = millis();
        
        // Connection health check with exponential backoff
        if (!encoderAvailable || encoderSuspended) {
            if (now - lastConnectionCheck >= reconnectBackoffMs) {
                lastConnectionCheck = now;
                
                if (attemptReconnection()) {
                    encoderAvailable = true;
                    encoderSuspended = false;
                } else {
                    // Wait longer when disconnected to avoid spam
                    vTaskDelay(pdMS_TO_TICKS(500));
                    continue;
                }
            }
        }
        
        // Poll encoders safely in dedicated task
        bool connectionLost = false;
        
        for (int i = 0; i < 8; i++) {
            // Track I2C transaction start time
            uint32_t transaction_start = micros();
            
            if (!encoder.isConnected()) {
                connectionLost = true;
                metrics.recordI2CTransaction(false);
                break;
            }
            
            // Read relative counter with error handling
            int32_t raw_delta = encoder.getRelCounter(i);
            
            // Reset the counter after reading to prevent accumulation
            if (raw_delta != 0) {
                encoder.resetCounter(i);
            }
            metrics.recordI2CTransaction(true);
            
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
                metrics.updateQueueDepth(16 - queue_spaces);
                
                // Send to main loop via queue (non-blocking)
                bool queued = (xQueueSend(encoderEventQueue, &event, 0) == pdTRUE);
                
                // Record metrics
                metrics.recordEvent(queued, response_time_us);
                
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
            metrics.recordConnectionLoss();
            continue;
        }
        
        // Print performance report periodically
        metrics.printReport();
        
        // Update encoder LEDs
        updateEncoderLEDs(now);
        
#if FEATURE_WIRELESS_ENCODERS
        // Update wireless receiver
        if (wirelessEnabled) {
            wirelessReceiver.update();
        }
#endif
        
        // Task runs at 50Hz (20ms intervals)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Attempt to reconnect to encoder
bool EncoderManager::attemptReconnection() {
    Serial.printf("🔄 TASK: Attempting encoder reconnection (backoff: %dms)...\n", reconnectBackoffMs);
    
    if (encoder.begin() && encoder.isConnected()) {
        Serial.println("✅ TASK: Encoder reconnected successfully!");
        Serial.printf("📋 TASK: Firmware V%d\n", encoder.getVersion());
        encoderFailCount = 0;
        reconnectBackoffMs = 1000;  // Reset backoff
        
        // Initialize encoder LEDs to idle state
        encoder.setAll(0, 0, 16);
        Serial.println("💡 TASK: Encoder LEDs initialized to idle state");
        
        // Record successful reconnection
        metrics.recordReconnect();
        return true;
    } else {
        encoderFailCount++;
        // Exponential backoff: 1s, 2s, 4s, 8s, max 30s
        uint32_t maxShift = min(encoderFailCount, (uint32_t)5);
        reconnectBackoffMs = min((uint32_t)(1000 * (1 << maxShift)), (uint32_t)30000);
        Serial.printf("❌ TASK: Reconnection failed (%d attempts), next try in %dms\n", 
                     encoderFailCount, reconnectBackoffMs);
        return false;
    }
}

// Update encoder LEDs
void EncoderManager::updateEncoderLEDs(uint32_t now) {
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
}

// Set individual encoder LED
void EncoderManager::setEncoderLED(uint8_t encoderId, uint8_t r, uint8_t g, uint8_t b) {
    if (encoderId < 8 && encoderAvailable) {
        encoder.writeRGB(encoderId, r, g, b);
    }
}

// Rate-limited serial output
void EncoderManager::rateLimitedSerial(const char* message) {
    uint32_t now = millis();
    if (now - lastSerialOutput >= SERIAL_RATE_LIMIT_MS) {
        lastSerialOutput = now;
        Serial.print(message);
    }
}

// Wireless encoder methods
#if FEATURE_WIRELESS_ENCODERS
bool EncoderManager::enableWireless() {
    if (!wirelessEnabled) {
        if (wirelessReceiver.initialize()) {
            wirelessEnabled = true;
            Serial.println("✅ Wireless encoder receiver enabled");
            return true;
        } else {
            Serial.println("❌ Failed to enable wireless encoder receiver");
            return false;
        }
    }
    return true;
}

void EncoderManager::disableWireless() {
    wirelessEnabled = false;
    Serial.println("🔄 Wireless encoder receiver disabled");
}

bool EncoderManager::isWirelessConnected() const {
    return wirelessEnabled && wirelessReceiver.isConnected();
}

void EncoderManager::startWirelessPairing() {
    if (wirelessEnabled) {
        Serial.println("🔗 Wireless pairing mode active - waiting for transmitter...");
        Serial.println("   Power on your wireless encoder transmitter device");
    } else {
        Serial.println("⚠️ Wireless not enabled");
    }
}
#endif

// EncoderMetrics implementation
void EncoderMetrics::recordEvent(bool queued, uint32_t response_time_us) {
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

void EncoderMetrics::recordI2CTransaction(bool success) {
    i2c_transactions++;
    if (!success) {
        i2c_failures++;
    }
}

void EncoderMetrics::recordConnectionLoss() {
    connection_losses++;
}

void EncoderMetrics::recordReconnect() {
    successful_reconnects++;
}

void EncoderMetrics::updateQueueDepth(uint8_t depth) {
    current_queue_depth = depth;
    max_queue_depth = max(max_queue_depth, depth);
}

void EncoderMetrics::printReport() {
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

// DetentDebounce implementation
bool DetentDebounce::processRawDelta(int32_t raw_delta, uint32_t now) {
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

int32_t DetentDebounce::getNormalizedDelta() {
    int32_t result = pending_count;
    pending_count = 0;
    expecting_pair = false;
    return result;
}