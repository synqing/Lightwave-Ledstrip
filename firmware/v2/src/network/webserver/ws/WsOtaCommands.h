/**
 * @file WsOtaCommands.h
 * @brief WebSocket OTA command handlers registration
 */

#pragma once

#include "../WebServerContext.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

/**
 * @brief Register WebSocket OTA command handlers
 */
void registerWsOtaCommands(const WebServerContext& ctx);

/**
 * @brief Handle client disconnect - abort any active OTA session for this client
 * @param clientId WebSocket client ID that disconnected
 */
void handleOtaClientDisconnect(uint32_t clientId);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
