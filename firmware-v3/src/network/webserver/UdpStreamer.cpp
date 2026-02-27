// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file UdpStreamer.cpp
 * @brief UDP frame streamer implementation
 *
 * Sends pre-encoded LED and audio frames over UDP to registered subscribers.
 * Uses staggered delivery and per-stream throttling to keep within WiFi
 * bandwidth limits without TCP ACK overhead.
 */

#include "UdpStreamer.h"

#ifndef NATIVE_BUILD

#define LW_LOG_TAG "UdpStream"
#include "../../utils/Log.h"

#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

// ============================================================================
// Construction / Lifecycle
// ============================================================================

UdpStreamer::UdpStreamer()
    : m_lastLedSend(0)
    , m_lastAudioSend(0)
    , m_started(false)
{
#if defined(ESP32)
    m_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
    memset(m_ledBuffer, 0, sizeof(m_ledBuffer));
#if FEATURE_AUDIO_SYNC
    memset(m_audioBuffer, 0, sizeof(m_audioBuffer));
#endif
    // Subscribers are value-initialised to inactive by default
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        m_subscribers[i] = UdpSubscriber{};
    }
    resetStats();
}

UdpStreamer::~UdpStreamer() {
    stop();
}

bool UdpStreamer::begin(uint16_t localPort) {
    if (m_started) return true;

    // Bind to a local port (0 = ephemeral port chosen by stack)
    if (!m_udp.begin(localPort)) {
        LW_LOGE("Failed to bind UDP socket on port %u", localPort);
        return false;
    }

    m_localPort = localPort;
    resetStats();
    m_started = true;
    LW_LOGI("UDP streamer started on port %u", localPort);
    return true;
}

void UdpStreamer::stop() {
    if (!m_started) return;

    removeAll();
    m_udp.stop();
    m_started = false;
    resetStats();
    LW_LOGI("UDP streamer stopped");
}

void UdpStreamer::service() {
    if (!m_started) return;
    uint32_t now = millis();
    maybeResetSocket(now);
    maybeLogStats(now);
}

// ============================================================================
// Subscriber Management
// ============================================================================

bool UdpStreamer::addLedSubscriber(IPAddress ip, uint16_t port) {
    bool ok = false;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif

    int idx = findSlot(ip);
    if (idx >= 0) {
        // Existing subscriber -- update flags
        m_subscribers[idx].port = port;
        m_subscribers[idx].active = true;
        m_subscribers[idx].wantsLed = true;
        m_subscribers[idx].failStreak = 0;
        m_subscribers[idx].suppressUntilMs = 0;
        ok = true;
    } else {
        idx = findFreeSlot();
        if (idx >= 0) {
            m_subscribers[idx].ip = ip;
            m_subscribers[idx].port = port;
            m_subscribers[idx].active = true;
            m_subscribers[idx].wantsLed = true;
            m_subscribers[idx].wantsAudio = false;
            m_subscribers[idx].failStreak = 0;
            m_subscribers[idx].suppressUntilMs = 0;
            ok = true;
        }
    }

#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    if (ok) {
        LW_LOGI("UDP: LED subscriber added %s:%u", ip.toString().c_str(), port);
    } else {
        LW_LOGW("UDP: No free subscriber slots for LED %s:%u", ip.toString().c_str(), port);
    }
    return ok;
}

bool UdpStreamer::addAudioSubscriber(IPAddress ip, uint16_t port) {
    bool ok = false;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif

    int idx = findSlot(ip);
    if (idx >= 0) {
        // Existing subscriber -- update flags
        m_subscribers[idx].port = port;
        m_subscribers[idx].active = true;
        m_subscribers[idx].wantsAudio = true;
        m_subscribers[idx].failStreak = 0;
        m_subscribers[idx].suppressUntilMs = 0;
        ok = true;
    } else {
        idx = findFreeSlot();
        if (idx >= 0) {
            m_subscribers[idx].ip = ip;
            m_subscribers[idx].port = port;
            m_subscribers[idx].active = true;
            m_subscribers[idx].wantsLed = false;
            m_subscribers[idx].wantsAudio = true;
            m_subscribers[idx].failStreak = 0;
            m_subscribers[idx].suppressUntilMs = 0;
            ok = true;
        }
    }

#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    if (ok) {
        LW_LOGI("UDP: Audio subscriber added %s:%u", ip.toString().c_str(), port);
    } else {
        LW_LOGW("UDP: No free subscriber slots for audio %s:%u", ip.toString().c_str(), port);
    }
    return ok;
}

void UdpStreamer::removeSubscriber(IPAddress ip) {
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif

    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active && m_subscribers[i].ip == ip) {
            m_subscribers[i].active = false;
            m_subscribers[i].wantsLed = false;
            m_subscribers[i].wantsAudio = false;
            m_subscribers[i].failStreak = 0;
            m_subscribers[i].suppressUntilMs = 0;
        }
    }

#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    LW_LOGI("UDP: Removed subscriber %s", ip.toString().c_str());
}

void UdpStreamer::removeAll() {
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif

    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        m_subscribers[i].active = false;
        m_subscribers[i].wantsLed = false;
        m_subscribers[i].wantsAudio = false;
        m_subscribers[i].failStreak = 0;
        m_subscribers[i].suppressUntilMs = 0;
    }

#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    LW_LOGD("UDP: All subscribers removed");
}

// ============================================================================
// Frame Sending
// ============================================================================

void UdpStreamer::sendLedFrame(const CRGB* leds) {
    if (!leds || !m_started || !hasLedSubscribers()) return;

    uint32_t now = millis();

    maybeResetSocket(now);

    if (now < m_cooldownUntilMs) {
        return;
    }

    // Throttle to configured frame interval
    if (now - m_lastLedSend < LedStreamConfig::FRAME_INTERVAL_MS) return;

    // Stagger: if audio was sent more recently, defer LED to next call
    // This avoids back-to-back UDP bursts within a single update tick
    if (m_lastAudioSend > m_lastLedSend && (now - m_lastAudioSend) < 10) return;

    // Encode frame into pre-allocated buffer
    size_t encoded = LedFrameEncoder::encode(leds, m_ledBuffer, sizeof(m_ledBuffer));
    if (encoded == 0) return;

    struct UdpTarget {
        IPAddress ip;
        uint16_t port;
        uint8_t slot;
    };

    // Copy subscriber info under lock, then send outside the critical section
    UdpTarget targets[MAX_SUBSCRIBERS];
    size_t targetCount = 0;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active && m_subscribers[i].wantsLed) {
            if (m_subscribers[i].suppressUntilMs > now) {
                continue;
            }
            targets[targetCount++] = UdpTarget{m_subscribers[i].ip, m_subscribers[i].port, static_cast<uint8_t>(i)};
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    if (targetCount == 0) return;

    size_t startIndex = m_rrLedIndex % targetCount;
    size_t sendCount = 0;
    bool anyFailure = false;
    bool anySuccess = false;

    for (size_t i = 0; i < targetCount && sendCount < MAX_PACKETS_PER_TICK; i++) {
        size_t idx = (startIndex + i) % targetCount;
        bool ok = sendPacket(targets[idx].ip, targets[idx].port, m_ledBuffer, encoded);
        recordSendResult(true, ok);
        updateSubscriberResult(targets[idx].slot, ok, now);
        anyFailure = anyFailure || !ok;
        anySuccess = anySuccess || ok;
        sendCount++;
    }

    if (sendCount > 0) {
        m_lastLedSend = now;
        m_rrLedIndex = static_cast<uint8_t>((startIndex + sendCount) % targetCount);
    }

    updateCooldown(now, anyFailure, anySuccess);
    maybeResetSocket(now);
    maybeLogStats(now);
}

#if FEATURE_AUDIO_SYNC

void UdpStreamer::sendAudioFrame(const audio::ControlBusFrame& frame,
                                 const audio::MusicalGridSnapshot& grid) {
    if (!m_started || !hasAudioSubscribers()) return;

    uint32_t now = millis();

    maybeResetSocket(now);

    if (now < m_cooldownUntilMs) {
        return;
    }

    // Throttle to configured frame interval
    if (now - m_lastAudioSend < AudioStreamConfig::FRAME_INTERVAL_MS) return;

    // Stagger: if LED was sent more recently, defer audio to next call
    if (m_lastLedSend > m_lastAudioSend && (now - m_lastLedSend) < 10) return;

    // Encode frame into pre-allocated buffer
    size_t encoded = AudioFrameEncoder::encode(frame, grid, now,
                                               m_audioBuffer, sizeof(m_audioBuffer));
    if (encoded == 0) return;

    struct UdpTarget {
        IPAddress ip;
        uint16_t port;
        uint8_t slot;
    };

    // Copy subscriber info under lock, then send outside the critical section
    UdpTarget targets[MAX_SUBSCRIBERS];
    size_t targetCount = 0;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active && m_subscribers[i].wantsAudio) {
            if (m_subscribers[i].suppressUntilMs > now) {
                continue;
            }
            targets[targetCount++] = UdpTarget{m_subscribers[i].ip, m_subscribers[i].port, static_cast<uint8_t>(i)};
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif

    if (targetCount == 0) return;

    size_t startIndex = m_rrAudioIndex % targetCount;
    size_t sendCount = 0;
    bool anyFailure = false;
    bool anySuccess = false;

    for (size_t i = 0; i < targetCount && sendCount < MAX_PACKETS_PER_TICK; i++) {
        size_t idx = (startIndex + i) % targetCount;
        bool ok = sendPacket(targets[idx].ip, targets[idx].port, m_audioBuffer, encoded);
        recordSendResult(false, ok);
        updateSubscriberResult(targets[idx].slot, ok, now);
        anyFailure = anyFailure || !ok;
        anySuccess = anySuccess || ok;
        sendCount++;
    }

    if (sendCount > 0) {
        m_lastAudioSend = now;
        m_rrAudioIndex = static_cast<uint8_t>((startIndex + sendCount) % targetCount);
    }

    updateCooldown(now, anyFailure, anySuccess);
    maybeResetSocket(now);
    maybeLogStats(now);
}

#endif // FEATURE_AUDIO_SYNC

// ============================================================================
// Queries
// ============================================================================

bool UdpStreamer::hasLedSubscribers() const {
    bool has = false;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active && m_subscribers[i].wantsLed) {
            has = true;
            break;
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    return has;
}

bool UdpStreamer::hasAudioSubscribers() const {
    bool has = false;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active && m_subscribers[i].wantsAudio) {
            has = true;
            break;
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    return has;
}

size_t UdpStreamer::getSubscriberCount() const {
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active) {
            count++;
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    return count;
}

void UdpStreamer::getStats(UdpStats& outStats) const {
    outStats.started = m_started;
    outStats.ledAttempts = m_ledAttempts;
    outStats.ledSuccess = m_ledSuccess;
    outStats.ledFailures = m_ledFailures;
    outStats.audioAttempts = m_audioAttempts;
    outStats.audioSuccess = m_audioSuccess;
    outStats.audioFailures = m_audioFailures;
    outStats.lastFailureMs = m_lastFailureMs;
    outStats.cooldownUntilMs = m_cooldownUntilMs;
    outStats.consecutiveFailures = m_consecutiveFailures;
    outStats.socketResets = m_socketResets;
    outStats.lastSocketResetMs = m_lastSocketResetMs;

    uint8_t subscriberCount = 0;
    uint8_t suppressedCount = 0;
    uint32_t nowMs = millis();
#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active) {
            subscriberCount++;
            if (m_subscribers[i].suppressUntilMs > nowMs) {
                suppressedCount++;
            }
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
    outStats.subscriberCount = subscriberCount;
    outStats.suppressedCount = suppressedCount;
}

// ============================================================================
// Internal Helpers (must be called under lock)
// ============================================================================

int UdpStreamer::findSlot(IPAddress ip) const {
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (m_subscribers[i].active && m_subscribers[i].ip == ip) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int UdpStreamer::findFreeSlot() const {
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (!m_subscribers[i].active) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void UdpStreamer::resetStats() {
    m_lastLedSend = 0;
    m_lastAudioSend = 0;
    m_ledAttempts = 0;
    m_ledSuccess = 0;
    m_ledFailures = 0;
    m_audioAttempts = 0;
    m_audioSuccess = 0;
    m_audioFailures = 0;
    m_lastFailureMs = 0;
    m_lastStatsLogMs = 0;
    m_cooldownUntilMs = 0;
    m_consecutiveFailures = 0;
    m_rrLedIndex = 0;
    m_rrAudioIndex = 0;
    m_socketResets = 0;
    m_lastSocketResetMs = 0;
    m_needsSocketReset = false;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    for (size_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        m_subscribers[i].failStreak = 0;
        m_subscribers[i].suppressUntilMs = 0;
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
}

void UdpStreamer::recordSendResult(bool isLed, bool success) {
    if (isLed) {
        m_ledAttempts++;
        if (success) {
            m_ledSuccess++;
        } else {
            m_ledFailures++;
        }
    } else {
        m_audioAttempts++;
        if (success) {
            m_audioSuccess++;
        } else {
            m_audioFailures++;
        }
    }
}

void UdpStreamer::updateCooldown(uint32_t nowMs, bool anyFailure, bool anySuccess) {
    if (anyFailure) {
        m_lastFailureMs = nowMs;
        if (m_consecutiveFailures < 255) {
            m_consecutiveFailures++;
        }
        uint32_t cooldown = (m_consecutiveFailures >= FAILURE_STREAK_LONG_SUPPRESS)
            ? COOLDOWN_LONG_MS
            : COOLDOWN_SHORT_MS;
        m_cooldownUntilMs = nowMs + cooldown;

        // Circuit breaker: schedule a socket reset after a short streak of failures.
        if (m_consecutiveFailures >= FAILURE_STREAK_SOCKET_RESET) {
            m_needsSocketReset = true;
        }

        // Hard breaker: persistent failure means the best recovery is to drop subscribers and let the
        // client re-subscribe (or fall back to WS). This avoids indefinite error storms.
        if (m_consecutiveFailures >= FAILURE_STREAK_DROP_ALL) {
            LW_LOGW("UDP: dropping all subscribers after %u consecutive failures",
                    static_cast<unsigned>(m_consecutiveFailures));
            removeAll();
            m_needsSocketReset = true;
            m_cooldownUntilMs = nowMs + SUBSCRIBER_SUPPRESS_LONG_MS;
        }
        return;
    }

    if (anySuccess && m_consecutiveFailures > 0) {
        m_consecutiveFailures = 0;
        m_cooldownUntilMs = 0;
        m_needsSocketReset = false;
    }
}

void UdpStreamer::maybeResetSocket(uint32_t nowMs) {
    if (!m_started) return;
    if (!m_needsSocketReset) return;
    if (m_lastSocketResetMs != 0 && (nowMs - m_lastSocketResetMs) < SOCKET_RESET_MIN_INTERVAL_MS) {
        return;
    }

    // Reset the UDP socket in-place. This clears out stuck lwIP state and is significantly cheaper
    // than rebooting WiFi. Subscribers are retained unless the hard breaker already dropped them.
    m_udp.stop();
    if (!m_udp.begin(m_localPort)) {
        LW_LOGW("UDP: socket reset failed (begin failed)");
        m_lastSocketResetMs = nowMs;
        m_socketResets++;
        return;
    }

    m_lastSocketResetMs = nowMs;
    m_socketResets++;
    m_needsSocketReset = false;
    // Give lwIP a moment to breathe after reset.
    m_cooldownUntilMs = nowMs + COOLDOWN_SHORT_MS;
    LW_LOGW("UDP: socket reset performed (count=%lu)", static_cast<unsigned long>(m_socketResets));
}

void UdpStreamer::updateSubscriberResult(uint8_t slot, bool success, uint32_t nowMs) {
    if (slot >= MAX_SUBSCRIBERS) return;

#if defined(ESP32)
    portENTER_CRITICAL(&m_mux);
#endif
    if (!m_subscribers[slot].active) {
#if defined(ESP32)
        portEXIT_CRITICAL(&m_mux);
#endif
        return;
    }

    if (success) {
        m_subscribers[slot].failStreak = 0;
        m_subscribers[slot].suppressUntilMs = 0;
    } else {
        if (m_subscribers[slot].failStreak < 255) {
            m_subscribers[slot].failStreak++;
        }
        if (m_subscribers[slot].failStreak >= FAILURE_STREAK_SUPPRESS) {
            uint32_t suppressFor = (m_subscribers[slot].failStreak >= FAILURE_STREAK_LONG_SUPPRESS)
                ? SUBSCRIBER_SUPPRESS_LONG_MS
                : SUBSCRIBER_SUPPRESS_SHORT_MS;
            uint32_t targetUntil = nowMs + suppressFor;
            if (m_subscribers[slot].suppressUntilMs < targetUntil) {
                m_subscribers[slot].suppressUntilMs = targetUntil;
            }
        }
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_mux);
#endif
}

bool UdpStreamer::sendPacket(const IPAddress& ip, uint16_t port, const uint8_t* buffer, size_t size) {
    if (!buffer || size == 0) return false;

    int beginOk = m_udp.beginPacket(ip, port);
    if (!beginOk) return false;

    size_t written = m_udp.write(buffer, size);
    int endOk = m_udp.endPacket();
    if (written != size) return false;
    return endOk == 1;
}

void UdpStreamer::maybeLogStats(uint32_t nowMs) {
    if (m_ledFailures == 0 && m_audioFailures == 0) return;
    if (nowMs - m_lastStatsLogMs < STATS_LOG_INTERVAL_MS) return;

    m_lastStatsLogMs = nowMs;
    uint32_t cooldownRemaining = (m_cooldownUntilMs > nowMs) ? (m_cooldownUntilMs - nowMs) : 0;
    uint32_t lastFailAgo = (m_lastFailureMs > 0) ? (nowMs - m_lastFailureMs) : 0;

    LW_LOGW(
        "UDP stats: led a/s/f=%lu/%lu/%lu audio a/s/f=%lu/%lu/%lu consecFail=%u cooldown=%lu ms lastFail=%lu ms ago",
        static_cast<unsigned long>(m_ledAttempts),
        static_cast<unsigned long>(m_ledSuccess),
        static_cast<unsigned long>(m_ledFailures),
        static_cast<unsigned long>(m_audioAttempts),
        static_cast<unsigned long>(m_audioSuccess),
        static_cast<unsigned long>(m_audioFailures),
        static_cast<unsigned>(m_consecutiveFailures),
        static_cast<unsigned long>(cooldownRemaining),
        static_cast<unsigned long>(lastFailAgo)
    );
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // NATIVE_BUILD
