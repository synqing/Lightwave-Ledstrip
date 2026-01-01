/**
 * @file WsDebugCommands.h
 * @brief WebSocket debug command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register debug-related WebSocket commands
 * @param ctx WebServer context
 *
 * Commands:
 *   debug.audio.get - Get current verbosity settings
 *   debug.audio.set - Set verbosity level {verbosity: 0-5}
 */
void registerWsDebugCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
