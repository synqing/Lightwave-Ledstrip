/**
 * @file WsRenderCommands.h
 * @brief WebSocket render-output command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

struct WebServerContext;

namespace ws {

void registerWsRenderCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
