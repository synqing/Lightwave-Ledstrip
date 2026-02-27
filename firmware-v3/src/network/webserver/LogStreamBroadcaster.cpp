// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LogStreamBroadcaster.cpp
 * @brief Log stream broadcaster implementation
 *
 * Enables wireless serial monitoring by streaming log messages to WebSocket clients.
 */

#include "LogStreamBroadcaster.h"
#include <Arduino.h>
#include <cstring>
#include <cstdio>

namespace lightwaveos {
namespace network {
namespace webserver {

LogStreamBroadcaster::LogStreamBroadcaster(AsyncWebSocket* ws)
    : m_ws(ws)
    , m_ringHead(0)
    , m_ringCount(0)
    , m_lastBroadcast(0)
{
#if defined(ESP32)
    m_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

    // Allocate ring buffer
    m_ringBuffer = new char*[LogStreamConfig::RING_BUFFER_SIZE];
    for (size_t i = 0; i < LogStreamConfig::RING_BUFFER_SIZE; i++) {
        m_ringBuffer[i] = new char[LogStreamConfig::MAX_MESSAGE_LENGTH];
        m_ringBuffer[i][0] = '\0';
    }
}

LogStreamBroadcaster::~LogStreamBroadcaster() {
    if (m_ringBuffer) {
        for (size_t i = 0; i < LogStreamConfig::RING_BUFFER_SIZE; i++) {
            delete[] m_ringBuffer[i];
        }
        delete[] m_ringBuffer;
    }
}

bool LogStreamBroadcaster::setSubscription(uint32_t clientId, bool subscribe) {
    bool success = false;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    if (subscribe) {
        success = m_subscribers.add(clientId);
    } else {
        m_subscribers.remove(clientId);
        success = true;
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    // Send backfill to new subscriber
    if (subscribe && success) {
        sendBackfill(clientId);
    }

    return success;
}

bool LogStreamBroadcaster::hasSubscribers() const {
    bool has = false;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    has = m_subscribers.count() > 0;
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    return has;
}

size_t LogStreamBroadcaster::broadcast(const char* level, const char* tag, const char* message) {
    // Format message: [LEVEL][TAG] message
    char formatted[LogStreamConfig::MAX_MESSAGE_LENGTH];
    snprintf(formatted, sizeof(formatted), "[%s][%s] %s", level, tag, message);
    return broadcastLine(formatted);
}

size_t LogStreamBroadcaster::broadcastLine(const char* formattedLine) {
    if (!formattedLine || formattedLine[0] == '\0') return 0;
    if (!m_ws || m_ws->count() == 0) return 0;

    // Add to ring buffer (even if no subscribers - for future backfill)
    addToRingBuffer(formattedLine);

    if (!hasSubscribers()) return 0;

    // Optional: throttle to prevent flooding
    uint32_t now = getTime();
    if (now - m_lastBroadcast < LogStreamConfig::THROTTLE_MS) {
        // Still add to buffer but don't send yet
        // For logs, we actually want to send immediately, so skip throttling
        // return 0;
    }
    m_lastBroadcast = now;

    // Get subscriber IDs (copy to avoid holding lock during send)
    uint32_t ids[8];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
    for (size_t i = 0; i < count && i < 8; i++) {
        ids[i] = m_subscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    // Send to subscribers
    uint32_t toRemove[8];
    uint8_t removeCount = 0;
    size_t sentCount = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (!c || c->status() != WS_CONNECTED) {
            if (removeCount < 8) {
                toRemove[removeCount++] = clientId;
            }
            continue;
        }
        c->text(formattedLine);
        sentCount++;
    }

    // Clean up disconnected clients
    if (removeCount > 0) {
#if defined(ESP32)
        portENTER_CRITICAL(&m_mux);
#endif
        for (uint8_t r = 0; r < removeCount; r++) {
            m_subscribers.remove(toRemove[r]);
        }
#if defined(ESP32)
        portEXIT_CRITICAL(&m_mux);
#endif
    }

    return sentCount;
}

void LogStreamBroadcaster::sendBackfill(uint32_t clientId) {
    if (!m_ws) return;

    AsyncWebSocketClient* c = m_ws->client(clientId);
    if (!c || c->status() != WS_CONNECTED) return;

    // Send header indicating backfill start
    c->text("--- Log History ---");

    // Copy ring buffer state to avoid long critical section
    char tempBuffer[LogStreamConfig::RING_BUFFER_SIZE][LogStreamConfig::MAX_MESSAGE_LENGTH];
    size_t tempCount = 0;
    size_t startIdx = 0;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    tempCount = m_ringCount;
    if (tempCount > 0) {
        // Calculate start index for oldest message
        if (tempCount >= LogStreamConfig::RING_BUFFER_SIZE) {
            startIdx = m_ringHead; // Head points to oldest when full
        } else {
            startIdx = 0;
        }

        // Copy messages in order (oldest to newest)
        for (size_t i = 0; i < tempCount; i++) {
            size_t idx = (startIdx + i) % LogStreamConfig::RING_BUFFER_SIZE;
            strncpy(tempBuffer[i], m_ringBuffer[idx], LogStreamConfig::MAX_MESSAGE_LENGTH - 1);
            tempBuffer[i][LogStreamConfig::MAX_MESSAGE_LENGTH - 1] = '\0';
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    // Send backfill messages outside critical section
    for (size_t i = 0; i < tempCount; i++) {
        if (tempBuffer[i][0] != '\0') {
            c->text(tempBuffer[i]);
        }
    }

    // Send footer indicating live logs start
    c->text("--- Live Logs ---");
}

void LogStreamBroadcaster::cleanupDisconnected() {
    if (!m_ws) return;

    uint32_t ids[8];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
    for (size_t i = 0; i < count && i < 8; i++) {
        ids[i] = m_subscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    uint32_t toRemove[8];
    uint8_t removeCount = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (!c || c->status() != WS_CONNECTED) {
            if (removeCount < 8) {
                toRemove[removeCount++] = clientId;
            }
        }
    }

    if (removeCount > 0) {
#if defined(ESP32)
        portENTER_CRITICAL(&m_mux);
#endif
        for (uint8_t r = 0; r < removeCount; r++) {
            m_subscribers.remove(toRemove[r]);
        }
#if defined(ESP32)
        portEXIT_CRITICAL(&m_mux);
#endif
    }
}

size_t LogStreamBroadcaster::getSubscriberCount() const {
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    return count;
}

size_t LogStreamBroadcaster::getBackfillCount() const {
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_ringCount;
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    return count;
}

void LogStreamBroadcaster::addToRingBuffer(const char* message) {
    if (!message || !m_ringBuffer) return;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif

    // Copy message to current head position
    strncpy(m_ringBuffer[m_ringHead], message, LogStreamConfig::MAX_MESSAGE_LENGTH - 1);
    m_ringBuffer[m_ringHead][LogStreamConfig::MAX_MESSAGE_LENGTH - 1] = '\0';

    // Advance head
    m_ringHead = (m_ringHead + 1) % LogStreamConfig::RING_BUFFER_SIZE;

    // Update count (max out at buffer size)
    if (m_ringCount < LogStreamConfig::RING_BUFFER_SIZE) {
        m_ringCount++;
    }

#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
}

uint32_t LogStreamBroadcaster::getTime() const {
    return millis();
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos
