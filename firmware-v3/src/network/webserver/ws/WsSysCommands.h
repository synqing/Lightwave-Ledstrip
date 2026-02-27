/**
 * @file WsSysCommands.h
 * @brief WebSocket system command handlers (sys.capabilities)
 *
 * Provides capability discovery for clients (e.g., PRISM dashboard) to
 * determine which features are available on this device.
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register system-related WebSocket commands
 * @param ctx WebServer context
 *
 * Registers:
 *   - sys.capabilities: Returns feature flags and capability information
 */
void registerWsSysCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
