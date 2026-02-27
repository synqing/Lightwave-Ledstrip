/**
 * @file WsPresetCommands.h
 * @brief WebSocket preset command handlers (stub)
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register preset-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsPresetCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
