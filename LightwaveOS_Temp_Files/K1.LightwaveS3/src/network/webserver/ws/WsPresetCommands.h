/**
 * @file WsPresetCommands.h
 * @brief WebSocket effect preset command handlers
 *
 * LightwaveOS v2 - WebSocket API
 *
 * Handles effect preset CRUD operations over WebSocket:
 * - effect.presets.list - List all saved presets
 * - effect.presets.get - Get preset details by ID
 * - effect.presets.save - Save current effect config as preset
 * - effect.presets.apply - Apply preset by ID
 * - effect.presets.delete - Delete preset by ID
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register effect preset WebSocket commands
 * @param ctx WebServer context
 */
void registerWsPresetCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
