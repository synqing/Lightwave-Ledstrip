/**
 * @file WsAudioCommands.h
 * @brief WebSocket audio command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register audio-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsAudioCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

