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
#include "../../../plugins/PluginManagerActor.h"
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
    const char* requestId = doc["requestId"] | "";

    if (!ctx.pluginManager) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
            "Plugin manager not available", requestId));
        return;
    }

    const auto& stats = ctx.pluginManager->getStats();

    // Build list of registered effect IDs
    String response = buildWsResponse("plugins.list", requestId,
        [&ctx, &stats](JsonObject& data) {
            data["registeredCount"] = stats.registeredCount;
            data["overrideModeEnabled"] = stats.overrideModeEnabled;

            // List registered effect IDs
            JsonArray effects = data["effects"].to<JsonArray>();
            for (uint8_t i = 0; i < plugins::PluginConfig::MAX_EFFECTS; i++) {
                if (ctx.pluginManager->isEffectRegistered(i)) {
                    effects.add(i);
                }
            }
        });
    client->text(response);
}

// ============================================================================
// Plugin Stats
// ============================================================================

static void handlePluginsStats(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    if (!ctx.pluginManager) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
            "Plugin manager not available", requestId));
        return;
    }

    const auto& stats = ctx.pluginManager->getStats();

    String response = buildWsResponse("plugins.stats", requestId,
        [&stats](JsonObject& data) {
            // Core stats
            data["registeredCount"] = stats.registeredCount;
            data["loadedFromLittleFS"] = stats.loadedFromLittleFS;
            data["overrideModeEnabled"] = stats.overrideModeEnabled;
            data["disabledByOverride"] = stats.disabledByOverride;
            data["registrationsFailed"] = stats.registrationsFailed;
            data["unregistrations"] = stats.unregistrations;

            // Reload status (Phase 2)
            data["lastReloadOk"] = stats.lastReloadOk;
            data["lastReloadMillis"] = stats.lastReloadMillis;
            data["manifestCount"] = stats.manifestCount;
            data["errorCount"] = stats.errorCount;

            // Include error summary if present
            if (stats.lastErrorSummary[0] != '\0') {
                data["lastErrorSummary"] = stats.lastErrorSummary;
            }
        });
    client->text(response);
}

// ============================================================================
// Plugin Reload
// ============================================================================

static void handlePluginsReload(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

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
            data["reloadSuccess"] = success;

            // Stats
            JsonObject statsObj = data["stats"].to<JsonObject>();
            statsObj["registeredCount"] = stats.registeredCount;
            statsObj["loadedFromLittleFS"] = stats.loadedFromLittleFS;
            statsObj["overrideModeEnabled"] = stats.overrideModeEnabled;
            statsObj["disabledByOverride"] = stats.disabledByOverride;
            statsObj["lastReloadOk"] = stats.lastReloadOk;
            statsObj["lastReloadMillis"] = stats.lastReloadMillis;
            statsObj["manifestCount"] = stats.manifestCount;
            statsObj["errorCount"] = stats.errorCount;

            // Errors array (only include manifests with errors)
            JsonArray errors = data["errors"].to<JsonArray>();
            for (uint8_t i = 0; i < manifestCount; i++) {
                if (!manifests[i].valid) {
                    JsonObject errObj = errors.add<JsonObject>();
                    errObj["file"] = manifests[i].filePath;
                    errObj["error"] = manifests[i].errorMsg;
                }
            }
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
