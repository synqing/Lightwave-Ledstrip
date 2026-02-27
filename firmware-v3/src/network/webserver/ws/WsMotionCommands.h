/**
 * @file WsMotionCommands.h
 * @brief WebSocket motion command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register motion-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsMotionCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

