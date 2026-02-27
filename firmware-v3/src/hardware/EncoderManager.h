/**
 * @file EncoderManager.h
 * @brief M5Stack ROTATE8 Encoder Manager for LightwaveOS v2
 *
 * Provides hardware HMI support via M5Stack 8-encoder unit connected over I2C.
 * Runs as a FreeRTOS task on Core 0 to avoid blocking the render loop on Core 1.
 *
 * Features:
 * - Detent-aware debouncing for clean encoder events
 * - I2C bus recovery for robustness
 * - Exponential backoff reconnection
 * - LED feedback on encoder activity
 * - Performance metrics and diagnostics
 *
 * Encoder Mapping:
 *   0: Effect selection
 *   1: Brightness
 *   2: Palette
 *   3: Speed
 *   4: Intensity
 *   5: Saturation
 *   6: Complexity
 *   7: Variation
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_ROTATE8_ENCODER

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "m5rotate8.h"

namespace lightwaveos {
namespace actors {
    class ActorSystem;
    class RendererActor;
}
}

namespace lightwaveos {
namespace hardware {

// ============================================================================
// Constants
// ============================================================================

namespace EncoderConfig {
    // I2C Configuration
    constexpr uint8_t I2C_SDA = 17;
    constexpr uint8_t I2C_SCL = 18;
    constexpr uint8_t M5ROTATE8_ADDRESS = 0x41;

    // Task Configuration
    constexpr uint16_t TASK_STACK_SIZE = 4096;
    constexpr uint8_t TASK_PRIORITY = 1;
    constexpr uint8_t TASK_CORE = 0;  // Core 0 for I2C operations
    constexpr uint16_t POLL_INTERVAL_MS = 20;  // 50Hz polling

    // Event Queue
    constexpr uint8_t EVENT_QUEUE_SIZE = 64;

    // Debouncing
    constexpr uint32_t DEBOUNCE_INTERVAL_MS = 60;  // Minimum between events

    // LED Timing
    constexpr uint32_t LED_FLASH_DURATION_MS = 300;
    constexpr uint32_t LED_UPDATE_INTERVAL_MS = 100;

    // Reconnection
    constexpr uint32_t INITIAL_BACKOFF_MS = 1000;
    constexpr uint32_t MAX_BACKOFF_MS = 30000;

    // Serial Rate Limiting
    constexpr uint32_t SERIAL_RATE_LIMIT_MS = 100;
    constexpr uint32_t METRICS_REPORT_INTERVAL_MS = 60000;

    // Encoder Count
    constexpr uint8_t NUM_ENCODERS = 8;
}

// ============================================================================
// Encoder Event
// ============================================================================

/**
 * @brief Encoder event structure for queue communication
 *
 * Sent from the encoder task (Core 0) to the main loop (Core 1)
 * via a FreeRTOS queue.
 */
struct EncoderEvent {
    uint8_t encoder_id;      // Encoder index (0-7)
    int32_t delta;           // Normalized delta (+1/-1 per detent)
    bool button_pressed;     // Button state (future use)
    uint32_t timestamp;      // Event timestamp (millis)
};

// ============================================================================
// Encoder Metrics
// ============================================================================

/**
 * @brief Performance metrics for encoder subsystem
 */
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

    /**
     * @brief Record an encoder event
     * @param queued Whether the event was successfully queued
     * @param response_time_us I2C transaction time in microseconds
     */
    void recordEvent(bool queued, uint32_t response_time_us);

    /**
     * @brief Record an I2C transaction
     * @param success Whether the transaction succeeded
     */
    void recordI2CTransaction(bool success);

    /**
     * @brief Record a connection loss
     */
    void recordConnectionLoss();

    /**
     * @brief Record a successful reconnection
     */
    void recordReconnect();

    /**
     * @brief Update queue depth tracking
     * @param depth Current queue depth
     */
    void updateQueueDepth(uint8_t depth);

    /**
     * @brief Print performance report if interval elapsed
     */
    void printReport();

    /**
     * @brief Reset counters for next reporting period
     */
    void resetCounters();
};

// ============================================================================
// Detent Debounce
// ============================================================================

/**
 * @brief Detent-aware debouncing for mechanical encoders
 *
 * The M5ROTATE8 encoders report 2 counts per detent (mechanical click).
 * This class normalizes the counts to 1 event per detent and handles
 * timing-based debouncing.
 */
struct DetentDebounce {
    int32_t pending_count = 0;
    uint32_t last_event_time = 0;
    uint32_t last_emit_time = 0;
    bool expecting_pair = false;

    /**
     * @brief Process raw encoder delta
     * @param raw_delta Raw count from encoder
     * @param now Current timestamp (millis)
     * @return true if a normalized event should be emitted
     */
    bool processRawDelta(int32_t raw_delta, uint32_t now);

    /**
     * @brief Get the normalized delta value
     * @return Normalized delta (+1/-1 per detent)
     */
    int32_t getNormalizedDelta();
};

// ============================================================================
// EncoderManager Class
// ============================================================================

/**
 * @brief M5Stack ROTATE8 Encoder Manager
 *
 * Manages the M5Stack 8-encoder unit via I2C, running as a FreeRTOS task.
 * Encoder events are sent to the main loop via a queue for thread-safe
 * integration with the Actor system.
 *
 * Usage:
 *   EncoderManager encoderManager;
 *   encoderManager.begin();
 *
 *   // In main loop:
 *   EncoderEvent event;
 *   while (xQueueReceive(encoderManager.getEventQueue(), &event, 0) == pdTRUE) {
 *       handleEncoderEvent(event);
 *   }
 */
class EncoderManager {
public:
    /**
     * @brief Constructor
     */
    EncoderManager();

    /**
     * @brief Destructor - cleans up task and queue
     */
    ~EncoderManager();

    // Prevent copying
    EncoderManager(const EncoderManager&) = delete;
    EncoderManager& operator=(const EncoderManager&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialize encoder system
     *
     * Creates the FreeRTOS task and event queue. Attempts initial
     * connection to the M5ROTATE8. If not found, the task will
     * continue attempting reconnection with exponential backoff.
     *
     * @return true if task and queue created successfully
     */
    bool begin();

    // ========================================================================
    // Accessors
    // ========================================================================

    /**
     * @brief Get the event queue handle
     *
     * Use this to receive encoder events in the main loop.
     *
     * @return FreeRTOS queue handle, or NULL if not initialized
     */
    QueueHandle_t getEventQueue() const { return m_eventQueue; }

    /**
     * @brief Check if encoder hardware is available
     * @return true if M5ROTATE8 is connected and responding
     */
    bool isAvailable() const { return m_encoderAvailable; }

    /**
     * @brief Get performance metrics
     * @return Reference to encoder metrics
     */
    const EncoderMetrics& getMetrics() const { return m_metrics; }

    /**
     * @brief Get raw encoder hardware pointer (for advanced use)
     * @return M5ROTATE8 pointer, or nullptr if not available
     */
    M5ROTATE8* getEncoder() { return m_encoderAvailable ? m_encoder : nullptr; }

    // ========================================================================
    // LED Control
    // ========================================================================

    /**
     * @brief Set individual encoder LED color
     *
     * Thread-safe via I2C mutex.
     *
     * @param encoderId Encoder index (0-7)
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void setEncoderLED(uint8_t encoderId, uint8_t r, uint8_t g, uint8_t b);

    // ========================================================================
    // Diagnostics
    // ========================================================================

    /**
     * @brief Rate-limited serial output
     *
     * Prevents serial spam during rapid encoder activity.
     *
     * @param message Message to print
     */
    void rateLimitedSerial(const char* message);

private:
    // ========================================================================
    // FreeRTOS Task
    // ========================================================================

    /**
     * @brief Static wrapper for FreeRTOS task
     * @param parameter Pointer to EncoderManager instance
     */
    static void taskWrapper(void* parameter);

    /**
     * @brief Main encoder polling task
     */
    void encoderTask();

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Initialize M5ROTATE8 hardware
     * @return true if encoder found and initialized
     */
    bool initializeM5Rotate8();

    /**
     * @brief Process encoder events from hardware
     */
    void processEncoderEvents();

    /**
     * @brief Attempt to reconnect to encoder
     * @return true if reconnection successful
     */
    bool attemptReconnection();

    /**
     * @brief Update encoder LED states
     * @param now Current timestamp (millis)
     */
    void updateEncoderLEDs(uint32_t now);

    /**
     * @brief Perform I2C bus recovery sequence
     * @param sda SDA pin
     * @param scl SCL pin
     */
    void performI2CBusRecovery(uint8_t sda, uint8_t scl);

    // ========================================================================
    // Member Variables
    // ========================================================================

    // Encoder hardware
    M5ROTATE8* m_encoder = nullptr;
    bool m_encoderAvailable = false;

    // FreeRTOS handles
    TaskHandle_t m_taskHandle = nullptr;
    QueueHandle_t m_eventQueue = nullptr;

    // Debouncing state per encoder
    DetentDebounce m_debounce[EncoderConfig::NUM_ENCODERS];

    // Performance metrics
    EncoderMetrics m_metrics;

    // Connection health
    uint32_t m_lastConnectionCheck = 0;
    uint32_t m_failCount = 0;
    uint32_t m_reconnectBackoffMs = EncoderConfig::INITIAL_BACKOFF_MS;
    bool m_suspended = false;

    // LED flash timing
    uint32_t m_ledFlashTime[EncoderConfig::NUM_ENCODERS] = {0};
    bool m_ledNeedsUpdate[EncoderConfig::NUM_ENCODERS] = {false};

    // Serial rate limiting
    uint32_t m_lastSerialOutput = 0;
};

// ============================================================================
// Global I2C Mutex
// ============================================================================

/**
 * @brief Global I2C mutex for thread-safe bus access
 *
 * Must be created before EncoderManager::begin() is called.
 * Typically created in main.cpp setup().
 */
extern SemaphoreHandle_t i2cMutex;

// ============================================================================
// Global Instance
// ============================================================================

/**
 * @brief Global encoder manager instance
 */
extern EncoderManager encoderManager;

/**
 * @brief Process encoder event and dispatch to Actor system
 * @param event The encoder event to process
 * @param actors Reference to ActorSystem
 * @param renderer Pointer to RendererActor
 */
void handleEncoderEvent(const EncoderEvent& event, 
                       lightwaveos::actors::ActorSystem& actors,
                       lightwaveos::actors::RendererActor* renderer);

} // namespace hardware
} // namespace lightwaveos

#endif // FEATURE_ROTATE8_ENCODER
