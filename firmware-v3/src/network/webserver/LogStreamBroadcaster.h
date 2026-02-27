// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LogStreamBroadcaster.h
 * @brief Log stream broadcaster for WebSocket clients
 *
 * Enables wireless serial monitoring by streaming log messages to
 * subscribed WebSocket clients. Maintains a ring buffer for backfill
 * so new clients receive recent log history.
 *
 * Thread-safe: logs can come from any core.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "../SubscriptionManager.h"
#include <stdint.h>
#include <stddef.h>

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief Configuration for log streaming
 */
struct LogStreamConfig {
    static constexpr size_t RING_BUFFER_SIZE = 100;      // Number of messages to keep
    static constexpr size_t MAX_MESSAGE_LENGTH = 256;    // Max chars per log message
    static constexpr uint32_t THROTTLE_MS = 10;          // Min ms between broadcasts
};

/**
 * @brief Broadcasts log messages to subscribed WebSocket clients
 *
 * Usage:
 * 1. Create instance with WebSocket server pointer
 * 2. Call setSubscription() when clients subscribe/unsubscribe
 * 3. Call broadcast() from Log.h callback to send messages
 * 4. Call sendBackfill() when new client subscribes to send history
 */
class LogStreamBroadcaster {
public:
    /**
     * @brief Construct broadcaster
     * @param ws WebSocket server instance
     */
    explicit LogStreamBroadcaster(AsyncWebSocket* ws);

    /**
     * @brief Destructor
     */
    ~LogStreamBroadcaster();

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
     * @brief Broadcast a log message to all subscribers
     * @param level Log level string ("E", "W", "I", "D")
     * @param tag Component tag (e.g., "WiFi", "LED")
     * @param message Log message content
     * @return Number of clients that received the message
     */
    size_t broadcast(const char* level, const char* tag, const char* message);

    /**
     * @brief Broadcast a pre-formatted log line
     * @param formattedLine Complete log line with ANSI codes
     * @return Number of clients that received the message
     */
    size_t broadcastLine(const char* formattedLine);

    /**
     * @brief Send backfill (recent log history) to a specific client
     * @param clientId Client to send backfill to
     */
    void sendBackfill(uint32_t clientId);

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
     * @brief Get number of messages in backfill buffer
     */
    size_t getBackfillCount() const;

private:
    AsyncWebSocket* m_ws;
    SubscriptionManager<8> m_subscribers;  // MAX_WS_CLIENTS = 8

    // Ring buffer for backfill
    char** m_ringBuffer;
    size_t m_ringHead;       // Next write position
    size_t m_ringCount;      // Current message count

    uint32_t m_lastBroadcast;

#if defined(ESP32)
    mutable portMUX_TYPE m_mux;
#endif

    /**
     * @brief Add message to ring buffer
     */
    void addToRingBuffer(const char* message);

    /**
     * @brief Get current time in milliseconds
     */
    uint32_t getTime() const;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos
