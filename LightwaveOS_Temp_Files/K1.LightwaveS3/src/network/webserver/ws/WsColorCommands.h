/**
 * @file WsColorCommands.h
 * @brief WebSocket color command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register color-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsColorCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

