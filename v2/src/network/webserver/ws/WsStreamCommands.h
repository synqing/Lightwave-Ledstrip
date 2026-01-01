/**
 * @file WsStreamCommands.h
 * @brief WebSocket stream subscription command handlers
 */

#pragma once

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
void registerWsStreamCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

