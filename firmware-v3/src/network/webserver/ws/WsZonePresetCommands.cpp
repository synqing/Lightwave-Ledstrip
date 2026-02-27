// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsZonePresetCommands.cpp
 * @brief WebSocket zone preset command handlers implementation (stub)
 */

#include "WsZonePresetCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

void registerWsZonePresetCommands(const WebServerContext& ctx) {
    (void)ctx;
    // Stub - zone preset WebSocket commands not yet implemented
    // Future commands: zonePresets.list, zonePresets.save, zonePresets.load, zonePresets.delete
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
