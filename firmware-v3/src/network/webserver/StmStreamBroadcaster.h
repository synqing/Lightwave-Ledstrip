/**
 * @file StmStreamBroadcaster.h
 * @brief STM (spectral-temporal modulation) stream broadcaster for WebSocket clients
 *
 * Manages STM frame subscriptions and broadcasting to WebSocket clients.
 * Handles subscription lifecycle, throttling, and client cleanup.
 *
 * Pattern follows LedStreamBroadcaster for consistency.
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC

#include <ESPAsyncWebServer.h>
#include "../SubscriptionManager.h"
#include "../../audio/contracts/ControlBus.h"
#include <stdint.h>

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace lightwaveos {
namespace network {
namespace webserver {

class ITimeSource;
class ArduinoTimeSource;

// ============================================================================
// STM Stream Configuration
// ============================================================================

namespace StmStreamConfig {
    static constexpr uint8_t SPECTRAL_BINS = audio::ControlBusFrame::STM_SPECTRAL_BINS;  // 42
    static constexpr uint8_t TEMPORAL_BANDS = audio::ControlBusFrame::STM_MEL_BANDS;     // 16
    static constexpr uint8_t MAGIC_BYTE = 0xFD;  // Distinct from LED (0xFE) and audio streams

    // Frame layout:
    //   1  magic byte
    //   1  ready flag
    //   SPECTRAL_BINS * 4  spectral floats   (42 * 4 = 168)
    //   TEMPORAL_BANDS * 4  temporal floats   (16 * 4 = 64)
    //   4 * 4  scalar floats (spectralEnergy, temporalEnergy, centroid, dominantBin)
    // Total: 1 + 1 + 168 + 64 + 16 = 250 bytes
    static constexpr uint16_t FRAME_SIZE =
        1                           // magic
        + 1                         // ready flag
        + (SPECTRAL_BINS * 4)       // spectral bins (float32)
        + (TEMPORAL_BANDS * 4)      // temporal bands (float32)
        + (4 * 4);                  // four scalar floats

    static constexpr uint8_t TARGET_FPS = 30;
    static constexpr uint32_t FRAME_INTERVAL_MS = 33;
}

// ============================================================================
// STM Stream Broadcaster
// ============================================================================

/**
 * @brief Broadcasts STM frames to subscribed WebSocket clients
 */
class StmStreamBroadcaster {
public:
    /**
     * @brief Construct broadcaster
     * @param ws WebSocket server instance
     * @param maxClients Maximum number of subscribers (unused, kept for API compatibility)
     * @param timeSource Time source for throttling (nullptr = use millis)
     */
    StmStreamBroadcaster(AsyncWebSocket* ws, size_t maxClients, ITimeSource* timeSource = nullptr);

    /**
     * @brief Subscribe/unsubscribe a client
     * @param clientId WebSocket client ID
     * @param subscribe true to subscribe, false to unsubscribe
     * @return true if subscription state changed
     */
    bool setSubscription(uint32_t clientId, bool subscribe);

    /**
     * @brief Check if any clients are subscribed
     */
    bool hasSubscribers() const;

    /**
     * @brief Broadcast STM frame to all subscribers
     * @param frame Audio control bus frame containing STM data
     * @return Number of clients that received the frame
     */
    size_t broadcast(const audio::ControlBusFrame& frame);

    /**
     * @brief Clean up disconnected clients
     * Should be called periodically to remove stale subscriptions
     */
    void cleanupDisconnected();

    /**
     * @brief Get current subscriber count
     */
    size_t getSubscriberCount() const;

private:
    AsyncWebSocket* m_ws;
    SubscriptionManager<8> m_subscribers;  // MAX_WS_CLIENTS = 8
    ITimeSource* m_timeSource;
    ArduinoTimeSource* m_defaultTimeSource;
    uint32_t m_lastBroadcast;
    uint8_t m_frameBuffer[StmStreamConfig::FRAME_SIZE];
#if defined(ESP32)
    mutable portMUX_TYPE m_mux;
#endif

    uint32_t getTime() const;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
