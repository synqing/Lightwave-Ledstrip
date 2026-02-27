/**
 * @file WsStreamCommands.h
 * @brief WebSocket stream subscription command handlers
 */

#pragma once

#include "../../../config/features.h"

// Forward declarations for FFT stream stubs
class AsyncWebSocket;

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

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
