// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsPluginCommands.h
 * @brief WebSocket plugin command handlers for the Plugin Subsystem
 *
 * Provides real-time plugin functionality via WebSocket:
 * - Plugin list
 * - Plugin statistics with reload status
 * - Manifest reload (atomic)
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
 * @brief Register plugin-related WebSocket commands
 * @param ctx WebServer context
 *
 * Commands:
 *   plugins.list   - List registered effects from plugin manager
 *   plugins.stats  - Get plugin statistics including reload status
 *   plugins.reload - Reload manifests from LittleFS (atomic)
 */
void registerWsPluginCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
