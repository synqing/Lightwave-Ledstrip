/**
 * @file LedStreamBroadcaster.cpp
 * @brief LED stream broadcaster implementation
 */

#include "LedStreamBroadcaster.h"
#include "RateLimiter.h"  // For ITimeSource and ArduinoTimeSource
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

LedStreamBroadcaster::LedStreamBroadcaster(AsyncWebSocket* ws, size_t maxClients, ITimeSource* timeSource)
    : m_ws(ws)
    , m_timeSource(timeSource ? timeSource : nullptr)
    , m_defaultTimeSource(timeSource ? nullptr : new ArduinoTimeSource())
    , m_lastBroadcast(0)
{
    (void)maxClients;  // Unused parameter
    if (!m_timeSource) {
        m_timeSource = m_defaultTimeSource;
    }
#if defined(ESP32)
    m_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
    memset(m_frameBuffer, 0, sizeof(m_frameBuffer));
}

bool LedStreamBroadcaster::setSubscription(uint32_t clientId, bool subscribe) {
    bool success = false;
    bool wasPresent = false;
    
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    if (subscribe) {
        success = m_subscribers.add(clientId);
    } else {
        wasPresent = m_subscribers.remove(clientId);
        success = true;
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    
    return success;
}

bool LedStreamBroadcaster::hasSubscribers() const {
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

size_t LedStreamBroadcaster::broadcast(const CRGB* leds) {
    if (!hasSubscribers() || !m_ws || m_ws->count() == 0) return 0;
    
    // Throttle to target FPS
    uint32_t now = getTime();
    if (now - m_lastBroadcast < LedStreamConfig::FRAME_INTERVAL_MS) return 0;
    m_lastBroadcast = now;
    
    // Encode frame
    size_t encoded = LedFrameEncoder::encode(leds, m_frameBuffer, sizeof(m_frameBuffer));
    if (encoded == 0) return 0;
    
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
    
    // Send to subscribers and track disconnected clients
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
        c->binary(m_frameBuffer, LedStreamConfig::FRAME_SIZE);
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

void LedStreamBroadcaster::cleanupDisconnected() {
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

size_t LedStreamBroadcaster::getSubscriberCount() const {
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

uint32_t LedStreamBroadcaster::getTime() const {
    return m_timeSource->millis();
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

