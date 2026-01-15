/**
 * @file WsZonePresetCommands.h
 * @brief WebSocket zone preset command handlers
 *
 * LightwaveOS v2 - WebSocket API
 *
 * Handles zone preset CRUD operations over WebSocket:
 * - zonePresets.list - List all saved zone presets
 * - zonePresets.get - Get preset details by ID
 * - zonePresets.save - Save current zone config as preset
 * - zonePresets.apply - Apply preset by ID
 * - zonePresets.delete - Delete preset by ID
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register zone preset WebSocket commands
 * @param ctx WebServer context
 */
void registerWsZonePresetCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
