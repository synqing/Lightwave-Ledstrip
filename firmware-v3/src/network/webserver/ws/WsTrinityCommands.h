// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsTrinityCommands.h
 * @brief WebSocket Trinity command handlers
 *
 * Handles trinity.beat, trinity.macro, and trinity.sync commands
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
