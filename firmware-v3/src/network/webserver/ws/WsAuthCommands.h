/**
 * @file WsAuthCommands.h
 * @brief WebSocket API key authentication command handlers
 *
 * Provides WebSocket commands for API key management:
 * - auth.status - Returns authentication status (public)
 * - auth.rotate - Generate new API key (requires authenticated client)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../../../config/features.h"

#if FEATURE_WEB_SERVER && FEATURE_API_AUTH

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register authentication-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsAuthCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_API_AUTH
