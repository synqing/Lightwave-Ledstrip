/**
 * @file BenchmarkStreamBroadcaster.h
 * @brief Benchmark metrics broadcaster for WebSocket clients
 *
 * Manages benchmark subscription and broadcasting to WebSocket clients.
 * Streams AudioBenchmarkStats at 10 Hz when clients are subscribed.
 *
 * Pattern follows AudioStreamBroadcaster for consistency.
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_AUDIO_BENCHMARK

#include <ESPAsyncWebServer.h>
#include "../SubscriptionManager.h"
#include "BenchmarkStreamConfig.h"
#include "BenchmarkFrameEncoder.h"
#include "../../audio/AudioBenchmarkMetrics.h"
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

/**
 * @brief Broadcasts benchmark metrics to subscribed WebSocket clients
 */
class BenchmarkStreamBroadcaster {
public:
    /**
     * @brief Construct broadcaster
     * @param ws WebSocket server instance
     * @param timeSource Time source for throttling (nullptr = use millis)
     */
    BenchmarkStreamBroadcaster(AsyncWebSocket* ws, ITimeSource* timeSource = nullptr);

    /**
     * @brief Destructor
     */
    ~BenchmarkStreamBroadcaster();

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
     * @brief Broadcast compact stats frame to all subscribers
     * @param stats Benchmark stats to broadcast
     * @return Number of clients that received the frame
     */
    size_t broadcastCompact(const audio::AudioBenchmarkStats& stats);

    /**
     * @brief Broadcast extended stats frame to all subscribers
     * @param stats Benchmark stats to broadcast
     * @return Number of clients that received the frame
     */
    size_t broadcastExtended(const audio::AudioBenchmarkStats& stats);

    /**
     * @brief Clean up disconnected clients
     * Should be called periodically to remove stale subscriptions
     */
    void cleanupDisconnected();

    /**
     * @brief Get current subscriber count
     */
    size_t getSubscriberCount() const;

    /**
     * @brief Set streaming active flag
     */
    void setStreamingActive(bool active) { m_streamingActive = active; }

    /**
     * @brief Check if streaming is active
     */
    bool isStreamingActive() const { return m_streamingActive; }

private:
    AsyncWebSocket* m_ws;
    SubscriptionManager<BenchmarkStreamConfig::MAX_CLIENTS> m_subscribers;
    ITimeSource* m_timeSource;
    ArduinoTimeSource* m_defaultTimeSource;
    uint32_t m_lastBroadcast;
    bool m_streamingActive;
    uint8_t m_frameBuffer[BenchmarkStreamConfig::EXTENDED_FRAME_SIZE];
#if defined(ESP32)
    mutable portMUX_TYPE m_mux;
#endif

    uint32_t getTime() const;
    uint8_t getFlags() const;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_BENCHMARK
