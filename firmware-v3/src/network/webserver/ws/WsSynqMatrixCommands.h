/**
 * @file WsSynqMatrixCommands.h
 * @brief WebSocket commands for runtime-only synq-matrix director controls.
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

struct WebServerContext;

namespace ws {

void registerWsSynqMatrixCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
