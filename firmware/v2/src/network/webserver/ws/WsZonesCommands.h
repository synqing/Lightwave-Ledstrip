/**
 * @file WsZonesCommands.h
 * @brief WebSocket zones command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register zones-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsZonesCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

