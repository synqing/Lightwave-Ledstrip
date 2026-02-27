// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file FilesystemHandlers.h
 * @brief Filesystem management REST API handlers (stub)
 *
 * Placeholder for filesystem operations - to be implemented.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {

class WebServer;

namespace webserver {
namespace handlers {

/**
 * @brief Filesystem REST API handlers (stub implementation)
 */
class FilesystemHandlers {
public:
    /**
     * @brief Handle GET /api/v1/filesystem/status
     */
    static void handleFilesystemStatus(AsyncWebServerRequest* request, WebServer* server);

    /**
     * @brief Handle POST /api/v1/filesystem/mount
     */
    static void handleFilesystemMount(AsyncWebServerRequest* request, WebServer* server);

    /**
     * @brief Handle POST /api/v1/filesystem/unmount
     */
    static void handleFilesystemUnmount(AsyncWebServerRequest* request, WebServer* server);

    /**
     * @brief Handle POST /api/v1/filesystem/restart
     */
    static void handleFilesystemRestart(AsyncWebServerRequest* request, WebServer* server);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
