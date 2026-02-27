/**
 * @file FilesystemHandlers.cpp
 * @brief Filesystem management REST API handlers (stub implementation)
 */

#include "FilesystemHandlers.h"
#include "../../ApiResponse.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void FilesystemHandlers::handleFilesystemStatus(AsyncWebServerRequest* request, WebServer* server) {
    (void)server;  // Unused for now

    sendSuccessResponse(request, [](JsonObject& data) {
        data["mounted"] = true;
        data["type"] = "LittleFS";
        data["totalBytes"] = 0;
        data["usedBytes"] = 0;
        data["status"] = "stub";
    });
}

void FilesystemHandlers::handleFilesystemMount(AsyncWebServerRequest* request, WebServer* server) {
    (void)server;

    sendSuccessResponse(request, [](JsonObject& data) {
        data["message"] = "Mount operation not implemented";
        data["status"] = "stub";
    });
}

void FilesystemHandlers::handleFilesystemUnmount(AsyncWebServerRequest* request, WebServer* server) {
    (void)server;

    sendSuccessResponse(request, [](JsonObject& data) {
        data["message"] = "Unmount operation not implemented";
        data["status"] = "stub";
    });
}

void FilesystemHandlers::handleFilesystemRestart(AsyncWebServerRequest* request, WebServer* server) {
    (void)server;

    sendSuccessResponse(request, [](JsonObject& data) {
        data["message"] = "Restart operation not implemented";
        data["status"] = "stub";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
