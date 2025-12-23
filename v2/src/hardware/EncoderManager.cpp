/**
 * @file EncoderManager.cpp
 * @brief M5Stack ROTATE8 Encoder Manager implementation for LightwaveOS v2
 *
 * This file implements the encoder manager which runs as a FreeRTOS task
 * on Core 0, polling the M5Stack 8-encoder unit via I2C and sending
 * events to the main loop via a queue.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "EncoderManager.h"

#if FEATURE_ROTATE8_ENCODER

#include "../core/actors/ActorSystem.h"

namespace lightwaveos {
namespace hardware {

// ============================================================================
// Global Instances
// ============================================================================

// Global I2C mutex - must be created in main.cpp before begin()
SemaphoreHandle_t i2cMutex = nullptr;

// Global encoder manager instance
EncoderManager encoderManager;

// ============================================================================
// EncoderMetrics Implementation
// ============================================================================

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
    if (now - last_report_time < EncoderConfig::METRICS_REPORT_INTERVAL_MS) {
        return;
    }

    last_report_time = now;

    // Only show report if there are issues
    if (dropped_events > 0 || queue_full_count > 0 || connection_losses > 0) {
        Serial.println("\n[EncoderManager] Performance Alert");

        float event_success_rate = total_events > 0 ?
            (100.0f * successful_events / total_events) : 100.0f;

        Serial.printf("  Events: %u total, %.1f%% delivered, %u dropped\n",
                     total_events, event_success_rate, dropped_events);
        Serial.printf("  Queue: %u overflows, max depth %u/%u\n",
                     queue_full_count, max_queue_depth, EncoderConfig::EVENT_QUEUE_SIZE);

        if (connection_losses > 0) {
            Serial.printf("  Connection: %u losses, %u reconnects\n",
                         connection_losses, successful_reconnects);
        }
    }

    resetCounters();
}

void EncoderMetrics::resetCounters() {
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

// ============================================================================
// DetentDebounce Implementation
// ============================================================================

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

        if (now - last_emit_time >= EncoderConfig::DEBOUNCE_INTERVAL_MS) {
            last_emit_time = now;
            return true;
        }
        pending_count = 0;  // Too soon, discard

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

                if (now - last_emit_time >= EncoderConfig::DEBOUNCE_INTERVAL_MS) {
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

        if (now - last_emit_time >= EncoderConfig::DEBOUNCE_INTERVAL_MS) {
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

// ============================================================================
// EncoderManager Implementation
// ============================================================================

EncoderManager::EncoderManager() {
    memset(m_ledFlashTime, 0, sizeof(m_ledFlashTime));
    memset(m_ledNeedsUpdate, 0, sizeof(m_ledNeedsUpdate));
}

EncoderManager::~EncoderManager() {
    // Safely delete task
    if (m_taskHandle != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(m_taskHandle);
        m_taskHandle = nullptr;
    }

    // Delete queue
    if (m_eventQueue != nullptr) {
        vQueueDelete(m_eventQueue);
        m_eventQueue = nullptr;
    }

    // Clean up encoder hardware
    if (m_encoder) {
        delete m_encoder;
        m_encoder = nullptr;
    }
}

bool EncoderManager::begin() {
    // Prevent double initialization
    if (m_taskHandle != nullptr) {
        Serial.println("[EncoderManager] WARNING: Already initialized");
        return true;
    }

    Serial.println("[EncoderManager] Initializing encoder system...");

    // Verify I2C mutex exists
    if (i2cMutex == nullptr) {
        Serial.println("[EncoderManager] ERROR: I2C mutex not created");
        Serial.println("[EncoderManager] Create i2cMutex in main.cpp setup() before calling begin()");
        return false;
    }

    // Create encoder event queue
    m_eventQueue = xQueueCreate(EncoderConfig::EVENT_QUEUE_SIZE, sizeof(EncoderEvent));
    if (m_eventQueue == nullptr) {
        Serial.println("[EncoderManager] ERROR: Failed to create event queue");
        return false;
    }

    Serial.println("[EncoderManager] Event queue created");

    // Attempt initial connection to M5ROTATE8
    if (!initializeM5Rotate8()) {
        Serial.println("[EncoderManager] M5ROTATE8 not found - task will retry");
        m_encoderAvailable = false;
    }

    // Create dedicated I2C task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        taskWrapper,
        "EncoderI2C",
        EncoderConfig::TASK_STACK_SIZE,
        this,
        EncoderConfig::TASK_PRIORITY,
        &m_taskHandle,
        EncoderConfig::TASK_CORE
    );

    if (result != pdPASS || m_taskHandle == nullptr) {
        Serial.println("[EncoderManager] ERROR: Failed to create task");
        vQueueDelete(m_eventQueue);
        m_eventQueue = nullptr;
        return false;
    }

    Serial.println("[EncoderManager] I2C task created on Core 0");
    Serial.println("[EncoderManager] Encoder mapping:");
    Serial.println("  0: Effect    1: Brightness  2: Palette  3: Speed");
    Serial.println("  4: Intensity 5: Saturation  6: Complexity 7: Variation");

    return true;
}

void EncoderManager::taskWrapper(void* parameter) {
    EncoderManager* manager = static_cast<EncoderManager*>(parameter);
    manager->encoderTask();
}

void EncoderManager::encoderTask() {
    Serial.println("[EncoderManager] Task started on Core 0");

    while (true) {
        uint32_t now = millis();

        // Connection health check with exponential backoff
        if (!m_encoderAvailable || m_suspended) {
            if (now - m_lastConnectionCheck >= m_reconnectBackoffMs) {
                m_lastConnectionCheck = now;

                if (attemptReconnection()) {
                    m_encoderAvailable = true;
                    m_suspended = false;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    continue;
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        // Poll encoders with I2C mutex
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            processEncoderEvents();
            xSemaphoreGive(i2cMutex);
        } else {
            // Could not get mutex - skip this cycle
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        // Print performance report periodically
        m_metrics.printReport();

        // Update encoder LEDs
        updateEncoderLEDs(now);

        // Task runs at 50Hz
        vTaskDelay(pdMS_TO_TICKS(EncoderConfig::POLL_INTERVAL_MS));
    }
}

bool EncoderManager::initializeM5Rotate8() {
    Serial.printf("[EncoderManager] Scanning I2C on GPIO %d/%d...\n",
                  EncoderConfig::I2C_SDA, EncoderConfig::I2C_SCL);

    if (m_encoder) {
        delete m_encoder;
    }

    m_encoder = new M5ROTATE8(EncoderConfig::M5ROTATE8_ADDRESS);

    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
        bool success = m_encoder->begin();
        if (success && m_encoder->isConnected()) {
            Serial.printf("[EncoderManager] M5ROTATE8 connected, firmware V%d\n",
                         m_encoder->getVersion());

            // Set all LEDs to dim blue idle state
            m_encoder->setAll(0, 0, 16);

            m_encoderAvailable = true;
            xSemaphoreGive(i2cMutex);
            return true;
        }

        xSemaphoreGive(i2cMutex);
        delete m_encoder;
        m_encoder = nullptr;
        Serial.println("[EncoderManager] M5ROTATE8 not found");
    }

    return false;
}

bool EncoderManager::attemptReconnection() {
    Serial.printf("[EncoderManager] Reconnection attempt (backoff: %dms)\n",
                  m_reconnectBackoffMs);

    bool success = initializeM5Rotate8();

    if (success) {
        m_failCount = 0;
        m_reconnectBackoffMs = EncoderConfig::INITIAL_BACKOFF_MS;
        Serial.println("[EncoderManager] Reconnected successfully");
        m_metrics.recordReconnect();
    } else {
        m_failCount++;
        // Exponential backoff: 1s, 2s, 4s, 8s, max 30s
        uint32_t maxShift = min(m_failCount, (uint32_t)5);
        m_reconnectBackoffMs = min(
            (uint32_t)(EncoderConfig::INITIAL_BACKOFF_MS * (1 << maxShift)),
            EncoderConfig::MAX_BACKOFF_MS
        );
        Serial.printf("[EncoderManager] Reconnection failed (%d attempts)\n", m_failCount);
    }

    return success;
}

void EncoderManager::processEncoderEvents() {
    if (!m_encoder || !m_encoder->isConnected()) {
        return;
    }

    // Poll all 8 encoders
    for (uint8_t i = 0; i < EncoderConfig::NUM_ENCODERS; i++) {
        uint32_t transaction_start = micros();

        // Read relative counter
        int32_t raw_delta = m_encoder->getRelCounter(i);

        // Reset counter after reading
        if (raw_delta != 0) {
            m_encoder->resetCounter(i);
        }

        m_metrics.recordI2CTransaction(true);

        // Process through detent-aware debouncing
        DetentDebounce& debounce = m_debounce[i];

        if (debounce.processRawDelta(raw_delta, millis())) {
            int32_t delta = debounce.getNormalizedDelta();

            // Create event
            EncoderEvent event = {
                .encoder_id = i,
                .delta = delta,
                .button_pressed = false,
                .timestamp = millis()
            };

            uint32_t response_time_us = micros() - transaction_start;

            // Update queue depth
            UBaseType_t spaces = uxQueueSpacesAvailable(m_eventQueue);
            m_metrics.updateQueueDepth(EncoderConfig::EVENT_QUEUE_SIZE - spaces);

            // Send to main loop via queue (non-blocking)
            bool queued = (xQueueSend(m_eventQueue, &event, 0) == pdTRUE);
            m_metrics.recordEvent(queued, response_time_us);

            if (queued) {
                // Flash LED for activity feedback
                m_ledFlashTime[i] = millis();
                m_ledNeedsUpdate[i] = true;

                // Log encoder 0 (effect selection) with throttling
                if (i == 0) {
                    static uint32_t lastLog = 0;
                    uint32_t now = millis();
                    if (now - lastLog > 500) {
                        lastLog = now;
                        Serial.printf("[EncoderManager] Encoder %d delta %+d\n", i, delta);
                    }
                }
            }
        }

        // Small delay between encoder reads
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void EncoderManager::updateEncoderLEDs(uint32_t now) {
    if (!m_encoder) return;

    static uint32_t lastUpdate = 0;
    if (now - lastUpdate < EncoderConfig::LED_UPDATE_INTERVAL_MS) {
        return;
    }
    lastUpdate = now;

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (uint8_t i = 0; i < EncoderConfig::NUM_ENCODERS; i++) {
            if (m_ledNeedsUpdate[i]) {
                if (now - m_ledFlashTime[i] < EncoderConfig::LED_FLASH_DURATION_MS) {
                    // Green flash for activity
                    m_encoder->writeRGB(i, 0, 64, 0);
                } else {
                    // Return to function-colored idle state
                    switch (i) {
                        case 0: m_encoder->writeRGB(i, 16, 0, 0); break;   // Red - Effect
                        case 1: m_encoder->writeRGB(i, 16, 16, 16); break; // White - Brightness
                        case 2: m_encoder->writeRGB(i, 8, 0, 16); break;   // Purple - Palette
                        case 3: m_encoder->writeRGB(i, 16, 8, 0); break;   // Yellow - Speed
                        case 4: m_encoder->writeRGB(i, 16, 0, 8); break;   // Orange - Intensity
                        case 5: m_encoder->writeRGB(i, 0, 16, 16); break;  // Cyan - Saturation
                        case 6: m_encoder->writeRGB(i, 8, 16, 0); break;   // Lime - Complexity
                        case 7: m_encoder->writeRGB(i, 16, 0, 16); break;  // Magenta - Variation
                    }
                    m_ledNeedsUpdate[i] = false;
                }
            }
        }
        xSemaphoreGive(i2cMutex);
    }
}

void EncoderManager::setEncoderLED(uint8_t encoderId, uint8_t r, uint8_t g, uint8_t b) {
    if (!m_encoderAvailable || !m_encoder) return;

    if (encoderId < EncoderConfig::NUM_ENCODERS) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            m_encoder->writeRGB(encoderId, r, g, b);
            xSemaphoreGive(i2cMutex);
        }
    }
}

void EncoderManager::rateLimitedSerial(const char* message) {
    uint32_t now = millis();
    if (now - m_lastSerialOutput >= EncoderConfig::SERIAL_RATE_LIMIT_MS) {
        m_lastSerialOutput = now;
        Serial.print(message);
    }
}

void EncoderManager::performI2CBusRecovery(uint8_t sda, uint8_t scl) {
    Serial.println("[EncoderManager] I2C Bus Recovery Sequence:");

    // Configure pins as GPIO outputs
    pinMode(sda, OUTPUT);
    pinMode(scl, OUTPUT);

    // Set both lines high initially
    digitalWrite(sda, HIGH);
    digitalWrite(scl, HIGH);
    delay(5);

    // Check initial state
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    bool sdaStuck = !digitalRead(sda);
    bool sclStuck = !digitalRead(scl);

    if (sdaStuck || sclStuck) {
        Serial.printf("[EncoderManager] Stuck lines - SDA: %s, SCL: %s\n",
                     sdaStuck ? "LOW" : "OK",
                     sclStuck ? "LOW" : "OK");
    }

    // Perform clock pulsing to release stuck devices
    pinMode(scl, OUTPUT);
    pinMode(sda, INPUT_PULLUP);

    Serial.println("[EncoderManager] Sending 9 clock pulses...");
    for (int i = 0; i < 9; i++) {
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);

        if (digitalRead(sda)) {
            Serial.printf("[EncoderManager] SDA released after %d pulses\n", i + 1);
            break;
        }
    }

    // Generate STOP condition
    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);

    // Final check
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delay(10);

    bool sdaFinal = digitalRead(sda);
    bool sclFinal = digitalRead(scl);
    Serial.printf("[EncoderManager] Final state - SDA: %s, SCL: %s\n",
                 sdaFinal ? "HIGH" : "LOW",
                 sclFinal ? "HIGH" : "LOW");

    if (!sdaFinal || !sclFinal) {
        Serial.println("[EncoderManager] WARNING: I2C lines still stuck!");
        Serial.println("  Check for: shorts, missing pull-ups, wiring, power");
    } else {
        Serial.println("[EncoderManager] I2C bus recovery complete");
    }

    delay(50);
}

// ============================================================================
// Event Handler Helper (for main.cpp integration)
// ============================================================================

/**
 * @brief Process encoder event and dispatch to Actor system
 *
 * This helper function translates encoder events into Actor system
 * commands. Call this from the main loop after receiving events
 * from the encoder queue.
 *
 * Example usage in main.cpp:
 *
 *   // In main loop:
 *   EncoderEvent event;
 *   while (xQueueReceive(encoderManager.getEventQueue(), &event, 0) == pdTRUE) {
 *       handleEncoderEvent(event, actors, renderer);
 *   }
 *   #endif
 *
 * @param event The encoder event to process
 * @param actors Reference to ActorSystem
 * @param renderer Pointer to RendererActor
 */
void handleEncoderEvent(const EncoderEvent& event, 
                       lightwaveos::actors::ActorSystem& actors,
                       lightwaveos::actors::RendererActor* renderer) {
    using namespace lightwaveos::actors;

    if (!renderer) return;

    switch (event.encoder_id) {
        case 0: {
            // Effect selection
            // Use actual effect count from renderer
            uint8_t effectCount = renderer->getEffectCount();
            if (effectCount == 0) effectCount = 45;  // Fallback
            uint8_t current = renderer->getCurrentEffect();
            int16_t newEffect = (int16_t)current + event.delta;
            if (newEffect < 0) newEffect = effectCount - 1;
            if (newEffect >= effectCount) newEffect = 0;
            actors.setEffect((uint8_t)newEffect);
            break;
        }

        case 1: {
            // Brightness (0-255, steps of 8)
            uint8_t current = renderer->getBrightness();
            int16_t newVal = (int16_t)current + (event.delta * 8);
            newVal = constrain(newVal, 0, 255);
            actors.setBrightness((uint8_t)newVal);
            break;
        }

        case 2: {
            // Palette selection
            uint8_t paletteCount = renderer->getPaletteCount();
            if (paletteCount == 0) paletteCount = 20;  // Fallback
            uint8_t current = renderer->getPaletteIndex();
            int16_t newPalette = (int16_t)current + event.delta;
            if (newPalette < 0) newPalette = paletteCount - 1;
            if (newPalette >= paletteCount) newPalette = 0;
            actors.setPalette((uint8_t)newPalette);
            break;
        }

        case 3: {
            // Speed (1-50)
            uint8_t current = renderer->getSpeed();
            int16_t newVal = (int16_t)current + event.delta;
            newVal = constrain(newVal, 1, 50);
            actors.setSpeed((uint8_t)newVal);
            break;
        }

        case 4:
        case 5:
        case 6:
        case 7:
            // Reserved for future parameters (intensity, saturation, complexity, variation)
            // These can be mapped when additional parameters are exposed via Actor system
            break;
    }
}

} // namespace hardware
} // namespace lightwaveos

#endif // FEATURE_ROTATE8_ENCODER
