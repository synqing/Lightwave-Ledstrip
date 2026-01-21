/**
 * @file WsDeviceCommands.h
 * @brief WebSocket device command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register device-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsDeviceCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

