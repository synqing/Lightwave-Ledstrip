/**
 * @file WsEffectsCommands.h
 * @brief WebSocket effects command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register effects-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsEffectsCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

