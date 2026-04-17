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
 * @brief Force-abort a stale WebSocket OTA session (watchdog path).
 *
 * Called from WebServer::update() when OtaSessionLock::isStale() fires.
 * Unlike handleOtaClientDisconnect this does not require a client id — the
 * watchdog simply declares the session dead and runs the same cleanup path
 * (telemetry, Update.abort, LED feedback, WS-local state clear, OtaLock
 * release). Safe no-op when no WS session is active (e.g. REST owns the
 * lock, in which case the caller handles REST cleanup separately).
 *
 * @param reason Short diagnostic string for telemetry (e.g. "timeout")
 */
void forceAbortStaleOtaSession(const char* reason);

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
