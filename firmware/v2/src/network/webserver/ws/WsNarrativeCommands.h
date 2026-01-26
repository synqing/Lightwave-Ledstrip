/**
 * @file WsNarrativeCommands.h
 * @brief WebSocket narrative command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register narrative-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsNarrativeCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

