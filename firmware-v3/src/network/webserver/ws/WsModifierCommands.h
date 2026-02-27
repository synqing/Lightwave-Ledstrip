/**
 * @file WsModifierCommands.h
 * @brief WebSocket modifier command handlers
 *
 * Provides WebSocket commands for effect modifier management:
 * - modifiers.list - List active modifiers
 * - modifiers.add - Add a modifier to the stack
 * - modifiers.remove - Remove a modifier by type
 * - modifiers.clear - Clear all modifiers
 * - modifiers.update - Update modifier parameters
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../../../config/features.h"

#if FEATURE_WEB_SERVER

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register modifier-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsModifierCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
