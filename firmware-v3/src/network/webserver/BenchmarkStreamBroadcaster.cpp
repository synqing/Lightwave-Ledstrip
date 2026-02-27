// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file BenchmarkStreamBroadcaster.cpp
 * @brief Benchmark stream broadcaster implementation
 */

#include "BenchmarkStreamBroadcaster.h"
#include "../../config/features.h"

#if FEATURE_AUDIO_BENCHMARK

#include "RateLimiter.h"  // For ITimeSource and ArduinoTimeSource
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

BenchmarkStreamBroadcaster::BenchmarkStreamBroadcaster(AsyncWebSocket* ws, ITimeSource* timeSource)
    : m_ws(ws)
    , m_timeSource(timeSource ? timeSource : nullptr)
    , m_defaultTimeSource(timeSource ? nullptr : new ArduinoTimeSource())
    , m_lastBroadcast(0)
    , m_streamingActive(false)
{
    if (!m_timeSource) {
        m_timeSource = m_defaultTimeSource;
    }
#if defined(ESP32)
    m_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
    memset(m_frameBuffer, 0, sizeof(m_frameBuffer));
}

BenchmarkStreamBroadcaster::~BenchmarkStreamBroadcaster() {
    delete m_defaultTimeSource;
}

bool BenchmarkStreamBroadcaster::setSubscription(uint32_t clientId, bool subscribe) {
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

bool BenchmarkStreamBroadcaster::hasSubscribers() const {
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

size_t BenchmarkStreamBroadcaster::broadcastCompact(const audio::AudioBenchmarkStats& stats) {
    if (!hasSubscribers() || !m_ws || m_ws->count() == 0) return 0;

    // Throttle to target FPS (10 Hz)
    uint32_t now = getTime();
    if (now - m_lastBroadcast < BenchmarkStreamConfig::FRAME_INTERVAL_MS) return 0;
    m_lastBroadcast = now;

    // Encode compact frame
    BenchmarkFrameEncoder::encodeCompact(stats, now, getFlags(), m_frameBuffer);

    // Get subscriber IDs (copy to avoid holding lock during send)
    uint32_t ids[BenchmarkStreamConfig::MAX_CLIENTS];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
    for (size_t i = 0; i < count && i < BenchmarkStreamConfig::MAX_CLIENTS; i++) {
        ids[i] = m_subscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    // Send to subscribers and track disconnected clients
    uint32_t toRemove[BenchmarkStreamConfig::MAX_CLIENTS];
    uint8_t removeCount = 0;
    size_t sentCount = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (!c || c->status() != WS_CONNECTED) {
            if (removeCount < BenchmarkStreamConfig::MAX_CLIENTS) {
                toRemove[removeCount++] = clientId;
            }
            continue;
        }
        c->binary(m_frameBuffer, BenchmarkStreamConfig::COMPACT_FRAME_SIZE);
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

size_t BenchmarkStreamBroadcaster::broadcastExtended(const audio::AudioBenchmarkStats& stats) {
    if (!hasSubscribers() || !m_ws || m_ws->count() == 0) return 0;

    uint32_t now = getTime();

    // Encode extended frame (no throttle check - called explicitly)
    BenchmarkFrameEncoder::encodeExtended(stats, now, getFlags(), m_frameBuffer);

    // Get subscriber IDs
    uint32_t ids[BenchmarkStreamConfig::MAX_CLIENTS];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
    for (size_t i = 0; i < count && i < BenchmarkStreamConfig::MAX_CLIENTS; i++) {
        ids[i] = m_subscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    size_t sentCount = 0;
    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (c && c->status() == WS_CONNECTED) {
            c->binary(m_frameBuffer, BenchmarkStreamConfig::EXTENDED_FRAME_SIZE);
            sentCount++;
        }
    }

    return sentCount;
}

void BenchmarkStreamBroadcaster::cleanupDisconnected() {
    if (!m_ws) return;

    uint32_t ids[BenchmarkStreamConfig::MAX_CLIENTS];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    count = m_subscribers.count();
    for (size_t i = 0; i < count && i < BenchmarkStreamConfig::MAX_CLIENTS; i++) {
        ids[i] = m_subscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    uint32_t toRemove[BenchmarkStreamConfig::MAX_CLIENTS];
    uint8_t removeCount = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (!c || c->status() != WS_CONNECTED) {
            if (removeCount < BenchmarkStreamConfig::MAX_CLIENTS) {
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

size_t BenchmarkStreamBroadcaster::getSubscriberCount() const {
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

uint32_t BenchmarkStreamBroadcaster::getTime() const {
    return m_timeSource->millis();
}

uint8_t BenchmarkStreamBroadcaster::getFlags() const {
    uint8_t flags = 0;
    if (m_streamingActive) {
        flags |= BenchmarkStreamConfig::FLAG_STREAMING_ACTIVE;
    }
    flags |= BenchmarkStreamConfig::FLAG_BENCHMARK_ENABLED;
    return flags;
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_BENCHMARK
