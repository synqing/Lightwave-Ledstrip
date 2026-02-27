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

/**
 * @brief Check if a WebSocket OTA session is currently active
 *
 * Used by WiFiManager to avoid STA retry during OTA uploads,
 * which would tear down the AP and interrupt the transfer.
 *
 * @return true if a WebSocket OTA upload session is in progress
 */
bool isWsOtaInProgress();

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
