/**
 * @file WsEffectPresetCommands.h
 * @brief WebSocket effect preset command handlers
 *
 * LightwaveOS v2 - WebSocket API
 *
 * Commands for managing effect presets (single-effect snapshots):
 * - effectPresets.list   : List all saved effect presets
 * - effectPresets.get    : Get specific preset details
 * - effectPresets.saveCurrent : Save current effect state to slot
 * - effectPresets.load   : Load and apply a preset
 * - effectPresets.delete : Remove a preset
 *
 * @author LightwaveOS Team
 * @version 2.0.0
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
 *
 * Registers all effectPresets.* command handlers with the WsCommandRouter.
 * Commands use EffectPresetManager::instance() for NVS persistence.
 *
 * @param ctx WebServer context (provides renderer, actorSystem, ws broadcast)
 */
void registerWsEffectPresetCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
