/**
 * @file WsZonePresetCommands.h
 * @brief WebSocket zone preset command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register zone preset-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsZonePresetCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
