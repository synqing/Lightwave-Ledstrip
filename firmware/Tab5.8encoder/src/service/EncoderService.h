#pragma once

#include <Arduino.h>
#include "src/transport/Rotate8Transport.h"
#include "src/processing/EncoderProcessing.h"

/**
 * @file EncoderService.h
 * @brief Encoder service integration layer
 * 
 * Polls transport layer, feeds processing layer, emits events via callback.
 * Non-blocking, rate-limited polling.
 */

class EncoderService {
public:
    /**
     * @brief Constructor
     * @param transport Rotate8Transport instance
     * @param processing EncoderProcessing instance
     */
    EncoderService(Rotate8Transport& transport, EncoderProcessing& processing);

    /**
     * @brief Initialise service
     * @return true if successful
     */
    bool begin();

    /**
     * @brief Update service (call from main loop)
     * Polls encoders, processes deltas/buttons, emits events
     */
    void update();

    /**
     * @brief Set callback for parameter changes
     * @param callback Function to call: void callback(uint8_t paramId, uint16_t value, bool isReset)
     */
    void setCallback(EncoderEventCallback callback);

    /**
     * @brief Check if encoder hardware is connected
     * @return true if connected
     */
    bool isConnected();

private:
    Rotate8Transport& _transport;
    EncoderProcessing& _processing;
    
    // Rate limiting
    uint32_t _last_poll_ms;
    static const uint32_t POLL_INTERVAL_MS = 20;  // Don't hammer I2C
    
    // Lockout to avoid rapid cross-channel reads
    uint8_t _last_active_channel;
    uint32_t _last_channel_change_time[8];
    static const uint32_t CHANNEL_LOCKOUT_MS = 50;
};

