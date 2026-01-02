/**
 * @file WsBatchCommands.h
 * @brief WebSocket batch command handlers
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register batch-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsBatchCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

