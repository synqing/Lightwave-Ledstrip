/**
 * @file WsModifierCommands.h
 * @brief WebSocket effect modifier command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register effect modifier-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsModifierCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
