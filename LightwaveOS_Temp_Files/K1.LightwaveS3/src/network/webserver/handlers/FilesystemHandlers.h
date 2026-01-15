/**
 * @file FilesystemHandlers.h
 * @brief Filesystem management HTTP handlers
 *
 * REST API handlers for LittleFS filesystem operations:
 * - Status check (mount state, filesystem info, usage)
 * - Mount/Unmount operations
 * - Restart (unmount and remount)
 */

#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
// Forward declaration
class WebServer;

namespace webserver {
namespace handlers {

/**
 * @brief Filesystem management HTTP handlers
 */
class FilesystemHandlers {
public:
    /**
     * @brief Get filesystem status
     * GET /api/v1/filesystem/status
     *
     * Response:
     * {
     *   "success": true,
     *   "data": {
     *     "mounted": true,
     *     "totalBytes": 1468000,
     *     "usedBytes": 245760,
     *     "freeBytes": 1222240,
     *     "mountTime": 12345
     *   }
     * }
     */
    static void handleFilesystemStatus(AsyncWebServerRequest* request, lightwaveos::network::WebServer* server);

    /**
     * @brief Mount LittleFS
     * POST /api/v1/filesystem/mount
     *
     * Response:
     * {
     *   "success": true,
     *   "data": {
     *     "mounted": true,
     *     "message": "Filesystem mounted successfully"
     *   }
     * }
     */
    static void handleFilesystemMount(AsyncWebServerRequest* request, lightwaveos::network::WebServer* server);

    /**
     * @brief Unmount LittleFS (with safety checks)
     * POST /api/v1/filesystem/unmount
     */
    static void handleFilesystemUnmount(AsyncWebServerRequest* request, lightwaveos::network::WebServer* server);

    /**
     * @brief Restart filesystem (unmount and remount)
     * POST /api/v1/filesystem/restart
     */
    static void handleFilesystemRestart(AsyncWebServerRequest* request, lightwaveos::network::WebServer* server);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
