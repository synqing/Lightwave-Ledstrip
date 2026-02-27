// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LedStreamBroadcaster.h
 * @brief LED stream broadcaster for WebSocket clients
 *
 * Manages LED frame subscriptions and broadcasting to WebSocket clients.
 * Handles subscription lifecycle, throttling, and client cleanup.
 *
 * Extracted from WebServer for better separation of concerns.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "../SubscriptionManager.h"
#include "LedFrameEncoder.h"
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
 * @brief Broadcasts LED frames to subscribed WebSocket clients
 */
class LedStreamBroadcaster {
public:
    /**
     * @brief Construct broadcaster
     * @param ws WebSocket server instance
     * @param maxClients Maximum number of subscribers (unused, kept for API compatibility)
     * @param timeSource Time source for throttling (nullptr = use millis)
     */
    LedStreamBroadcaster(AsyncWebSocket* ws, size_t maxClients, ITimeSource* timeSource = nullptr);
    
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
     * @brief Broadcast LED frame to all subscribers
     * @param leds LED buffer (must have TOTAL_LEDS entries)
     * @return Number of clients that received the frame
     */
    size_t broadcast(const CRGB* leds);
    
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
    uint8_t m_frameBuffer[LedStreamConfig::FRAME_SIZE];
#if defined(ESP32)
    mutable portMUX_TYPE m_mux;
#endif
    
    uint32_t getTime() const;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

