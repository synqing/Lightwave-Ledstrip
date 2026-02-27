// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsFilesystemCommands.cpp
 * @brief WebSocket filesystem command handlers implementation (stub)
 *
 * Placeholder for filesystem-related WebSocket commands.
 * Currently not implemented - all operations return "not supported".
 */

#include "WsFilesystemCommands.h"
#include "../WebServerContext.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

void registerWsFilesystemCommands(const WebServerContext& ctx) {
    (void)ctx;
    // Stub - filesystem commands not yet implemented
    // When implemented, register commands like:
    // WsCommandRouter::registerCommand("filesystem.list", handleList);
    // WsCommandRouter::registerCommand("filesystem.read", handleRead);
    // WsCommandRouter::registerCommand("filesystem.write", handleWrite);
    // WsCommandRouter::registerCommand("filesystem.delete", handleDelete);
    // WsCommandRouter::registerCommand("filesystem.info", handleInfo);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
