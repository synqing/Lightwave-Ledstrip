// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioStreamBroadcaster.h
 * @brief Audio stream broadcaster for WebSocket clients
 *
 * Manages audio frame subscriptions and broadcasting to WebSocket clients.
 * Handles subscription lifecycle, throttling, and client cleanup.
 *
 * Pattern follows LedStreamBroadcaster for consistency.
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC

#include <ESPAsyncWebServer.h>
#include "../SubscriptionManager.h"
#include "AudioStreamConfig.h"
#include "AudioFrameEncoder.h"
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
 * @brief Broadcasts audio frames to subscribed WebSocket clients
 */
class AudioStreamBroadcaster {
public:
    /**
     * @brief Construct broadcaster
     * @param ws WebSocket server instance
     * @param timeSource Time source for throttling (nullptr = use millis)
     */
    AudioStreamBroadcaster(AsyncWebSocket* ws, ITimeSource* timeSource = nullptr);

    /**
     * @brief Destructor
     */
    ~AudioStreamBroadcaster();

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
     * @brief Broadcast audio frame to all subscribers
     * @param frame Audio control bus frame to broadcast
     * @param grid Musical grid snapshot with tempo/beat data
     * @return Number of clients that received the frame
     */
    size_t broadcast(const audio::ControlBusFrame& frame, const audio::MusicalGridSnapshot& grid);

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
    SubscriptionManager<AudioStreamConfig::MAX_CLIENTS> m_subscribers;
    ITimeSource* m_timeSource;
    ArduinoTimeSource* m_defaultTimeSource;
    uint32_t m_lastBroadcast;
    uint8_t m_frameBuffer[AudioStreamConfig::FRAME_SIZE];
#if defined(ESP32)
    mutable portMUX_TYPE m_mux;
#endif

    uint32_t getTime() const;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
