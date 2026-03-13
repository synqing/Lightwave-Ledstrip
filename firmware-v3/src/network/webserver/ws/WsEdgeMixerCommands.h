/**
 * @file WsEdgeMixerCommands.h
 * @brief WebSocket edge mixer command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register edge mixer WebSocket commands
 * @param ctx WebServer context
 */
void registerWsEdgeMixerCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
