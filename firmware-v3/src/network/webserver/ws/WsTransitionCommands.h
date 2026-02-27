/**
 * @file WsTransitionCommands.h
 * @brief WebSocket transition command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register transition-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsTransitionCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

