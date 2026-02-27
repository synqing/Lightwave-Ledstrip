// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsFilesystemCommands.h
 * @brief WebSocket filesystem command handlers (stub)
 *
 * Placeholder for filesystem-related WebSocket commands.
 * Currently not implemented - all operations return "not supported".
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../../ApiResponse.h"
#include "../WebServerContext.h"

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief Stub filesystem commands for WebSocket
 */
class WsFilesystemCommands {
public:
    /**
     * @brief Handle filesystem.list command
     */
    static void handleList(AsyncWebSocketClient* client, const JsonObject& params, const char* requestId) {
        String response = buildWsError(ErrorCodes::FEATURE_DISABLED,
                                        "Filesystem commands not implemented",
                                        requestId);
        client->text(response);
    }

    /**
     * @brief Handle filesystem.read command
     */
    static void handleRead(AsyncWebSocketClient* client, const JsonObject& params, const char* requestId) {
        String response = buildWsError(ErrorCodes::FEATURE_DISABLED,
                                        "Filesystem commands not implemented",
                                        requestId);
        client->text(response);
    }

    /**
     * @brief Handle filesystem.write command
     */
    static void handleWrite(AsyncWebSocketClient* client, const JsonObject& params, const char* requestId) {
        String response = buildWsError(ErrorCodes::FEATURE_DISABLED,
                                        "Filesystem commands not implemented",
                                        requestId);
        client->text(response);
    }

    /**
     * @brief Handle filesystem.delete command
     */
    static void handleDelete(AsyncWebSocketClient* client, const JsonObject& params, const char* requestId) {
        String response = buildWsError(ErrorCodes::FEATURE_DISABLED,
                                        "Filesystem commands not implemented",
                                        requestId);
        client->text(response);
    }

    /**
     * @brief Handle filesystem.info command
     */
    static void handleInfo(AsyncWebSocketClient* client, const JsonObject& params, const char* requestId) {
        String response = buildWsError(ErrorCodes::FEATURE_DISABLED,
                                        "Filesystem commands not implemented",
                                        requestId);
        client->text(response);
    }
};

namespace ws {

/**
 * @brief Register filesystem-related WebSocket commands
 * @param ctx WebServer context
 */
void registerWsFilesystemCommands(const webserver::WebServerContext& ctx);

} // namespace ws

} // namespace webserver
} // namespace network
} // namespace lightwaveos
