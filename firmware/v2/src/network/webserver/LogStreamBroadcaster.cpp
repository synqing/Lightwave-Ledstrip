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
#include <esp_heap_caps.h>

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

    // Allocate ring buffer in PSRAM (25.6KB - too large for internal SRAM)
    m_ringBuffer = static_cast<char**>(
        heap_caps_calloc(LogStreamConfig::RING_BUFFER_SIZE, sizeof(char*), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (!m_ringBuffer) {
        // Fallback to internal SRAM if PSRAM unavailable
        m_ringBuffer = static_cast<char**>(
            heap_caps_calloc(LogStreamConfig::RING_BUFFER_SIZE, sizeof(char*), MALLOC_CAP_8BIT)
        );
    }
    if (m_ringBuffer) {
        for (size_t i = 0; i < LogStreamConfig::RING_BUFFER_SIZE; i++) {
            m_ringBuffer[i] = static_cast<char*>(
                heap_caps_calloc(LogStreamConfig::MAX_MESSAGE_LENGTH, sizeof(char), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
            );
            if (!m_ringBuffer[i]) {
                // Fallback to internal SRAM for this slot
                m_ringBuffer[i] = static_cast<char*>(
                    heap_caps_calloc(LogStreamConfig::MAX_MESSAGE_LENGTH, sizeof(char), MALLOC_CAP_8BIT)
                );
            }
        }
    }
}

LogStreamBroadcaster::~LogStreamBroadcaster() {
    if (m_ringBuffer) {
        for (size_t i = 0; i < LogStreamConfig::RING_BUFFER_SIZE; i++) {
            if (m_ringBuffer[i]) {
                heap_caps_free(m_ringBuffer[i]);
            }
        }
        heap_caps_free(m_ringBuffer);
        m_ringBuffer = nullptr;
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

    c->text("--- Log History ---");

    // Capture ring buffer state (only indices, not data)
    size_t capturedCount = 0;
    size_t capturedHead = 0;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    capturedCount = m_ringCount;
    capturedHead = m_ringHead;
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    if (capturedCount == 0 || !m_ringBuffer) {
        c->text("--- Live Logs ---");
        return;
    }

    size_t startIdx = 0;
    if (capturedCount >= LogStreamConfig::RING_BUFFER_SIZE) {
        startIdx = capturedHead;
    }

    // Single message buffer on stack (256 bytes - safe)
    char msgBuffer[LogStreamConfig::MAX_MESSAGE_LENGTH];

    for (size_t i = 0; i < capturedCount; i++) {
        size_t idx = (startIdx + i) % LogStreamConfig::RING_BUFFER_SIZE;

#if defined(ESP32)
        portENTER_CRITICAL(&m_mux);
#endif
        if (m_ringBuffer[idx]) {
            strncpy(msgBuffer, m_ringBuffer[idx], LogStreamConfig::MAX_MESSAGE_LENGTH - 1);
            msgBuffer[LogStreamConfig::MAX_MESSAGE_LENGTH - 1] = '\0';
        } else {
            msgBuffer[0] = '\0';
        }
#if defined(ESP32)
        portEXIT_CRITICAL(&m_mux);
#endif

        if (msgBuffer[0] != '\0') {
            c->text(msgBuffer);
        }

        if (c->status() != WS_CONNECTED) {
            return;
        }
    }

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
