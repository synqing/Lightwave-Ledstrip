/**
 * @file UdpStreamer.h
 * @brief UDP frame streamer for LED and audio data
 *
 * Replaces TCP-based WebSocket streaming for LED and audio frames to eliminate
 * TCP ACK timeout issues on weak WiFi. Sends pre-encoded binary frames via UDP
 * to up to 4 subscribers, with per-stream throttling and staggered sends.
 *
 * Frame sizes fit within WiFi MTU (1472 bytes): LED=966, Audio=464.
 */

#pragma once

#ifndef NATIVE_BUILD

#include <WiFiUdp.h>
#include <IPAddress.h>
#include <FastLED.h>
#include "LedFrameEncoder.h"
#include "AudioFrameEncoder.h"
#include "AudioStreamConfig.h"
#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC
#include "../../audio/contracts/ControlBus.h"
#include "../../audio/contracts/MusicalGrid.h"
#endif

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace lightwaveos {
namespace network {
namespace webserver {

// ============================================================================
// UDP Subscriber Record
// ============================================================================

struct UdpSubscriber {
    IPAddress ip;
    uint16_t port = 0;
    bool active = false;
    bool wantsLed = false;
    bool wantsAudio = false;
    uint8_t failStreak = 0;
    uint32_t suppressUntilMs = 0;
};

// ============================================================================
// UDP Streamer
// ============================================================================

/**
 * @brief Sends LED and audio frames via UDP to registered subscribers
 *
 * Designed as a fire-and-forget transport: no ACKs, no retransmits.
 * Subscribers register via WebSocket command; frames are sent at throttled
 * intervals with staggered delivery to avoid bursts.
 */
class UdpStreamer {
public:
    static constexpr size_t MAX_SUBSCRIBERS = 4;
    // Keep this conservative. A single bad tick can starve lwIP pbuf allocation and trigger ENOMEM.
    static constexpr uint8_t MAX_PACKETS_PER_TICK = 1;

    struct UdpStats {
        bool started = false;
        uint32_t ledAttempts = 0;
        uint32_t ledSuccess = 0;
        uint32_t ledFailures = 0;
        uint32_t audioAttempts = 0;
        uint32_t audioSuccess = 0;
        uint32_t audioFailures = 0;
        uint32_t lastFailureMs = 0;
        uint32_t cooldownUntilMs = 0;
        uint8_t consecutiveFailures = 0;
        uint8_t subscriberCount = 0;
        uint8_t suppressedCount = 0;
        uint32_t socketResets = 0;
        uint32_t lastSocketResetMs = 0;
    };

    UdpStreamer();
    ~UdpStreamer();

    /// Initialise UDP socket (call once during WebServer::begin)
    bool begin(uint16_t localPort = 0);

    /// Stop the UDP socket and clear all subscribers
    void stop();

    /// Service recovery actions (socket reset, logging) even when no frames are sent.
    void service();

    // -- Subscriber management (called from WS subscribe handlers) -----------

    /// Add a LED stream subscriber. Returns false if no slots available.
    bool addLedSubscriber(IPAddress ip, uint16_t port);

    /// Add an audio stream subscriber. Returns false if no slots available.
    bool addAudioSubscriber(IPAddress ip, uint16_t port);

    /// Remove all subscriptions for a given IP address
    void removeSubscriber(IPAddress ip);

    /// Remove all subscribers
    void removeAll();

    // -- Frame sending (called from WebServer::update) -----------------------

    /// Encode and send an LED frame to all LED subscribers.
    /// Handles throttling and staggering internally.
    void sendLedFrame(const CRGB* leds);

#if FEATURE_AUDIO_SYNC
    /// Encode and send an audio frame to all audio subscribers.
    /// Handles throttling and staggering internally.
    void sendAudioFrame(const audio::ControlBusFrame& frame,
                        const audio::MusicalGridSnapshot& grid);
#endif

    // -- Queries -------------------------------------------------------------

    bool hasLedSubscribers() const;
    bool hasAudioSubscribers() const;
    size_t getSubscriberCount() const;
    bool isStarted() const { return m_started; }
    void getStats(UdpStats& outStats) const;

private:
    WiFiUDP m_udp;
    UdpSubscriber m_subscribers[MAX_SUBSCRIBERS];

    // Pre-allocated frame buffers (no heap allocation on send path)
    uint8_t m_ledBuffer[LedStreamConfig::FRAME_SIZE];   // 966 bytes
#if FEATURE_AUDIO_SYNC
    uint8_t m_audioBuffer[AudioStreamConfig::FRAME_SIZE]; // 464 bytes
#endif

    // Throttle timestamps (millis)
    uint32_t m_lastLedSend = 0;
    uint32_t m_lastAudioSend = 0;

    // Send telemetry (attempts/success/failures)
    uint32_t m_ledAttempts = 0;
    uint32_t m_ledSuccess = 0;
    uint32_t m_ledFailures = 0;
    uint32_t m_audioAttempts = 0;
    uint32_t m_audioSuccess = 0;
    uint32_t m_audioFailures = 0;
    uint32_t m_lastFailureMs = 0;
    uint32_t m_lastStatsLogMs = 0;

    // Backoff control
    uint32_t m_cooldownUntilMs = 0;
    uint8_t m_consecutiveFailures = 0;
    uint16_t m_localPort = 0;
    uint32_t m_socketResets = 0;
    uint32_t m_lastSocketResetMs = 0;
    bool m_needsSocketReset = false;

    // Round-robin pointers for fair delivery
    uint8_t m_rrLedIndex = 0;
    uint8_t m_rrAudioIndex = 0;

    bool m_started = false;

#if defined(ESP32)
    mutable portMUX_TYPE m_mux;
#endif

    static constexpr uint32_t COOLDOWN_SHORT_MS = 1000;
    static constexpr uint32_t COOLDOWN_LONG_MS = 3000;
    static constexpr uint32_t SUBSCRIBER_SUPPRESS_SHORT_MS = 2000;
    static constexpr uint32_t SUBSCRIBER_SUPPRESS_LONG_MS = 5000;
    static constexpr uint32_t STATS_LOG_INTERVAL_MS = 5000;
    static constexpr uint8_t FAILURE_STREAK_SUPPRESS = 3;
    static constexpr uint8_t FAILURE_STREAK_LONG_SUPPRESS = 5;
    static constexpr uint8_t FAILURE_STREAK_SOCKET_RESET = 6;
    static constexpr uint8_t FAILURE_STREAK_DROP_ALL = 10;
    static constexpr uint32_t SOCKET_RESET_MIN_INTERVAL_MS = 15000;

    /// Find subscriber slot by IP, or first inactive slot if not found.
    /// Must be called under lock.
    int findSlot(IPAddress ip) const;
    int findFreeSlot() const;
    void resetStats();
    void recordSendResult(bool isLed, bool success);
    void updateCooldown(uint32_t nowMs, bool anyFailure, bool anySuccess);
    void maybeResetSocket(uint32_t nowMs);
    void updateSubscriberResult(uint8_t slot, bool success, uint32_t nowMs);
    bool sendPacket(const IPAddress& ip, uint16_t port, const uint8_t* buffer, size_t size);
    void maybeLogStats(uint32_t nowMs);
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // NATIVE_BUILD
