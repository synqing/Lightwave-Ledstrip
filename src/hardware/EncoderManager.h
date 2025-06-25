#ifndef ENCODER_MANAGER_H
#define ENCODER_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "m5rotate8.h"
#include "../config/hardware_config.h"
#include "../core/EffectTypes.h"

// Encoder event structure for queue communication
struct EncoderEvent {
    uint8_t encoder_id;
    int32_t delta;
    bool button_pressed;
    uint32_t timestamp;
};

// Encoder performance monitoring
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
    
    void recordEvent(bool queued, uint32_t response_time_us);
    void recordI2CTransaction(bool success);
    void recordConnectionLoss();
    void recordReconnect();
    void updateQueueDepth(uint8_t depth);
    void printReport();
};

// Detent debouncing for M5ROTATE8
struct DetentDebounce {
    int32_t pending_count = 0;
    uint32_t last_event_time = 0;
    uint32_t last_emit_time = 0;
    bool expecting_pair = false;
    
    bool processRawDelta(int32_t raw_delta, uint32_t now);
    int32_t getNormalizedDelta();
};

// Encoder Manager class
class EncoderManager {
private:
    // Encoder hardware
    M5ROTATE8 encoder;
    bool encoderAvailable = false;
    
    // FreeRTOS task and queue
    TaskHandle_t encoderTaskHandle = NULL;
    QueueHandle_t encoderEventQueue = NULL;
    
    // Debouncing
    DetentDebounce encoderDebounce[8];
    
    // Performance metrics
    EncoderMetrics metrics;
    
    // Connection health
    uint32_t lastConnectionCheck = 0;
    uint32_t encoderFailCount = 0;
    uint32_t reconnectBackoffMs = 1000;
    bool encoderSuspended = false;
    
    // LED flash timing
    uint32_t ledFlashTime[8] = {0};
    bool ledNeedsUpdate[8] = {false};
    
    // Serial rate limiting
    uint32_t lastSerialOutput = 0;
    static constexpr uint32_t SERIAL_RATE_LIMIT_MS = 100;
    
    // Task function (static for FreeRTOS)
    static void encoderTaskWrapper(void* parameter);
    void encoderTask();
    
    // Internal methods
    bool attemptReconnection();
    void updateEncoderLEDs(uint32_t now);
    void setEncoderLED(uint8_t encoderId, uint8_t r, uint8_t g, uint8_t b);
    
public:
    EncoderManager();
    ~EncoderManager();
    
    // Initialize encoder system
    bool begin();
    
    // Get event queue handle
    QueueHandle_t getEventQueue() { return encoderEventQueue; }
    
    // Check if encoder is available
    bool isAvailable() { return encoderAvailable; }
    
    // Get metrics
    const EncoderMetrics& getMetrics() { return metrics; }
    
    // Get encoder instance for VFS
    M5ROTATE8* getEncoder() { return encoderAvailable ? &encoder : nullptr; }
    
    // Rate-limited serial output
    void rateLimitedSerial(const char* message);
};

// Global instance (defined in EncoderManager.cpp)
extern EncoderManager encoderManager;

#endif // ENCODER_MANAGER_H