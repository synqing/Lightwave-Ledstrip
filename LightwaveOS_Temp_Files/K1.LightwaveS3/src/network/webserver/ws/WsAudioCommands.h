/**
 * @file WsAudioCommands.h
 * @brief WebSocket audio command handlers
 */

#pragma once

#include "../../../config/features.h"

// Forward declarations
class AsyncWebSocket;
class AsyncWebSocketClient;

namespace lightwaveos {
namespace audio {
struct ControlBusFrame;
}

namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register audio-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsAudioCommands(const WebServerContext& ctx);

#if FEATURE_AUDIO_SYNC
/**
 * @brief FFT WebSocket subscription management
 */
bool setFftStreamSubscription(AsyncWebSocketClient* client, bool subscribe);
bool hasFftStreamSubscribers();
void broadcastFftFrame(const audio::ControlBusFrame& frame, AsyncWebSocket* ws);
#endif

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

