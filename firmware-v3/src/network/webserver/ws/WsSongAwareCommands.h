/**
 * @file WsSongAwareCommands.h
 * @brief WebSocket commands for runtime-only song-aware director controls.
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

struct WebServerContext;

namespace ws {

void registerWsSongAwareCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
