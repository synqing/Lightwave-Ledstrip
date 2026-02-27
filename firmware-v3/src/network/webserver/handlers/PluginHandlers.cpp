// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file PluginHandlers.cpp
 * @brief Plugin handlers implementation for REST API
 *
 * Implements handlers for the plugin subsystem REST endpoints:
 * - Plugin list with stats
 * - Manifest file listing with validation status
 * - Plugin reload (atomic)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "PluginHandlers.h"
#include "../../ApiResponse.h"
#include "../../../plugins/PluginManagerActor.h"
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ============================================================================
// Plugin List
// ============================================================================

void PluginHandlers::handleList(AsyncWebServerRequest* request,
                                 plugins::PluginManagerActor* pluginMgr) {
    if (!pluginMgr) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::INTERNAL_ERROR, "Plugin manager not available");
        return;
    }

    const auto& stats = pluginMgr->getStats();

    sendSuccessResponse(request, [&stats](JsonObject& data) {
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
}

// ============================================================================
// Manifest Files
// ============================================================================

void PluginHandlers::handleManifests(AsyncWebServerRequest* request,
                                      plugins::PluginManagerActor* pluginMgr) {
    if (!pluginMgr) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::INTERNAL_ERROR, "Plugin manager not available");
        return;
    }

    uint8_t manifestCount = pluginMgr->getManifestCount();
    const auto* manifests = pluginMgr->getManifests();

    sendSuccessResponse(request, [manifestCount, manifests](JsonObject& data) {
        data["count"] = manifestCount;

        JsonArray files = data["files"].to<JsonArray>();
        for (uint8_t i = 0; i < manifestCount; i++) {
            JsonObject fileObj = files.add<JsonObject>();
            fileObj["file"] = manifests[i].filePath;
            fileObj["valid"] = manifests[i].valid;

            if (manifests[i].valid) {
                fileObj["name"] = manifests[i].pluginName;
                fileObj["mode"] = manifests[i].overrideMode ? "override" : "additive";
                fileObj["effectCount"] = manifests[i].effectCount;
            } else {
                fileObj["error"] = manifests[i].errorMsg;
            }
        }
    });
}

// ============================================================================
// Reload
// ============================================================================

void PluginHandlers::handleReload(AsyncWebServerRequest* request,
                                   plugins::PluginManagerActor* pluginMgr) {
    if (!pluginMgr) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::INTERNAL_ERROR, "Plugin manager not available");
        return;
    }

    // Trigger atomic reload
    bool success = pluginMgr->reloadFromLittleFS();

    const auto& stats = pluginMgr->getStats();
    uint8_t manifestCount = pluginMgr->getManifestCount();
    const auto* manifests = pluginMgr->getManifests();

    sendSuccessResponse(request, [success, &stats, manifestCount, manifests](JsonObject& data) {
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
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
