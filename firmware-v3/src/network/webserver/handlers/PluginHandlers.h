// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file PluginHandlers.h
 * @brief Plugin-related HTTP handlers for the Plugin Subsystem
 *
 * Provides REST API endpoints for:
 * - Plugin list and statistics
 * - Manifest file listing with validation status
 * - Plugin reload from LittleFS
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "../../../config/features.h"

// Forward declarations
namespace lightwaveos {
namespace plugins {
    class PluginManagerActor;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Plugin-related HTTP handlers
 *
 * Endpoints:
 *   GET  /api/v1/plugins           - List loaded plugins with stats
 *   GET  /api/v1/plugins/manifests - List manifest files with validation status
 *   POST /api/v1/plugins/reload    - Reload manifests from LittleFS
 */
class PluginHandlers {
public:
    // ========================================================================
    // Plugin List
    // ========================================================================

    /**
     * @brief Handle GET /api/v1/plugins
     *
     * Returns plugin statistics including:
     * - registeredCount: Number of registered effects
     * - loadedFromLittleFS: Effects loaded from manifests
     * - overrideModeEnabled: Whether override mode is active
     * - disabledByOverride: Count of effects disabled by override
     * - registrationsFailed: Failed registration attempts
     * - lastReloadOk: Whether last reload succeeded
     * - lastReloadMillis: Timestamp of last reload
     * - errorCount: Number of manifests with errors
     */
    static void handleList(AsyncWebServerRequest* request,
                           plugins::PluginManagerActor* pluginMgr);

    // ========================================================================
    // Manifest Files
    // ========================================================================

    /**
     * @brief Handle GET /api/v1/plugins/manifests
     *
     * Returns list of manifest files with validation status:
     * - files: Array of {file, valid, error?, name?, mode?}
     * - count: Number of manifest files
     */
    static void handleManifests(AsyncWebServerRequest* request,
                                plugins::PluginManagerActor* pluginMgr);

    // ========================================================================
    // Reload
    // ========================================================================

    /**
     * @brief Handle POST /api/v1/plugins/reload
     *
     * Triggers atomic reload of plugin manifests from LittleFS.
     * Returns stats and error list.
     *
     * Response includes:
     * - success: Whether reload succeeded
     * - stats: Updated plugin statistics
     * - errors: Array of {file, error} for invalid manifests
     */
    static void handleReload(AsyncWebServerRequest* request,
                             plugins::PluginManagerActor* pluginMgr);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
