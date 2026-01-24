/**
 * @file WsPluginCommands.cpp
 * @brief WebSocket plugin command handlers implementation
 *
 * Implements real-time plugin WebSocket commands for:
 * - Plugin list
 * - Plugin statistics with reload status
 * - Manifest reload (atomic)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsPluginCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../plugins/PluginManagerActor.h"  // Include full types before codec header
#include "../../../codec/WsPluginsCodec.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

// ============================================================================
// Forward Declarations
// ============================================================================

static void handlePluginsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
static void handlePluginsStats(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
static void handlePluginsReload(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);

// ============================================================================
// Plugin List
// ============================================================================

static void handlePluginsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::PluginsDecodeResult decodeResult = codec::WsPluginsCodec::decodePluginsList(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::PluginsRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";

    if (!ctx.pluginManager) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
            "Plugin manager not available", requestId));
        return;
    }

    const auto& stats = ctx.pluginManager->getStats();

    // Build list of registered effect IDs
    String response = buildWsResponse("plugins.list", requestId,
        [&ctx, &stats](JsonObject& data) {
            codec::WsPluginsCodec::encodePluginsList(*ctx.pluginManager, stats, data);
        });
    client->text(response);
}

// ============================================================================
// Plugin Stats
// ============================================================================

static void handlePluginsStats(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::PluginsDecodeResult decodeResult = codec::WsPluginsCodec::decodePluginsStats(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::PluginsRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";

    if (!ctx.pluginManager) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
            "Plugin manager not available", requestId));
        return;
    }

    const auto& stats = ctx.pluginManager->getStats();

    String response = buildWsResponse("plugins.stats", requestId,
        [&stats](JsonObject& data) {
            codec::WsPluginsCodec::encodePluginsStats(stats, data);
        });
    client->text(response);
}

// ============================================================================
// Plugin Reload
// ============================================================================

static void handlePluginsReload(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::PluginsDecodeResult decodeResult = codec::WsPluginsCodec::decodePluginsReload(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::PluginsRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";

    if (!ctx.pluginManager) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
            "Plugin manager not available", requestId));
        return;
    }

    // Trigger atomic reload
    bool success = ctx.pluginManager->reloadFromLittleFS();

    const auto& stats = ctx.pluginManager->getStats();
    uint8_t manifestCount = ctx.pluginManager->getManifestCount();
    const auto* manifests = ctx.pluginManager->getManifests();

    String response = buildWsResponse("plugins.reload.result", requestId,
        [success, &stats, manifestCount, manifests](JsonObject& data) {
            codec::WsPluginsCodec::encodePluginsReload(success, stats, manifestCount, manifests, data);
        });
    client->text(response);
}

// ============================================================================
// Registration
// ============================================================================

void registerWsPluginCommands(const WebServerContext& ctx) {
    (void)ctx;

    // Plugin commands
    WsCommandRouter::registerCommand("plugins.list", handlePluginsList);
    WsCommandRouter::registerCommand("plugins.stats", handlePluginsStats);
    WsCommandRouter::registerCommand("plugins.reload", handlePluginsReload);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
