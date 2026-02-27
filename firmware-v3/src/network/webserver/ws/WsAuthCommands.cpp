// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsAuthCommands.cpp
 * @brief WebSocket API key authentication command handlers implementation
 */

#include "WsAuthCommands.h"

#if FEATURE_WEB_SERVER && FEATURE_API_AUTH

#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"
#include "../../ApiKeyManager.h"
#include <ESPAsyncWebServer.h>

#define LW_LOG_TAG "WsAuth"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

/**
 * @brief Handle auth.status command
 *
 * Returns authentication status. This is a public command - no auth required.
 * Response: {enabled: true, keyConfigured: bool}
 */
static void handleAuthStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    WebServer* server = ctx.webServer;
    if (!server) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Server not available", requestId));
        return;
    }

    ApiKeyManager& keyManager = server->getApiKeyManager();

    String response = buildWsResponse("auth.status", requestId, [&keyManager](JsonObject& data) {
        data["enabled"] = true;  // Auth is enabled (this handler only exists when FEATURE_API_AUTH=1)
        data["keyConfigured"] = keyManager.hasCustomKey();
    });

    LW_LOGI("Auth status requested via WebSocket");
    client->text(response);
}

/**
 * @brief Handle auth.rotate command
 *
 * Generates a new API key. Requires authenticated client.
 * WARNING: The new key is only returned ONCE. Store it securely.
 *
 * Response: {key: "LW-XXXX-XXXX-...", message: "Store this key securely"}
 */
static void handleAuthRotate(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    WebServer* server = ctx.webServer;
    if (!server) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Server not available", requestId));
        return;
    }

    // Check if client is authenticated
    if (!server->isClientAuthenticated(client->id())) {
        LW_LOGW("Unauthenticated client attempted key rotation");
        client->text(buildWsError(ErrorCodes::UNAUTHORIZED, "Authentication required for key rotation", requestId));
        return;
    }

    ApiKeyManager& keyManager = server->getApiKeyManager();

    // Generate new key
    String newKey = keyManager.generateKey();
    if (newKey.isEmpty()) {
        LW_LOGE("Failed to generate new API key");
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to generate new key", requestId));
        return;
    }

    LW_LOGI("API key rotated successfully via WebSocket");

    String response = buildWsResponse("auth.rotate", requestId, [&newKey](JsonObject& data) {
        data["key"] = newKey;
        data["message"] = "Store this key securely. It will not be shown again.";
    });
    client->text(response);
}

void registerWsAuthCommands(const WebServerContext& ctx) {
    (void)ctx;  // Context is captured by handlers via static dispatch
    WsCommandRouter::registerCommand("auth.status", handleAuthStatus);
    WsCommandRouter::registerCommand("auth.rotate", handleAuthRotate);
    LW_LOGI("Registered WebSocket auth commands");
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_API_AUTH
