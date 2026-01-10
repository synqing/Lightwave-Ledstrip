/**
 * @file WsFilesystemCommands.h
 * @brief WebSocket filesystem command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {
struct WebServerContext;  // Forward declaration from parent namespace
namespace ws {

void registerWsFilesystemCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
