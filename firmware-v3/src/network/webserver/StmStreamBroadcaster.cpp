/**
 * @file StmStreamBroadcaster.cpp
 * @brief STM stream broadcaster implementation
 *
 * Broadcasts spectral-temporal modulation data to subscribed WebSocket clients.
 * Frame encoding packs floats as little-endian bytes via memcpy (no heap allocation).
 */

#include "StmStreamBroadcaster.h"

#if FEATURE_AUDIO_SYNC

#include "RateLimiter.h"  // For ITimeSource and ArduinoTimeSource
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

StmStreamBroadcaster::StmStreamBroadcaster(AsyncWebSocket* ws, size_t maxClients, ITimeSource* timeSource)
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

bool StmStreamBroadcaster::setSubscription(uint32_t clientId, bool subscribe) {
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

    (void)wasPresent;
    return success;
}

bool StmStreamBroadcaster::hasSubscribers() const {
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

size_t StmStreamBroadcaster::broadcast(const audio::ControlBusFrame& frame) {
    if (!hasSubscribers() || !m_ws || m_ws->count() == 0) return 0;

    // Throttle to target FPS
    uint32_t now = getTime();
    if (now - m_lastBroadcast < StmStreamConfig::FRAME_INTERVAL_MS) return 0;
    m_lastBroadcast = now;

    // Encode frame into pre-allocated buffer (no heap allocation)
    uint8_t* p = m_frameBuffer;
    *p++ = StmStreamConfig::MAGIC_BYTE;
    *p++ = frame.stmReady ? 1 : 0;

    // Pack spectral bins (7 floats)
    std::memcpy(p, frame.stmSpectral, sizeof(float) * StmStreamConfig::SPECTRAL_BINS);
    p += sizeof(float) * StmStreamConfig::SPECTRAL_BINS;

    // Pack temporal bands (16 floats)
    std::memcpy(p, frame.stmTemporal, sizeof(float) * StmStreamConfig::TEMPORAL_BANDS);
    p += sizeof(float) * StmStreamConfig::TEMPORAL_BANDS;

    // Pack scalar energies
    std::memcpy(p, &frame.stmSpectralEnergy, sizeof(float));
    p += sizeof(float);
    std::memcpy(p, &frame.stmTemporalEnergy, sizeof(float));
    p += sizeof(float);

    // Compute spectral centroid and dominant bin on-the-fly
    float centroidNum = 0.0f;
    float centroidDen = 0.0f;
    uint8_t dominantBin = 0;
    float maxVal = 0.0f;
    for (uint8_t i = 0; i < StmStreamConfig::SPECTRAL_BINS; ++i) {
        const float v = frame.stmSpectral[i];
        centroidNum += v * static_cast<float>(i);
        centroidDen += v;
        if (v > maxVal) {
            maxVal = v;
            dominantBin = i;
        }
    }
    float centroid = (centroidDen > 1e-6f) ? centroidNum / centroidDen : 0.0f;
    std::memcpy(p, &centroid, sizeof(float));
    p += sizeof(float);

    float dominantFloat = static_cast<float>(dominantBin);
    std::memcpy(p, &dominantFloat, sizeof(float));

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
        // Back-pressure: skip frames when client queue is full
        static constexpr size_t MAX_QUEUE_LEN_BEFORE_DROP = 1;
        if (c->queueIsFull() || c->queueLen() > MAX_QUEUE_LEN_BEFORE_DROP) {
            continue;
        }
        if (!c->canSend()) continue;
        c->binary(m_frameBuffer, StmStreamConfig::FRAME_SIZE);
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

void StmStreamBroadcaster::cleanupDisconnected() {
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

size_t StmStreamBroadcaster::getSubscriberCount() const {
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

uint32_t StmStreamBroadcaster::getTime() const {
    return m_timeSource->millis();
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
