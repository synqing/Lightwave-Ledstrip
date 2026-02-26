/**
 * @file WsStreamCommands.h
 * @brief WebSocket stream subscription command handlers
 */

#pragma once

#include "../../../config/features.h"
#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

// Forward declarations for FFT stream stubs
class AsyncWebSocket;
class AsyncWebSocketClient;

#if FEATURE_AUDIO_SYNC
namespace lightwaveos { namespace audio { struct ControlBusFrame; } }
#endif

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register stream subscription-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsStreamCommands(const webserver::WebServerContext& ctx);

struct RenderStreamStatusSnapshot {
    bool active = false;
    String sessionId;
    uint32_t ownerWsClientId = 0;
    uint32_t targetFps = 120;
    uint32_t staleTimeoutMs = 750;
    uint8_t frameContractVersion = 1;
    uint8_t pixelFormat = 1;
    uint16_t ledCount = 320;
    uint16_t headerBytes = 16;
    uint16_t payloadBytes = 960;
    uint16_t maxPayloadBytes = 960;
    uint8_t mailboxDepth = 2;
    uint32_t lastFrameSeq = 0;
    uint32_t lastFrameRxMs = 0;
    uint32_t framesRx = 0;
    uint32_t framesRendered = 0;
    uint32_t framesDroppedMailbox = 0;
    uint32_t framesInvalid = 0;
    uint32_t framesBlockedLease = 0;
    uint32_t staleTimeouts = 0;
};

/**
 * @brief Handle a binary render frame payload.
 *
 * Called by WsGateway after frame fragmentation reassembly.
 */
bool handleRenderStreamBinaryFrame(AsyncWebSocketClient* client, const uint8_t* payload, size_t len);

/**
 * @brief Notify stream session logic of WS disconnection.
 */
void handleRenderStreamClientDisconnect(uint32_t clientId);

/**
 * @brief Periodic service hook to sync lease/session/render state.
 */
void serviceRenderStreamState();

/**
 * @brief Snapshot render stream state/counters for REST/WS status payloads.
 */
RenderStreamStatusSnapshot getRenderStreamStatusSnapshot();

// ============================================================================
// FFT Stream Stubs
// TODO: Implement full FFT streaming if/when required
// These stubs exist because WebServer::broadcastFftFrame() references them
// but the feature is not yet fully implemented.
// ============================================================================

/**
 * @brief Check if any WebSocket clients are subscribed to FFT stream
 * @return Always false (stub - FFT streaming not yet implemented)
 */
inline bool hasFftStreamSubscribers() {
    return false;  // No subscribers until feature is implemented
}

#if FEATURE_AUDIO_SYNC
/**
 * @brief Broadcast FFT frame to subscribed WebSocket clients
 * @param frame Audio control bus frame containing FFT data
 * @param ws WebSocket instance
 *
 * Stub implementation - does nothing until FFT streaming is implemented.
 */
inline void broadcastFftFrame(const audio::ControlBusFrame& /* frame */,
                               AsyncWebSocket* /* ws */) {
    // Stub - FFT streaming not yet implemented
    // When implemented, this should:
    // 1. Check subscriber table for FFT stream subscriptions
    // 2. Throttle to 31 Hz to avoid overwhelming clients
    // 3. Serialize 64 FFT bins to JSON
    // 4. Broadcast to subscribed clients
}
#endif

// ============================================================================
// Beat Event Subscribers
// ============================================================================

/**
 * @brief Check if any client has subscribed to beat events
 *
 * beat.event broadcasts use textAll() which floods slow SoftAP clients.
 * Gating behind a subscriber check means Tab5/iOS (which never subscribe)
 * don't receive unwanted beat traffic.
 */
bool hasBeatEventSubscribers();

/**
 * @brief Remove a disconnected client from beat subscriber list
 */
void removeBeatSubscriber(uint32_t clientId);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
