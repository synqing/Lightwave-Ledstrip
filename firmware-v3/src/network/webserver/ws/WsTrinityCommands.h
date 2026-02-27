/**
 * @file WsTrinityCommands.h
 * @brief WebSocket Trinity command handlers
 *
 * Handles trinity.beat, trinity.macro, trinity.segment, and trinity.sync commands
 * for offline ML analysis sync.
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register Trinity-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsTrinityCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
