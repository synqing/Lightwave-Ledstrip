/**
 * @file AudioStreamBroadcaster.cpp
 * @brief Audio stream broadcaster implementation
 */

#include "AudioStreamBroadcaster.h"
#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC

#include "RateLimiter.h"  // For ITimeSource and ArduinoTimeSource
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

AudioStreamBroadcaster::AudioStreamBroadcaster(AsyncWebSocket* ws, ITimeSource* timeSource)
    : m_ws(ws)
    , m_timeSource(timeSource ? timeSource : nullptr)
    , m_defaultTimeSource(timeSource ? nullptr : new ArduinoTimeSource())
    , m_lastBroadcast(0)
{
    if (!m_timeSource) {
        m_timeSource = m_defaultTimeSource;
    }
#if defined(ESP32)
    m_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
    memset(m_frameBuffer, 0, sizeof(m_frameBuffer));
}

AudioStreamBroadcaster::~AudioStreamBroadcaster() {
    delete m_defaultTimeSource;
}

bool AudioStreamBroadcaster::setSubscription(uint32_t clientId, bool subscribe) {
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

    return success;
}

bool AudioStreamBroadcaster::hasSubscribers() const {
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

size_t AudioStreamBroadcaster::broadcast(const audio::ControlBusFrame& frame, const audio::MusicalGridSnapshot& grid) {
    if (!hasSubscribers() || !m_ws || m_ws->count() == 0) return 0;

    // Throttle to target FPS
    uint32_t now = getTime();
    if (now - m_lastBroadcast < AudioStreamConfig::FRAME_INTERVAL_MS) return 0;
    m_lastBroadcast = now;

    // Encode frame with musical grid data
    size_t encoded = AudioFrameEncoder::encode(frame, grid, now, m_frameBuffer, sizeof(m_frameBuffer));
    if (encoded == 0) return 0;

    // Get subscriber IDs (copy to avoid holding lock during send)
    uint32_t ids[AudioStreamConfig::MAX_CLIENTS];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
    for (size_t i = 0; i < count && i < AudioStreamConfig::MAX_CLIENTS; i++) {
        ids[i] = m_subscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    // Send to subscribers and track disconnected clients
    uint32_t toRemove[AudioStreamConfig::MAX_CLIENTS];
    uint8_t removeCount = 0;
    size_t sentCount = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (!c || c->status() != WS_CONNECTED) {
            if (removeCount < AudioStreamConfig::MAX_CLIENTS) {
                toRemove[removeCount++] = clientId;
            }
            continue;
        }
        // Back-pressure:
        // - canSend() alone is not sufficient because AsyncWebSocket may still accept messages into its queue,
        //   and queued WS messages consume internal heap (SRAM).
        // - Check queueLen() before calling binary() to avoid allocating a new WS message buffer unnecessarily.
        static constexpr size_t MAX_QUEUE_LEN_BEFORE_DROP = 1;
        if (c->queueIsFull() || c->queueLen() > MAX_QUEUE_LEN_BEFORE_DROP) {
            continue;
        }
        if (!c->canSend()) continue;
        c->binary(m_frameBuffer, AudioStreamConfig::FRAME_SIZE);
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

void AudioStreamBroadcaster::cleanupDisconnected() {
    if (!m_ws) return;

    uint32_t ids[AudioStreamConfig::MAX_CLIENTS];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
    for (size_t i = 0; i < count && i < AudioStreamConfig::MAX_CLIENTS; i++) {
        ids[i] = m_subscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    uint32_t toRemove[AudioStreamConfig::MAX_CLIENTS];
    uint8_t removeCount = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (!c || c->status() != WS_CONNECTED) {
            if (removeCount < AudioStreamConfig::MAX_CLIENTS) {
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

size_t AudioStreamBroadcaster::getSubscriberCount() const {
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

uint32_t AudioStreamBroadcaster::getTime() const {
    return m_timeSource->millis();
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
